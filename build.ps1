# build.ps1 - build the FD2 native loader host as a 32-bit Windows executable.
#
# The host loads the original 32-bit DOS/4GW FD2.EXE into a Win32 process,
# services the DOS/BIOS interfaces the game calls (int 21h/31h/10h/16h, port
# I/O, the VGA frame buffer, ...) and presents the frame buffer through
# sokol_gfx (D3D11 on Windows). Nothing emulates DOS or real mode.
#
# Usage:  pwsh -File build.ps1                 # build every target
#         pwsh -File build.ps1 -Target fd2host # build one target
param(
    [string]$Target = "all",
    [switch]$Debug
)

$ErrorActionPreference = "Stop"

$root    = $PSScriptRoot
$src     = Join-Path $root "src"
$out     = Join-Path $root "build"
$vcvars  = "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars32.bat"

if (-not (Test-Path $vcvars)) { throw "vcvars32.bat not found: $vcvars" }
New-Item -ItemType Directory -Force -Path $out | Out-Null

# Run inside an already-initialised developer command prompt (VSCMD_VER set by
# vcvars32.bat) without calling it again: some sandboxes refuse to start the
# reg.exe it launches, and the environment is correct either way.
$vcprompt = if ($env:VSCMD_VER) { "" } else { "`"$vcvars`" >nul && " }

$cfg  = if ($Debug) { "/Od /Zi /MDd" } else { "/O2 /MD" }
$cflags = "/nologo /W3 /EHsc /GS- /D_CRT_SECURE_NO_WARNINGS $cfg"

$targets = @{
    # loader self-check: maps the LE image and compares it byte-for-byte with
    # reference images when they are available (see src/letest.c)
    letest = @{ srcs = @("letest.c", "le.c"); libs = @(); subsystem = "console"; link = "/BASE:0x60000000" }
    # platform self-test: reserve the guest window and touch every object range
    platprobe = @{ srcs = @("platprobe.c", "le.c"); libs = @(); subsystem = "console"; link = "/BASE:0x60000000" }
    # DOS layer self-check: INT 21h file services, the 0x70000 low-memory
    # mirror, the bios tick thread and a real `int 0x21` serviced by the VEH
    doscheck = @{ srcs = @("doscheck.c", "le.c", "dos.c", "dos_fault_win.c"); libs = @(); subsystem = "console"; link = "/BASE:0x60000000" }
    # portable key table (src/keys.c) pinned against MapVirtualKeyA
    keyscheck = @{ srcs = @("keyscheck.c", "keys.c", "keys_win32.c"); libs = @("user32.lib"); subsystem = "console"; link = "" }
    # the host itself: loader + DOS/BIOS layer + AIL replacement + sokol
    fd2host = @{ srcs = @("host.c", "entry.c", "winshot.c", "le.c", "dos.c", "dos_fault_win.c",
                          "ail.c", "xmidi.c", "synth.c", "dls.c", "audio_sokol.c",
                          "keylog.c", "keys.c", "keys_win32.c",
                          "main_sokol.c", "render_sokol.c", "sokol_impl.c");
                 libs = @("user32.lib", "gdi32.lib", "winmm.lib", "d3d11.lib", "dxgi.lib", "shell32.lib", "ole32.lib");
                 inc  = "/I `"$(Join-Path $root 'vendor\sokol')`"";
                 subsystem = "windows";
                 # ASLR must stay on (with /DYNAMICBASE:NO Windows reserves the
                 # low 0x10000..0x6FFFF window and the game objects can no
                 # longer be mapped there), but the image should not land near
                 # the addresses the game uses either. Push the preferred base
                 # far away.
                 link = "/ENTRY:fd2_entry /BASE:0x60000000 /MAP:fd2host.map" }
}

# Anything that links le.c needs the Windows side of the OS seam too
# (src/platform.h): the loader talks to kernel32 through its own wrappers.
foreach ($k in @($targets.Keys)) {
    if ($targets[$k].srcs -contains "le.c") {
        $targets[$k].srcs = $targets[$k].srcs + @("platform_win32.c")
    }
}

if ($Target -ne "all") { $targets = @{ $Target = $targets[$Target] } }

foreach ($name in $targets.Keys) {
    $t = $targets[$name]
    $srcFiles = $t.srcs | ForEach-Object {
        if ([System.IO.Path]::IsPathRooted($_)) { $_ } else { Join-Path $src $_ }
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
    $cmd = "$vcprompt cl $cflags $($t.defs) /Fe:`"$out\$name.exe`" /Fo:`"$objdir\\`" $($t.inc) $srcFiles $($t.libs -join ' ') $sub"
    Write-Host "== building $name =="
    cmd /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "build failed: $name" }
    Write-Host "   -> $out\$name.exe"
}
