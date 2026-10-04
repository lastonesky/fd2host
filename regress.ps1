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
# The sandbox lives in port\build\sandbox and is rebuilt from scratch every
# run, so it never touches the real E:\FD2 saves.

param(
    # 60 s: the continue -> save-load -> FD2.TMP path is timing sensitive and
    # 45 s was borderline once other processes (Defender, IDA, an editor) share
    # the machine - it then failed 4 checks although nothing was broken.
    [int] $Seconds = 60,
    [string] $GameDir = "E:\FD2",
    # Which binary to regression-test. build\fd2host.exe is whatever was
    # built last, so pass -Exe to test the other render backend explicitly:
    #   pwsh -File regress.ps1 -Exe build\fd2host_sokol.exe
    [string] $Exe = "E:\FD2\port\build\fd2host.exe"
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
# NOTE: $Exe comes from the -Exe parameter; PowerShell variables are
# case-insensitive, so a second assignment here would clobber it.
$log  = Join-Path $root "build\host.log"
$sb   = Join-Path $root "build\sandbox"

if (-not (Test-Path $Exe)) { throw "$Exe not found - run build.ps1 first" }

# ---- 1. fresh sandbox: data files + FD2.SAV, deliberately NO FD2.TMP -----
if (Test-Path $sb) { Remove-Item -Recurse -Force $sb }
New-Item -ItemType Directory -Force -Path $sb | Out-Null
Get-ChildItem $GameDir -File |
    Where-Object { $_.Name -match '\.(DAT|INI|B24)$' -or $_.Name -eq 'FD2.SAV' } |
    Copy-Item -Destination $sb
if (Test-Path (Join-Path $sb "FD2.TMP")) { Remove-Item (Join-Path $sb "FD2.TMP") }
Write-Host "sandbox: $sb (FD2.TMP removed on purpose)"

# ---- 2. run the host against it -----------------------------------------
if (Test-Path $log) { Remove-Item $log }
$schedule = "5000:SPACE;2500:RETURN;2500:RETURN;2500:DOWN,RETURN"
Start-Process $exe -ArgumentList @(
    "--gamedir=$sb",
    "--exit-after=$Seconds",
    "--autokey=$schedule",
    "--screenshot=$(Join-Path $root 'build\regress.bmp')",
    "--shot-frame=900"
)
Write-Host "running $Seconds s (autokey: $schedule) ..."
Start-Sleep ($Seconds + 15)

if (-not (Test-Path $log)) { Write-Host "FAIL: no host.log"; exit 1 }
$t = Get-Content $log -Raw

# ---- 3. assertions -------------------------------------------------------
$tmp = Join-Path $sb "FD2.TMP"
$checks = @(
    @{ n = "AH=3C create issued";      ok = ($t -match "dos: create 'FD2\.TMP'") }
    @{ n = "reopen after create";      ok = ($t -match "dos: open 'FD2\.TMP' -> (?!FFFFFF)") }
    @{ n = "no unhandled INT21";       ok = ($t -notmatch "UNHANDLED INT21") }
    @{ n = "no cpu crash";             ok = ($t -notmatch "cpu:") }
    @{ n = "no unhandled exception";   ok = ($t -notmatch "unhandled exception") }
    @{ n = "FD2.TMP created";          ok = (Test-Path $tmp) }
    @{ n = "FD2.TMP non-empty";        ok = ((Test-Path $tmp) -and ((Get-Item $tmp).Length -gt 0)) }
    @{ n = "clean end (watchdog/exit)";ok = (($t -match "watchdog fired") -or ($t -match "AH=4Ch terminate")) }
)

$fail = 0
foreach ($c in $checks) {
    $mark = if ($c.ok) { "PASS" } else { "FAIL"; $fail++ }
    "{0}  {1}" -f $mark, $c.n
}
if ((Test-Path $tmp)) { "      FD2.TMP = {0} bytes (original: 207360)" -f (Get-Item $tmp).Length }
if ($fail) {
    Write-Host "`nFAIL: $fail check(s) failed - see $log"
    exit 1
}
Write-Host "`nALL PASS - see $log"
exit 0
