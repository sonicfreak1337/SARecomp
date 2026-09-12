#requires -Version 7.0
[CmdletBinding()]
param([ValidateSet('d3d11','vulkan')][string]$Renderer='d3d11',
      [ValidateSet('controller','keyboard')][string]$InputMode='controller',
      [ValidateSet('japanese','english','french','spanish','german')][string]$Language='german',
      [ValidatePattern('^[a-z0-9-]+$')][string]$Tag='quit-title')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$run=Join-Path $root "runs/$Tag"
if(Test-Path -LiteralPath $run){throw 'Use a new test tag.'}
if(Get-Process game -ErrorAction SilentlyContinue){throw 'A game is already running; leave it untouched.'}
[void][IO.Directory]::CreateDirectory($run)
$data=Join-Path $run 'user-data';[void][IO.Directory]::CreateDirectory($data)
foreach($folder in Get-ChildItem -LiteralPath (Join-Path $root '.local/baseline/r354/saves') -Directory){
    Copy-Item -LiteralPath $folder.FullName -Destination $data -Recurse
}
Get-ChildItem -LiteralPath $data -File -Recurse | ForEach-Object {$_.IsReadOnly=$false}
$display=Join-Path $run 'sonic-display.ini'
"setup_complete=1`nmode=widescreen`nwidth=1280`nheight=720`nrender_percent=100`nrenderer=$Renderer`npresentation_fps=144`nwindow_mode=windowed`ntext_language=$Language" |
    Set-Content -LiteralPath $display -Encoding utf8NoBOM
foreach($entry in @(Get-ChildItem Env: | Where-Object {$_.Name -like 'KATANA_*' -or $_.Name -like 'SARECOMP_*'})){
    Remove-Item -LiteralPath ('Env:'+$entry.Name)
}
$variables=@{
    KATANA_PORT_BACKGROUND_TEST='1';KATANA_PORT_IGNORE_FOCUS='1';KATANA_PORT_FINAL_PROGRESS='1'
    KATANA_USER_DATA_ROOT=$data;SARECOMP_DISPLAY_CONFIG=$display;SARECOMP_QUIT_TEST=$InputMode
    KATANA_SONIC_DIAGNOSTIC_MOVIE_SKIP_ONCE='1';KATANA_NATIVE_DIAGNOSTIC_TIMEOUT_MS='90000'
    KATANA_NATIVE_GRAPHICS_CAPTURE_DIRECTORY=(Join-Path $run 'frames')
    KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME='1';KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME='6000'
    KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL='15'
}
foreach($key in $variables.Keys){[Environment]::SetEnvironmentVariable($key,$variables[$key],'Process')}
$product=Join-Path $root 'out/experimental';$game=Join-Path $product 'game.exe'
$content=Join-Path $root '.local/baseline/r354/native-content'
$process=Start-Process -FilePath $game -WorkingDirectory $product -WindowStyle Hidden -PassThru `
    -ArgumentList @('--bringup-incomplete-hardware-closure','--content-root',('"'+$content+'"'),'--presentation-fps','144') `
    -RedirectStandardOutput (Join-Path $run 'stdout.log') -RedirectStandardError (Join-Path $run 'stderr.log')
$process.PriorityClass='BelowNormal'
@{pid=$process.Id;renderer=$Renderer;input_mode=$InputMode;language=$Language;hidden=$true;muted=$true;executable=$game} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'run.json')
Write-Host "SONIC_QUIT_TEST_STARTED pid=$($process.Id) run=$run"
$deadline=[DateTime]::UtcNow.AddSeconds(105)
while(-not $process.WaitForExit(1000)){
    if([DateTime]::UtcNow -gt $deadline){throw 'Owned diagnostic did not exit by its runtime deadline.'}
}
$process.WaitForExit()
$log=Get-Content -LiteralPath (Join-Path $run 'stderr.log') -Raw
$output=Get-Content -LiteralPath (Join-Path $run 'stdout.log') -Raw
$passed=$process.ExitCode -eq 0 -and
    [regex]::Matches($log,'SONIC_QUIT opened ').Count -eq 2 -and
    [regex]::Matches($log,'SONIC_QUIT visible ').Count -eq 2 -and
    [regex]::Matches($log,'SONIC_QUIT cancelled ').Count -eq 1 -and
    [regex]::Matches($log,'SONIC_QUIT confirmed ').Count -eq 1 -and
    [regex]::Matches($log,'SONIC_QUIT modal_end guest_instructions_delta=0 guest_frame_delta=0').Count -eq 2 -and
    $log.Contains('KATANA_SESSION_STOP reason=1 ') -and $output.Contains('SARECOMP_USER_EXIT clean=1') -and
    -not $log.Contains('SONIC_QUIT unavailable=')
@{passed=$passed;exit_code=$process.ExitCode;renderer=$Renderer;input_mode=$InputMode;language=$Language} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'result.json')
if(-not $passed){throw "Title quit test failed; inspect $run"}
Write-Host "SONIC_QUIT_INTEGRATION_OK renderer=$Renderer input=$InputMode exit=0 guest_frozen=1"
