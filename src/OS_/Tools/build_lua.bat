@echo off

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