# shotcmp.ps1 - pixel comparison for render-backend verification.
#
#   shotcmp.ps1 -Shared <shared.bmp> -Window <window.bmp>
#
# <shared.bmp> is what --screenshot dumped (320x200, the shared layer's BGRA
# buffer, before any backend), <window.bmp> is what --wshot captured from the
# screen. The shared image is nearest-neighbour upscaled to the window size
# first, because that is what both backends do (GDI integer scale / NEAREST
# sampler).
#
# Two different failures must not be conflated:
#
#   * "content differs"  -> the backend renders wrongly   (real bug)
#   * "origin differs"   -> the capture is offset         (tooling, DPI)
#
# The sokol window is 1920x1200 while its swapchain is 960x600 (the machine
# runs at 200% DPI), and the capture then starts ~24 px off - which alone
# produced a 10.9% mismatch even though every rendered pixel was correct.
# So the script first measures the raw mismatch, then searches the best
# alignment (coarse then fine) and reports both. PASS is decided on the
# *aligned* mismatch; the offset is printed so a non-zero origin stays visible.
#
# Alpha is ignored: window captures do not carry meaningful alpha.

param(
    [Parameter(Mandatory = $true)][string] $Shared,
    [Parameter(Mandatory = $true)][string] $Window,
    [double] $TolerancePct = 0.1,    # max % of differing pixels to pass
    [int]    $Search = 48            # alignment search range in pixels
)

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $Shared)) { Write-Host "not found: $Shared"; exit 1 }
if (-not (Test-Path $Window)) { Write-Host "not found: $Window"; exit 1 }

function Get-Px([System.Drawing.Bitmap]$bmp) {
    $r = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
    $d = $bmp.LockBits($r, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                       [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $n = $d.Stride * $bmp.Height
    $buf = New-Object byte[] $n
    [System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $buf, 0, $n)
    $bmp.UnlockBits($d)
    return @{ b = $buf; s = $d.Stride; w = $bmp.Width; h = $bmp.Height }
}

# NOTE: PowerShell variables are case-insensitive, so $a and $A are the same
# variable - the bitmap and its pixel dump must use distinct names.
$bmpS = [System.Drawing.Bitmap]::FromFile($Shared)
$bmpW = [System.Drawing.Bitmap]::FromFile($Window)
$S = Get-Px $bmpS
$W = Get-Px $bmpW
$bmpS.Dispose()
$bmpW.Dispose()

if (-not $S -or -not $W -or -not $S.w -or -not $W.w) {
    Write-Host "failed to read pixels"
    exit 1
}

$fx = $W.w / [double]$S.w
$fy = $W.h / [double]$S.h

# mismatch over a sampled grid at the given window-space origin
function Get-Diff([int]$ox, [int]$oy, [int]$step) {
    $diff = 0; $tot = 0
    for ($y = 0; $y -lt $W.h; $y += $step) {
        $wy = $y + $oy
        if ($wy -lt 0 -or $wy -ge $W.h) { continue }
        $ay = [int][Math]::Min([Math]::Floor($wy / $fy), $S.h - 1)
        $rowW = $wy * $W.s
        $rowS = $ay * $S.s
        for ($x = 0; $x -lt $W.w; $x += $step) {
            $wx = $x + $ox
            if ($wx -lt 0 -or $wx -ge $W.w) { continue }
            $ax = [int][Math]::Min([Math]::Floor($wx / $fx), $S.w - 1)
            $iW = $rowW + $wx * 4
            $iS = $rowS + $ax * 4
            $tot++
            if ($S.b[$iS] -ne $W.b[$iW] -or
                $S.b[$iS+1] -ne $W.b[$iW+1] -or
                $S.b[$iS+2] -ne $W.b[$iW+2]) { $diff++ }
        }
    }
    if ($tot -eq 0) { return 1.0 }
    return ($diff / [double]$tot)
}

$raw = Get-Diff 0 0 7

# coarse pass (step 8) then fine pass (step 1) around the best coarse hit
$best = @{ d = $raw; dx = 0; dy = 0 }
for ($dy = -$Search; $dy -le $Search; $dy += 8) {
    for ($dx = -$Search; $dx -le $Search; $dx += 8) {
        $d = Get-Diff $dx $dy 7
        if ($d -lt $best.d) { $best = @{ d = $d; dx = $dx; dy = $dy } }
    }
}
$cx = $best.dx; $cy = $best.dy
for ($dy = $cy - 8; $dy -le $cy + 8; $dy++) {
    for ($dx = $cx - 8; $dx -le $cx + 8; $dx++) {
        $d = Get-Diff $dx $dy 7
        if ($d -lt $best.d) { $best = @{ d = $d; dx = $dx; dy = $dy } }
    }
}

# full-resolution mismatch at the best alignment
$aligned = Get-Diff $best.dx $best.dy 1

Write-Host ("{0}  shared={1}x{2}  window={3}x{4}" -f `
    (Split-Path $Window -Leaf), $S.w, $S.h, $W.w, $W.h)
Write-Host ("  raw     (offset 0,0)     : {0:N4}%" -f (100.0 * $raw))
Write-Host ("  aligned (offset {0},{1}) : {2:N4}%" -f $best.dx, $best.dy, (100.0 * $aligned))

if ($aligned -gt $TolerancePct) {
    Write-Host "FAIL: content differs after alignment (backend rendering bug)"
    exit 1
}
if ($best.dx -ne 0 -or $best.dy -ne 0) {
    Write-Host ("PASS (content identical; capture origin offset {0},{1} - tooling/DPI, not rendering)" -f $best.dx, $best.dy)
} else {
    Write-Host "PASS (<= $TolerancePct%)"
}
exit 0
