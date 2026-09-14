#requires -Version 7.0
[CmdletBinding()]
param([ValidatePattern('^[a-z0-9-]+$')][string] $Scenario = 'emerald-coast',
      [ValidateSet('original','widescreen')][string] $Aspect = 'widescreen',
      [int] $Width=1280,[int] $Height=720,
      [ValidateRange(25,100)][int] $RenderPercent=100,
      [ValidateRange(10,60)][int] $Seconds=20,
      [ValidatePattern('^[a-zA-Z0-9_-]+$')][string] $Tag='wide-16x9',
      [string] $Executable='',
      [ValidateSet('d3d11','vulkan')][string] $Renderer='d3d11',
      [ValidateSet('original','recompiled')][string] $GameplayTiming='original',
      [ValidateSet(1,2)][Nullable[int]] $VSync=$null,
      [ValidateSet('original','recompiled')][string] $CameraStyle='original',
      [switch] $CameraTest,
      [switch] $CameraCollisionTest,
      [switch] $CameraTrace,
      [switch] $StartupTrace,
      [string] $CacheRoot='',
      [switch] $DisableCodePrefetch,
      [switch] $DisableStartupCache,
      [switch] $CheckOptions,
      [switch] $IsolatedForward,
      # Motion interpolation was withdrawn; no capture switch may enable it.
      [switch] $TraceFade)
$ErrorActionPreference='Stop'
if ($CameraCollisionTest) {
    if ($Scenario -notin @('emerald-coast','stage-01-0-sonic')) {throw 'The camera wall walk is bound to Emerald Coast Act 1.'}
    if ($CameraStyle -ne 'recompiled') {throw 'The camera wall walk requires Recompiled camera style.'}
    $CameraTest=$true
}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$catalog=(Get-Content -LiteralPath (Join-Path $root 'src/sonic_private_stage_scenarios.inc') -Raw) +
    (Get-Content -LiteralPath (Join-Path $root 'src/sonic_private_scenario_launcher.cpp') -Raw)
$actionRows=[regex]::Match($catalog, '(?s)kGeneratedActionStageScenarios\{\{(.*?)\}\};').Groups[1].Value
$launchable=@([regex]::Matches($actionRows, '\{\s*"([^"]+)"') | ForEach-Object {$_.Groups[1].Value})
$launchable+=@([regex]::Matches($catalog, '(?s)\{\s*\.id\s*=\s*"([^"]+)"(.*?)\}\s*[,}]') |
    Where-Object {$_.Groups[2].Value -match '\.provider_abi_proven\s*=\s*true'} |
    ForEach-Object {$_.Groups[1].Value})
if ($Scenario -notin $launchable) { throw "No launchable scenario '$Scenario'; selector inventory rows are not launch entries." }
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
"setup_complete=1`nmode=$Aspect`nwidth=$Width`nheight=$Height`nrender_percent=$RenderPercent`nrenderer=$Renderer`ngameplay_timing=$([int]($GameplayTiming -eq 'recompiled'))`ncamera_style=$CameraStyle" | Set-Content -LiteralPath $display -Encoding utf8NoBOM
if ($null -ne $VSync) { "vsync=$VSync" | Add-Content -LiteralPath $display -Encoding utf8NoBOM }
$envs=@{
    KATANA_PORT_BACKGROUND_TEST='1'; KATANA_PORT_IGNORE_FOCUS='1'
    KATANA_USER_DATA_ROOT=$saves; KATANA_PORT_FINAL_PROGRESS='1'
    KATANA_SONIC_PRIVATE_SCENARIO=$Scenario; KATANA_SONIC_DIAGNOSTIC_MOVIE_SKIP_ONCE='1'
    KATANA_NATIVE_DIAGNOSTIC_TIMEOUT_MS='90000'; KATANA_SONIC_SCENARIO_RUNTIME_MS=[string]($Seconds*1000)
    KATANA_NATIVE_GRAPHICS_CAPTURE_DIRECTORY=(Join-Path $run 'frames')
    KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME='300'; KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME='1500'
    KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL='150'; SARECOMP_DISPLAY_CONFIG=$display
    SARECOMP_OPTIONS_SELF_TEST=$(if ($CheckOptions) {'1'} else {'0'})
    SARECOMP_CAMERA_TEST=$(if ($CameraTest) {'1'} else {'0'})
    SARECOMP_CAMERA_COLLISION_TEST=$(if ($CameraCollisionTest) {'1'} else {'0'})
    SARECOMP_CAMERA_TRACE=$(if ($CameraTest -or $CameraTrace) {'1'} else {'0'})
    SARECOMP_STARTUP_TRACE=$(if ($StartupTrace) {'1'} else {'0'})
    SARECOMP_DISABLE_CODE_PREFETCH=$(if ($DisableCodePrefetch) {'1'} else {'0'})
    SARECOMP_DISABLE_STARTUP_CACHE=$(if ($DisableStartupCache) {'1'} else {'0'})
    SARECOMP_CACHE_ROOT=$(if ($CacheRoot) {[IO.Path]::GetFullPath($CacheRoot)} else {Join-Path $saves 'cache'})
}
if ($IsolatedForward) {
    $envs.SARECOMP_BENCHMARK_ISOLATED_INPUT='1'
    $envs.KATANA_SONIC_GAMEPLAY_PROBE='1'
    $envs.KATANA_SONIC_GAMEPLAY_INPUT_PROFILE='3'
}
if ($CameraTest -or $CameraTrace) {
    $envs.KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME='950'
    $envs.KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME='12000'
    $envs.KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL='90'
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
$game=if ($Executable) {[IO.Path]::GetFullPath($Executable)} else {Join-Path $product 'game.exe'}
if (-not (Test-Path -LiteralPath $game -PathType Leaf)) { throw 'Capture executable is missing.' }
$content=Join-Path $root '.local/baseline/r354/native-content'
$process=Start-Process -FilePath $game -WorkingDirectory $product `
    -ArgumentList @('--bringup-incomplete-hardware-closure','--content-root',('"'+$content+'"')) `
    -WindowStyle Hidden -RedirectStandardOutput (Join-Path $run 'stdout.log') `
    -RedirectStandardError (Join-Path $run 'stderr.log') -PassThru
$process.PriorityClass='BelowNormal'
[ordered]@{pid=$process.Id;executable=$game;hidden=$true;muted=$true;scenario=$Scenario;aspect=$Aspect;width=$Width;height=$Height;renderer=$Renderer;gameplay_timing=$GameplayTiming;vsync=$VSync;camera_style=$CameraStyle;camera_test=[bool]$CameraTest;camera_collision_test=[bool]$CameraCollisionTest;camera_trace=[bool]$CameraTrace;startup_trace=[bool]$StartupTrace;code_prefetch=(-not $DisableCodePrefetch);startup_cache=(-not $DisableStartupCache);cache_root=$envs.SARECOMP_CACHE_ROOT} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'run.json')
Write-Host "SONIC_CAPTURE_STARTED pid=$($process.Id) run=$run hidden=1 muted=1"
Write-Host "SONIC_CAPTURE_SCALE render_percent=$RenderPercent isolated_forward=$([int][bool]$IsolatedForward)"
