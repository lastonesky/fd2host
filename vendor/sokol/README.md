# sokol headers (vendored, pinned)

Source: <https://github.com/floooh/sokol> — commit
`2e75443dbd4940b5aa8d76a8e479f8e4b270b9a3` (fetched 2026-10-05, see `COMMIT`).
License: **zlib** (permissive; redistributing these headers in this repo is fine).

| header | used for |
|---|---|
| `sokol_app.h`   | window + input + main loop (`sokol_main()`, callbacks) |
| `sokol_gfx.h`   | rendering (D3D11 on Windows / Metal on macOS / GL on Linux) |
| `sokol_glue.h`  | `sglue_environment()` / `sglue_swapchain()` glue between the two |
| `sokol_audio.h` | audio stream callback (WASAPI / CoreAudio / ALSA·Pulse) — step 3 |
| `sokol_log.h`   | logging callback expected by the other headers |
| `sokol_time.h`  | frame timing |

**Why pinned**: sokol has no version numbers in the headers, only commits, and
the API moves (the current generation already differs from most tutorials:
`sg_view` / `sg_sampler` / `sg_environment` instead of binding images directly).
Always read the documentation *inside these files* — not blog posts.

**How to update** (deliberately manual, no package manager):

```powershell
# pick a commit, download, replace, re-run the probes + regression
$sha = (Invoke-RestMethod https://api.github.com/repos/floooh/sokol/commits/master).sha
foreach ($h in 'sokol_app','sokol_gfx','sokol_glue','sokol_audio','sokol_log','sokol_time') {
  Invoke-WebRequest "https://raw.githubusercontent.com/floooh/sokol/$sha/$h.h" -OutFile "$h.h"
}
Set-Content COMMIT $sha
```

Then: `pwsh -File build.ps1 -Target fd2host -Render sokol` and
`pwsh -File regress.ps1` must both pass (the GDI backend stays the pixel
reference, so an update that changes rendering shows up immediately).

**Sizing note** (measured 2026-10-05, `build/sokolprobe.c`): a 32-bit exe that
creates a window + D3D11 swapchain through these headers grew by **+146 KB** and
depends only on system DLLs (`d3d11`, `USER32`, `GDI32`, `SHELL32`, `KERNEL32`)
— **no extra DLL in the deliverable**. See `PROGRESS.md` §13.1.
