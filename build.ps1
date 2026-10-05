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
    letest = @{ srcs = @("letest.c", "le.c"); libs = @(); subsystem = "console"; link = "/BASE:0x60000000" }
    # differential test: src/game/rle.c (source translation) vs the original
    # machine code at 0x4E98D / 0x4E8D3 - see src/rlecheck.c
    rlecheck = @{ srcs = @("rlecheck.c", "le.c", "game\rle.c"); libs = @(); subsystem = "console"; link = "/BASE:0x60000000" }
    # differential test: src/game/gfx.c (graphics blitter helpers) vs the
    # original machine code at 0x4ECBF/0x4EC7C/0x4ED0B/0x4ED34/0x4ED7A/0x4EEE0
    gfxcheck = @{ srcs = @("gfxcheck.c", "le.c", "game\gfx.c"); libs = @(); subsystem = "console"; link = "/BASE:0x60000000" }
    # differential test: src/game/sprite24.c (24x24 sprite RLE family) vs the
    # original machine code at 0x4DF84/0x4E016/0x4E0A2/0x4E127/0x4E1A6/0x4E22A/0x4E29C
    sprite24check = @{ srcs = @("sprite24check.c", "le.c", "game\sprite24.c"); libs = @(); subsystem = "console"; link = "/BASE:0x60000000" }
    # differential test: src/game/util.c (byte/palette utilities) vs the
    # original machine code at 0x4DED4/0x4DEEC/0x4DF09/0x4DF28/0x4DF4C/0x4E795
    utilcheck = @{ srcs = @("utilcheck.c", "le.c", "game\util.c"); libs = @(); subsystem = "console"; link = "/BASE:0x60000000" }
    # differential test: src/game/path.c (movement range + path trace) vs the
    # original machine code at 0x4E390 / 0x4E4F6
    pathcheck = @{ srcs = @("pathcheck.c", "le.c", "game\path.c"); libs = @(); subsystem = "console"; link = "/BASE:0x60000000" }
    # differential test: src/game/res.c (LMI resource loader) vs 0x111BA,
    # with the game's CRT file/memory entry points redirected to the host libc
    rescheck = @{ srcs = @("rescheck.c", "le.c", "game\res.c"); libs = @(); subsystem = "console"; link = "/BASE:0x60000000" }
    fd2host = @{ srcs = @("host.c", "entry.c", "winshot.c", "le.c", "dos.c", "ail.c", "xmidi.c", "synth.c", "dls.c");
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
# The kernel (host.c) only sees render.h; exactly one entry layer + one
# present backend are compiled in. The entry layer differs too because sokol
# owns the window and drives frames through callbacks instead of a message
# pump (main_win32.c vs main_sokol.c).
$renderSrcs = switch ($Render) {
    "gdi"   { @("main_win32.c", "render_gdi.c") }
    "sokol" { @("main_sokol.c", "render_sokol.c", "sokol_impl.c") }
}
foreach ($f in $renderSrcs) {
    if (-not (Test-Path (Join-Path $src $f))) {
        throw "render backend '$Render' not implemented yet: src/$f is missing"
    }
}
if ($targets.ContainsKey("fd2host")) {
    $targets["fd2host"].srcs = $targets["fd2host"].srcs + $renderSrcs
    if ($Render -eq "sokol") {
        # sokol_gfx: D3D11 on Windows (system built-in, no extra DLL); the
        # HLSL compiler is loaded at runtime by d3dcompiler_47.dll.
        $targets["fd2host"].libs = $targets["fd2host"].libs +
            @("d3d11.lib", "dxgi.lib", "shell32.lib", "ole32.lib")
        $targets["fd2host"].inc  = "/I `"$(Join-Path $root 'vendor\sokol')`""
    }
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
