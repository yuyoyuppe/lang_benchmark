@echo off
setlocal EnableExtensions

REM Only install stuff that you might not have.

where winget >nul 2>nul
if errorlevel 1 (
  echo winget not found. Install App Installer from Microsoft Store.
  exit /b 1
)

set "WINGET_ARGS=install -e --accept-package-agreements --accept-source-agreements"

echo Installing benchmark languages via winget...
echo.

winget %WINGET_ARGS% --id zig.zig
winget %WINGET_ARGS% --id DEVCOM.LuaJIT
winget %WINGET_ARGS% --id Rustlang.Rustup
winget %WINGET_ARGS% --id DenoLand.Deno
winget %WINGET_ARGS% --id odin-lang.Odin

echo.
echo Done.
endlocal
