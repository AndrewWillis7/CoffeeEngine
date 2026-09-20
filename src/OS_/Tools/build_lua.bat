@echo off
rem Builds src\Core\lua\win64\liblua.a via MSYS2 UCRT64. Safe to double-click
rem or run from any directory -- the script locates the repo root itself.



cd /d "%~dp0"

echo Starting Lua build using MSYS2...

C:\msys64\msys2_shell.cmd -ucrt64 -defterm -no-start -here -c "./winlua.sh"

if errorlevel 1 (
    echo.
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo Build completed successfully.
pause