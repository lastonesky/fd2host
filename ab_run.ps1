param([string]$Rep, [string]$Bmp, [string]$Sb = "E:\FD2\port\build\sandbox")
$tmp = Join-Path $Sb "FD2.TMP"
if (Test-Path $tmp) { Remove-Item $tmp -Force }
if (Test-Path $Bmp) { Remove-Item $Bmp -Force }
$a = @(
    "--replace=$Rep",
    # AGENTS.md §2: destructive tests belong in the sandbox only. Without an
    # explicit --gamedir the host silently falls back to E:\FD2 (host.c:552) and
    # the -WorkingDirectory above is cosmetic - it chdirs to the real game dir.
    # Same form regress.ps1 uses (it leaves --exe at the real, read-only path).
    "--gamedir=$Sb",
    "--exit-after=60",
    "--volume=10",
    "--autokey=5000:SPACE;2500:RETURN;2500:RETURN;2500:DOWN,RETURN",
    "--screenshot=$Bmp",
    "--shot-tick=500",
    "--exit-when-file=${Bmp}:256054"
)
Start-Process "E:\FD2\port\build\fd2host.exe" -ArgumentList $a -WorkingDirectory $Sb -Wait
