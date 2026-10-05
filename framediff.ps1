# framediff.ps1 - exact pixel comparison of two 320x200 frame dumps.
#
#   pwsh -File framediff.ps1 -A build\ab_n1.bmp -B build\ab_a1.bmp
#
# Used for the repl A/B argument (PROGRESS §26.3): the same fixed
# --shot-frame taken with --replace=none and --replace=all must differ by no
# more than the run-to-run timing baseline (none vs none).
#
# Reports the number/percentage of differing pixels and, when they differ,
# the largest per-channel delta so "same pixels, different palette noise"
# stays distinguishable from a real change.

param(
    [Parameter(Mandatory = $true)][string] $A,
    [Parameter(Mandatory = $true)][string] $B,
    [double] $TolerancePct = 0.5
)

Add-Type -AssemblyName System.Drawing

function Get-Px([string]$path) {
    $bmp = New-Object System.Drawing.Bitmap($path)
    $rect = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
    $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                          [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $n = $data.Stride * $bmp.Height
    $buf = New-Object byte[] $n
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $buf, 0, $n)
    $bmp.UnlockBits($data)
    $bmp.Dispose()
    return , $buf
}

if (-not (Test-Path $A)) { Write-Host "not found: $A"; exit 1 }
if (-not (Test-Path $B)) { Write-Host "not found: $B"; exit 1 }

$pa = Get-Px $A
$pb = Get-Px $B
if ($pa.Length -ne $pb.Length) { Write-Host "size mismatch"; exit 1 }

$diff = 0
$maxd = 0
for ($i = 0; $i -lt $pa.Length; $i += 4) {
    $d = 0
    for ($c = 0; $c -lt 3; $c++) {
        $v = [Math]::Abs($pa[$i + $c] - $pb[$i + $c])
        if ($v -gt $d) { $d = $v }
    }
    if ($d -ne 0) {
        $diff++
        if ($d -gt $maxd) { $maxd = $d }
    }
}
$pct = 100.0 * $diff / ($pa.Length / 4)
Write-Host ("{0} vs {1}: {2} / {3} pixels differ ({4:F4}%), max channel delta {5}" `
    -f $A, $B, $diff, ($pa.Length / 4), $pct, $maxd)
if ($pct -gt $TolerancePct) { exit 1 }
exit 0
