#requires -Version 7.0
param([ValidateRange(1,8)][int]$Jobs=2)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if(-not $installation){throw 'Visual Studio C++ Build Tools are required.'}
$devcmd=Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$rows=& $env:ComSpec /d /s /c ('call "'+$devcmd+'" -no_logo -arch=x64 -host_arch=x64 >nul && set')
if($LASTEXITCODE){throw 'Visual Studio environment import failed.'}
foreach($row in $rows){$split=$row.IndexOf('=');if($split -gt 0){[Environment]::SetEnvironmentVariable($row.Substring(0,$split),$row.Substring($split+1),'Process')}}
$compiler=Join-Path $installation 'VC/Tools/Llvm/x64/bin/clang-cl.exe'
. (Join-Path $PSScriptRoot 'sonic-ninja-recovery.ps1')
$ninja=Get-SonicBoundNinja -BuildRoot (Join-Path $root 'build-performance')
$build=Join-Path $root 'build-windows-setup'
& cmake -S (Join-Path $root 'cmake/setup') -B $build -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_C_COMPILER=$compiler" "-DCMAKE_MAKE_PROGRAM=$ninja" '-DCMAKE_LINKER_TYPE=LLD'
if($LASTEXITCODE){throw 'Windows setup configure failed.'}
Save-SonicNinjaRecoverySnapshot -BuildRoot $build -Destination (Join-Path $root ('.local/checkpoints/windows-setup-'+[DateTime]::UtcNow.ToString('yyyyMMddHHmmss'))) -Ninja $ninja
& $ninja -C $build -j $Jobs
if($LASTEXITCODE){throw 'Windows setup build failed.'}
