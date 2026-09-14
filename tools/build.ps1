#requires -Version 7.0
[CmdletBinding()]
param([ValidateRange(1,16)][int] $Jobs = 8,
      [string[]] $Target = @('game'),
      [string] $Output = '')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build = Join-Path $root 'build-performance'
. (Join-Path $PSScriptRoot 'sonic-ninja-recovery.ps1')
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) { throw 'Visual Studio C++ Build Tools are required.' }
$devcmd = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$rows = & $env:ComSpec /d /s /c ('call "' + $devcmd + '" -no_logo -arch=x64 -host_arch=x64 >nul && set')
if ($LASTEXITCODE -ne 0) { throw 'Visual Studio environment import failed.' }
foreach ($row in $rows) {
    $split = $row.IndexOf('=')
    if ($split -le 0) { continue }
    $name = $row.Substring(0,$split)
    $value = $row.Substring($split+1)
    if ($name -ieq 'PATH') { $env:PATH = $value }
    else { [Environment]::SetEnvironmentVariable($name,$value,'Process') }
}
$llvm = Join-Path $installation 'VC/Tools/Llvm/x64/bin'
$env:PATH = $llvm + ';' + $env:PATH
$compiler = Join-Path $llvm 'clang-cl.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'The pinned clang-cl toolchain is missing.' }
if (Test-Path -LiteralPath (Join-Path $build 'CMakeCache.txt')) {
    $ninja = Get-SonicBoundNinja -BuildRoot $build
} else {
    $candidate = Get-Command ninja.exe -ErrorAction SilentlyContinue
    if ($candidate) { $ninja = $candidate.Source }
    else {
        $packages = Join-Path $env:LOCALAPPDATA 'Microsoft/WinGet/Packages'
        $ninja = Get-ChildItem -LiteralPath $packages -Directory -Filter 'Ninja-build.Ninja_*' |
            ForEach-Object { Join-Path $_.FullName 'ninja.exe' } |
            Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    }
    if (-not $ninja) { throw 'Install Ninja before building.' }
}
$timer = [Diagnostics.Stopwatch]::StartNew()
$buildStartedUtc = [DateTime]::UtcNow
& python (Join-Path $PSScriptRoot 'prepare-product.py')
if ($LASTEXITCODE -ne 0) { throw 'Sonic working product preparation failed.' }
$outputArguments=@()
if($Output){$outputArguments=@('-DSARECOMP_PRODUCT_OUTPUT='+[IO.Path]::GetFullPath($Output).Replace('\','/'))}
& cmake -S $root -B $build -G Ninja '-DCMAKE_BUILD_TYPE=Release' @outputArguments `
    "-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_MAKE_PROGRAM=$ninja" '-DCMAKE_LINKER_TYPE=LLD'
if ($LASTEXITCODE -ne 0) { throw 'Sonic configure failed.' }
$bound = Get-SonicBoundNinja -BuildRoot $build
if ($bound -ne $ninja) { throw 'Unexpected Ninja binding change.' }
& $bound -C $build -j $Jobs sonic_native_port_manifest sonic_native_provider_refresh
if ($LASTEXITCODE -ne 0) { throw 'Sonic provider contract needs review; manifest build failed.' }
& (Join-Path $build 'sonic_native_port_manifest.exe') `
    (Join-Path $root '.local/working-product/next.katana-native-port') `
    (Join-Path $root '.local/baseline/r354/native-content/boot.bin')
if ($LASTEXITCODE -ne 0) { throw 'Sonic manifest generation failed.' }
& python (Join-Path $PSScriptRoot 'prepare-product.py') --refresh
if ($LASTEXITCODE -ne 0) { throw 'Sonic provider-only refresh failed; AOT was not changed.' }
& $bound -C $build -j $Jobs @Target
if ($LASTEXITCODE -ne 0) { throw 'Sonic build failed.' }
$aotObjects = Join-Path $build 'CMakeFiles'
$aotRecompiles = if (Test-Path -LiteralPath $aotObjects) {
    @(Get-ChildItem -LiteralPath $aotObjects -Recurse -File -Filter 'unit-v*.cpp.obj' |
        Where-Object LastWriteTimeUtc -GE $buildStartedUtc).Count
} else { 0 }
Write-Host "SONIC_BUILD_OK elapsed_ms=$($timer.ElapsedMilliseconds) aot_recompiles=$aotRecompiles"
