@echo off
rem aux_build.bat - cmd wrapper around build.ps1 for sandboxes that block the
rem reg.exe vcvars32.bat launches (the Windows SDK paths it would add are
rem appended here instead, and build.ps1 then skips its own vcvars call).
rem Usage: aux_build.bat <target> [extra build.ps1 args, e.g. -Render sokol]
rem        (omit <target> to build everything; %* is forwarded verbatim)
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 (
  echo vcvars failed
  exit /b 1
)
set "KIT=C:\Program Files (x86)\Windows Kits\10"
set "SDK=10.0.26100.0"
set "INCLUDE=%INCLUDE%;%KIT%\Include\%SDK%\ucrt;%KIT%\Include\%SDK%\um;%KIT%\Include\%SDK%\shared"
set "LIB=%LIB%;%KIT%\Lib\%SDK%\ucrt\x86;%KIT%\Lib\%SDK%\um\x86"
if "%1" == "" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
)
exit /b %ERRORLEVEL%
