# regress.ps1 - platform-layer regression: "file services" round.
#
# Reproduces the crash that a fresh install hit (PROGRESS.md §12):
#   the game does fopen("FD2.TMP","wb") while FD2.TMP does not exist
#   -> Watcom sopen() falls back to INT 21h AH=3C (CREAT)
#   -> the host did not implement it -> fopen returns NULL
#   -> the game dereferences the FILE* -> AV at 0x377B2 reading address 0xC
#
# It builds a throw-away game directory that has every data file *except*
# FD2.TMP, drives the menu with --autokey up to the point where the game
# writes FD2.TMP, and then asserts on host.log + the file system.
#
#   pwsh -File E:\FD2\port\regress.ps1            # build sandbox + run + check
#   pwsh -File E:\FD2\port\regress.ps1 -Seconds 60
#
# Runtime (round 20): the host stops via --exit-when-file the moment FD2.TMP
# is full (+ 2 s settle after the autokey schedule), and this script polls
# for process exit instead of sleeping a fixed amount - a normal run takes
# ~15 s instead of the old 60 s watchdog + 15 s blind sleep. $Seconds stays
# the hard cap for slow machines and for "path never completed" runs.
#
# Retries: if a run fails AND build\host.err shows "guest window blocks",
# the Windows loader happened to occupy the low memory window (0x90000..)
# before fd2_entry could reserve it - an environment flake with a known
# signature (PROGRESS.md §8-48), so the run is repeated with a fresh
# process/ASLR layout. Genuine regressions have no such signature and fail
# on the first attempt.
#
# The sandbox lives in port\build\sandbox and is rebuilt from scratch every
# run, so it never touches the real E:\FD2 saves.

param(
    # Hard cap (was also the de-facto run length before --exit-when-file):
    # the continue -> save-load -> FD2.TMP path is timing sensitive and 45 s
    # was borderline once other processes (Defender, IDA, an editor) share the
    # machine - it then failed 4 checks although nothing was broken. The exit
    # trigger now ends normal runs early, so this only bounds slow/failed ones.
    [int] $Seconds = 60,
    [string] $GameDir = "E:\FD2",
    # Which binary to regression-test. build\fd2host.exe is whatever was
    # built last, so pass -Exe to test the other render backend explicitly:
    #   pwsh -File regress.ps1 -Exe build\fd2host_sokol.exe
    [string] $Exe = "E:\FD2\port\build\fd2host.exe",
    # A/B: "" = host default (all translations installed), "none" = original
    # machine code only, or a group list (rle,gfx,sprite24,util,path).
    [string] $Replace = ""
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
# NOTE: $Exe comes from the -Exe parameter; PowerShell variables are
# case-insensitive, so a second assignment here would clobber it.
$log  = Join-Path $root "build\host.log"
$err  = Join-Path $root "build\host.err"
$sb   = Join-Path $root "build\sandbox"
$tmp  = Join-Path $sb "FD2.TMP"
$shot = Join-Path $root "build\regress.bmp"

if (-not (Test-Path $Exe)) { throw "$Exe not found - run build.ps1 first" }

# ---- 1. fresh sandbox: data files + FD2.SAV, deliberately NO FD2.TMP -----
if (Test-Path $sb) { Remove-Item -Recurse -Force $sb }
New-Item -ItemType Directory -Force -Path $sb | Out-Null
Get-ChildItem $GameDir -File |
    Where-Object { $_.Name -match '\.(DAT|INI|B24)$' -or $_.Name -eq 'FD2.SAV' } |
    Copy-Item -Destination $sb
if (Test-Path $tmp) { Remove-Item $tmp }
Write-Host "sandbox: $sb (FD2.TMP removed on purpose)"

# ---- 2. run + assert (retry on environment-caused boot failures) ---------
$schedule = "5000:SPACE;2500:RETURN;2500:RETURN;2500:DOWN,RETURN"
$maxAttempts = 3
$fail = 0

for ($attempt = 1; $attempt -le $maxAttempts; $attempt++) {
    foreach ($f in @($log, $err, $shot, $tmp)) {
        if (Test-Path $f) { Remove-Item $f }
    }

    # ${...}: braces keep PowerShell from getting confused by the ':size'
    # suffix of --exit-when-file. The regular --shot-frame stays as a
    # fallback; with the early exit the host dumps its last frame instead.
    $argList = @(
        "--gamedir=$sb",
        "--exit-after=$Seconds",
        "--exit-when-file=${sb}\FD2.TMP:207360",
        "--autokey=$schedule",
        "--screenshot=$shot",
        "--shot-frame=900"
    )
    if ($Replace) { $argList += "--replace=$Replace" }
    $proc = Start-Process $Exe -ArgumentList $argList -PassThru
    Write-Host "attempt $attempt/$maxAttempts (autokey: $schedule; exit when FD2.TMP=207360, cap ${Seconds}s) ..."

    # Poll for the host's own exit instead of Start-Sleep ($Seconds + 15):
    # the old flat sleep idled ~15 s after the process was already gone.
    $deadline = (Get-Date).AddSeconds($Seconds + 15)
    while (-not $proc.HasExited -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 250
        $proc.Refresh()
    }
    if (-not $proc.HasExited) {
        Write-Host "WARN: host still running after $($Seconds + 15) s - stopping it"
        Stop-Process -Id $proc.Id -Force
    }
    Write-Host ("  ran {0:N1} s" -f ((Get-Date) - $proc.StartTime).TotalSeconds)

    if (-not (Test-Path $log)) { Write-Host "FAIL: no host.log"; exit 1 }
    $t = Get-Content $log -Raw

    # ---- assertions ------------------------------------------------------
    $checks = @(
        @{ n = "AH=3C create issued";      ok = ($t -match "dos: create 'FD2\.TMP'") }
        @{ n = "reopen after create";      ok = ($t -match "dos: open 'FD2\.TMP' -> (?!FFFFFF)") }
        @{ n = "no unhandled INT21";       ok = ($t -notmatch "UNHANDLED INT21") }
        @{ n = "no cpu crash";             ok = ($t -notmatch "cpu:") }
        @{ n = "no unhandled exception";   ok = ($t -notmatch "unhandled exception") }
        @{ n = "FD2.TMP created";          ok = (Test-Path $tmp) }
        @{ n = "FD2.TMP non-empty";        ok = ((Test-Path $tmp) -and ((Get-Item $tmp).Length -gt 0)) }
        @{ n = "clean end (watchdog/exit)";ok = (($t -match "watchdog fired") -or
                                                 ($t -match "AH=4Ch terminate") -or
                                                 ($t -match "exit condition met")) }
    )

    $fail = 0
    foreach ($c in $checks) {
        $mark = if ($c.ok) { "PASS" } else { "FAIL"; $fail++ }
        "{0}  {1}" -f $mark, $c.n
    }
    if ($fail -eq 0) { break }

    # Environment signature: the loader occupied the low window before
    # fd2_entry - rerunning gets a fresh layout. Anything else is a real
    # failure and must not be retried away.
    $envBad = (Test-Path $err) -and ((Get-Content $err -Raw) -match "guest window blocks")
    if (-not $envBad) { break }
    if ($attempt -lt $maxAttempts) {
        Write-Host "  retry: low memory window occupied by the loader - rerunning"
    }
}

if ((Test-Path $tmp)) { "      FD2.TMP = {0} bytes (original: 207360)" -f (Get-Item $tmp).Length }
if ($fail) {
    Write-Host "`nFAIL: $fail check(s) failed after $attempt attempt(s) - see $log"
    exit 1
}
Write-Host "`nALL PASS - see $log"
exit 0
