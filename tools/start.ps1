#requires -Version 7.0
[CmdletBinding()]
param([ValidateSet('baseline','experimental')][string] $Mode = 'experimental',
      [ValidatePattern('^[a-zA-Z0-9_-]+$')][string] $Profile = 'development')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$base = Join-Path $root '.local/baseline/r354'
$run = Join-Path $root "runs/$Mode-$Profile"
$userData = Join-Path $run 'user-data'
[void][IO.Directory]::CreateDirectory($run)
if (-not (Test-Path -LiteralPath $userData)) {
    [void][IO.Directory]::CreateDirectory($userData)
    foreach ($directory in Get-ChildItem -LiteralPath (Join-Path $base 'saves') -Directory -ErrorAction SilentlyContinue) {
        Copy-Item -LiteralPath $directory.FullName -Destination $userData -Recurse
    }
    Get-ChildItem -LiteralPath $userData -File -Recurse | ForEach-Object { $_.IsReadOnly = $false }
}
if ($Mode -eq 'baseline') {
    # The original host writes a replay beside the EXE even with a separate
    # save root. Run an independent copy, never the immutable snapshot.
    $product = Join-Path $run 'product'
    if (-not (Test-Path -LiteralPath $product)) {
        [void][IO.Directory]::CreateDirectory($product)
        foreach ($file in Get-ChildItem -LiteralPath (Join-Path $base 'product') -File) {
            if ($file.Extension -notin @('.exe','.dll','.txt')) { continue }
            $target = Join-Path $product $file.Name
            [IO.File]::Copy($file.FullName, $target, $false)
            [IO.File]::SetAttributes($target, [IO.FileAttributes]::Normal)
        }
    }
} else { $product = Join-Path $root 'out/experimental' }
$exe = Join-Path $product 'game.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Build the Sonic product first.' }
$start = [Diagnostics.ProcessStartInfo]::new($exe)
$start.UseShellExecute = $false
$start.WorkingDirectory = $product
$start.ArgumentList.Add('--content-root')
$start.ArgumentList.Add((Join-Path $base 'native-content'))
$start.ArgumentList.Add('--presentation-fps')
$start.ArgumentList.Add('144')
# Do not inherit an earlier probe or replay session from another task.
foreach ($key in @($start.Environment.Keys)) {
    if ($key -like 'KATANA_*') { [void]$start.Environment.Remove($key) }
}
$start.Environment['KATANA_USER_DATA_ROOT'] = $userData
$process = [Diagnostics.Process]::Start($start)
Write-Host "SONIC_STARTED mode=$Mode pid=$($process.Id) saves=$userData"
