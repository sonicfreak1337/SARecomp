#requires -Version 7.0
param([string]$Tag='options-native-01',[ValidateRange(15,120)][int]$Seconds=75,
      [ValidateSet('SoundTest','Back')][string]$Destination='SoundTest',
      [ValidateRange(25,100)][int]$RenderPercent=100)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if($Tag -notmatch '^[a-z0-9_-]+$'){throw 'Invalid run name'}
$run=Join-Path $root "runs/$Tag"
if(Test-Path -LiteralPath $run){throw 'Run directory must be fresh'}
if(Get-Process game -ErrorAction SilentlyContinue){throw 'A game is already running; leave it untouched'}
[void][IO.Directory]::CreateDirectory($run)
$data=Join-Path $run 'user-data'
Copy-Item -LiteralPath (Join-Path $root 'runs/legacy-frequency-continued/user-data') -Destination $data -Recurse
$display=Join-Path $run 'sonic-display.ini'
@'
setup_complete=1
mode=original
width=1280
height=720
render_percent=100
renderer=vulkan
presentation_fps=144
window_mode=windowed
text_language=german
'@ | Set-Content -LiteralPath $display -Encoding utf8NoBOM
(Get-Content -LiteralPath $display -Raw).Replace('render_percent=100',"render_percent=$RenderPercent") |
    Set-Content -LiteralPath $display -Encoding utf8NoBOM
$script=Join-Path $run 'menu-input.txt'
@'
0.70 0.10 40
1.00 0.10 13
1.50 0.10 40
2.00 0.10 37
2.50 0.10 27
3.00 0.10 40
3.30 0.10 40
3.60 0.10 40
3.90 0.10 40
4.20 0.10 40
4.50 0.10 40
4.80 0.10 40
5.10 0.10 40
5.25 0.10 40
5.50 0.10 13
6.00 0.10 27
6.50 0.10 38
6.75 0.10 38
7.00 0.10 13
'@ | Set-Content -LiteralPath $script -Encoding utf8NoBOM
if($Destination -eq 'Back'){'0.70 0.10 27' | Set-Content -LiteralPath $script -Encoding utf8NoBOM}
$product=Join-Path $root 'out/experimental'
$start=[Diagnostics.ProcessStartInfo]::new((Join-Path $product 'game.exe'))
$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.WindowStyle='Hidden'
$start.WorkingDirectory=$product;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
$start.ArgumentList.Add('--replay-input');$start.ArgumentList.Add((Join-Path $root '.local/legacy-video-continued.kat1'))
foreach($key in @($start.Environment.Keys)){if($key -like 'KATANA_*' -or $key -like 'SARECOMP_*'){[void]$start.Environment.Remove($key)}}
$values=@{KATANA_PORT_BACKGROUND_TEST='1';KATANA_PORT_IGNORE_FOCUS='1';KATANA_PORT_FINAL_PROGRESS='1'
    KATANA_USER_DATA_ROOT=$data;SARECOMP_DISPLAY_CONFIG=$display;SARECOMP_MENU_TEST_INPUT=$script
    SARECOMP_OPTIONS_TRANSITION_TRACE='1'
    KATANA_NATIVE_DIAGNOSTIC_TIMEOUT_MS=([string]($Seconds*1000))
    KATANA_NATIVE_GRAPHICS_CAPTURE_DIRECTORY=(Join-Path $run 'frames')
    KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME='1200';KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME='6000'
    KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL='100'}
foreach($key in $values.Keys){$start.Environment[$key]=$values[$key]}
$process=[Diagnostics.Process]::Start($start);$process.PriorityClass='BelowNormal'
$out=[IO.File]::Open((Join-Path $run 'stdout.log'),'CreateNew','Write','ReadWrite')
$err=[IO.File]::Open((Join-Path $run 'stderr.log'),'CreateNew','Write','ReadWrite')
$outTask=$process.StandardOutput.BaseStream.CopyToAsync($out);$errTask=$process.StandardError.BaseStream.CopyToAsync($err)
@{pid=$process.Id;hidden=$true;muted=$true;timeout_seconds=$Seconds;executable_sha256=(Get-FileHash -LiteralPath $start.FileName).Hash} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'run.json')
Write-Host "SONIC_OPTIONS_TEST_STARTED pid=$($process.Id)"
while(-not $process.WaitForExit(500)){}
$process.WaitForExit();[void]$outTask.GetAwaiter().GetResult();[void]$errTask.GetAwaiter().GetResult();$out.Dispose();$err.Dispose()
Write-Host "SONIC_OPTIONS_TEST_EXIT code=$($process.ExitCode)"
$log=Get-Content -LiteralPath (Join-Path $run 'stderr.log') -Raw
$expected=if($Destination -eq 'Back'){7}else{5}
$passed=$process.ExitCode -eq 1 -and $log.Contains('KATANA_SESSION_STOP reason=2 ') -and
    $log.Contains('SONIC_OPTIONS opened title=1 ') -and $log.Contains("SONIC_OPTIONS original_destination=$expected") -and
    $log.Contains('SONIC_OPTIONS closed guest_instructions_delta=0 guest_frame_delta=0') -and
    $log.Contains('legacy_display_suppressed=1 phase=1 ') -and
    $log.Contains('legacy_display_suppressed=1 phase=2 ') -and
    $log.Contains('legacy_display_suppressed=1 phase=3 ') -and
    $log -notmatch '(?m)^(KATANA_NATIVE_PORT_FAILURE|KATANA_NATIVE_GRAPHICS_CONTRACT|KATANA_NATIVE_PORT_CONTRACT|SONIC_NATIVE_FRAME_BEGIN failure=|SONIC_OPTIONS operation_error=|SONIC_OPTIONS music_error=)'
@{passed=$passed;exit_code=$process.ExitCode;destination=$Destination;hidden=$true;muted=$true;guest_frozen=$passed} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'result.json')
Select-String -LiteralPath (Join-Path $run 'stderr.log') -Pattern '^SONIC_OPTIONS |^KATANA_SESSION_STOP' | Select-Object -Last 18
if(-not $passed){throw "Options transition test failed; inspect $run"}
Write-Host "SONIC_OPTIONS_TRANSITION_OK destination=$Destination entry_exit_suppressed=1 guest_frozen=1"
