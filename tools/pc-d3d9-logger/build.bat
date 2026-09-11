@echo off
setlocal
cd /d "%~dp0"

cmake -S . -B build -A Win32
if errorlevel 1 exit /b %errorlevel%

cmake --build build --config Release
if errorlevel 1 exit /b %errorlevel%

echo.
echo Built: %CD%\build\Release\d3d9.dll
