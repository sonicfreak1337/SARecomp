#requires -Version 7.0
[CmdletBinding()]
param([ValidatePattern('^[a-z0-9-]+$')][string] $Scenario = 'emerald-coast',
      [ValidateSet('original','widescreen')][string] $Aspect = 'widescreen',
      [int] $Width=1280,[int] $Height=720,
      [ValidateRange(10,60)][int] $Seconds=20,
      [ValidatePattern('^[a-zA-Z0-9_-]+$')][string] $Tag='wide-16x9',
      [switch] $CheckOptions,
      [switch] $TraceFade)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$run=Join-Path $root "runs/$Tag"
if (Test-Path -LiteralPath $run) { throw 'Capture already exists; use a new tag.' }
if (Get-Process game -ErrorAction SilentlyContinue) { throw 'A game is already running.' }
[void][IO.Directory]::CreateDirectory($run)
$saves=Join-Path $run 'user-data'
[void][IO.Directory]::CreateDirectory($saves)
foreach ($directory in Get-ChildItem -LiteralPath (Join-Path $root '.local/baseline/r354/saves') -Directory -ErrorAction SilentlyContinue) {
    Copy-Item -LiteralPath $directory.FullName -Destination $saves -Recurse
}
Get-ChildItem -LiteralPath $saves -File -Recurse | ForEach-Object {$_.IsReadOnly=$false}
$display=Join-Path $run 'sonic-display.ini'
"mode=$Aspect`nwidth=$Width`nheight=$Height`nrender_percent=100" | Set-Content -LiteralPath $display -Encoding utf8NoBOM
$envs=@{
    KATANA_PORT_BACKGROUND_TEST='1'; KATANA_PORT_IGNORE_FOCUS='1'
    KATANA_USER_DATA_ROOT=$saves; KATANA_PORT_FINAL_PROGRESS='1'
    KATANA_SONIC_PRIVATE_SCENARIO=$Scenario; KATANA_SONIC_DIAGNOSTIC_MOVIE_SKIP_ONCE='1'
    KATANA_NATIVE_DIAGNOSTIC_TIMEOUT_MS='90000'; KATANA_SONIC_SCENARIO_RUNTIME_MS=[string]($Seconds*1000)
    KATANA_NATIVE_GRAPHICS_CAPTURE_DIRECTORY=(Join-Path $run 'frames')
    KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME='300'; KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME='1500'
    KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL='150'; SARECOMP_DISPLAY_CONFIG=$display
    SARECOMP_OPTIONS_SELF_TEST=$(if ($CheckOptions) {'1'} else {'0'})
}
if ($TraceFade) {
    $envs.KATANA_SONIC_DIAGNOSTIC_UI_PACKET_SEQUENCE='1'
    $envs.KATANA_SONIC_DIAGNOSTIC_UI_PACKET_SEQUENCE_START_FRAME='205'
    $envs.KATANA_SONIC_DIAGNOSTIC_UI_PACKET_SEQUENCE_END_FRAME='210'
}
# Clear only this helper process's environment, never the user's session.
foreach ($entry in @(Get-ChildItem Env: | Where-Object Name -Like 'KATANA_*')) {
    Remove-Item -LiteralPath ('Env:'+$entry.Name)
}
foreach ($key in $envs.Keys) {[Environment]::SetEnvironmentVariable($key,$envs[$key],'Process')}
$product=Join-Path $root 'out/experimental'
$content=Join-Path $root '.local/baseline/r354/native-content'
$process=Start-Process -FilePath (Join-Path $product 'game.exe') -WorkingDirectory $product `
    -ArgumentList @('--bringup-incomplete-hardware-closure','--content-root',('"'+$content+'"'),'--presentation-fps','144') `
    -WindowStyle Hidden -RedirectStandardOutput (Join-Path $run 'stdout.log') `
    -RedirectStandardError (Join-Path $run 'stderr.log') -PassThru
$process.PriorityClass='BelowNormal'
[ordered]@{pid=$process.Id;hidden=$true;muted=$true;scenario=$Scenario;aspect=$Aspect;width=$Width;height=$Height} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'run.json')
Write-Host "SONIC_CAPTURE_STARTED pid=$($process.Id) run=$run hidden=1 muted=1"
