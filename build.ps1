# build.ps1 - build FD2 native port host as a 32-bit Windows executable.
#
# Usage:  pwsh -File build.ps1                 # build all targets
#         pwsh -File build.ps1 -Target probe   # build one target
param(
    [string]$Target = "all",
    [switch]$Debug,
    # Render backend (PROGRESS.md §13): gdi = current StretchDIBits path
    # (reference implementation), sokol = sokol_gfx (step 2).
    [ValidateSet("gdi", "sokol")]
    [string]$Render = "gdi"
)

$ErrorActionPreference = "Stop"

$root    = $PSScriptRoot
$src     = Join-Path $root "src"
$out     = Join-Path $root "build"
$vcvars  = "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars32.bat"

if (-not (Test-Path $vcvars)) { throw "vcvars32.bat not found: $vcvars" }
New-Item -ItemType Directory -Force -Path $out | Out-Null

$cfg  = if ($Debug) { "/Od /Zi /MDd" } else { "/O2 /MD" }
$cflags = "/nologo /W3 /EHsc /GS- /D_CRT_SECURE_NO_WARNINGS $cfg"

$targets = @{
    probe = @{ srcs = @("probe.c"); libs = @(); subsystem = "console" }
    probe2 = @{ srcs = @("probe2.c"); libs = @(); subsystem = "console" }
    probe3 = @{ srcs = @("probe3.c"); libs = @(); subsystem = "console" }
    probe4 = @{ srcs = @("probe4.c"); libs = @(); subsystem = "console" }
    letest = @{ srcs = @("letest.c", "le.c"); libs = @(); subsystem = "console" }
    fd2host = @{ srcs = @("host.c", "main_win32.c", "le.c", "dos.c", "ail.c", "xmidi.c", "synth.c", "dls.c");
                 libs = @("user32.lib", "gdi32.lib", "winmm.lib");
                 subsystem = "windows";
                 # ASLR must stay on (with /DYNAMICBASE:NO Windows reserves the
                 # low 0x10000..0x6FFFF window and the game objects can no
                 # longer be mapped there), but the image should not land near
                 # the addresses the game uses either - a stray value in the
                 # game once turned into the host's own image base and the CPU
                 # jumped into our .text. Push the preferred base far away.
                 link = "/ENTRY:fd2_entry /BASE:0x60000000 /MAP:fd2host.map" }
}

# --- render backend selection (PROGRESS.md §13.1) ---------------------------
# The kernel (host.c) only sees render.h; the entry layer (main_win32.c) sees
# it too. Exactly one backend implementation is compiled in.
$renderSrc = if ($Render -eq "gdi") { "render_gdi.c" } else { "render_sokol.c" }
if (-not (Test-Path (Join-Path $src $renderSrc))) {
    throw "render backend '$Render' not implemented yet: src/$renderSrc is missing (step 2)"
}
if ($targets.ContainsKey("fd2host")) {
    $targets["fd2host"].srcs = $targets["fd2host"].srcs + @($renderSrc)
    $targets["fd2host"].defs = "/D FD2_RENDER_$($Render.ToUpperInvariant())"
}
if ($Target -ne "all") { $targets = @{ $Target = $targets[$Target] } }

foreach ($name in $targets.Keys) {
    $t = $targets[$name]
    $srcFiles = $t.srcs | ForEach-Object {
        if ([System.IO.Path]::IsPathRooted($_)) { $_ } else { Join-Path $src $_ }
    }
    if ($t.ext) {
        $srcFiles = $srcFiles + $t.ext
    }
    $missing = $srcFiles | Where-Object { -not (Test-Path $_) }
    if ($missing) { Write-Host "skip $name (missing sources: $missing)"; continue }

    $sub = if ($t.subsystem -eq "windows") { "/link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup" } else { "/link /SUBSYSTEM:CONSOLE" }
    if ($t.link) { $sub = "$sub $($t.link)" }
    # Keep default ASLR. Disabling it (/DYNAMICBASE:NO) makes Windows reserve
    # the low 0x10000..0x6FFFF window for compatibility, which is exactly the
    # space the DOS/4GW objects must be mapped into. With ASLR on, the image
    # lands far from that window and the reservation succeeds.
    $objdir = Join-Path $out "$name.obj"
    New-Item -ItemType Directory -Force -Path $objdir | Out-Null
    $cmd = "`"$vcvars`" >nul && cl $cflags $($t.defs) /Fe:`"$out\$name.exe`" /Fo:`"$objdir\\`" $($t.inc) $srcFiles $($t.libs -join ' ') $sub"
    Write-Host "== building $name =="
    cmd /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "build failed: $name" }
    Write-Host "   -> $out\$name.exe"
}
