@echo off
setlocal
rem IsoDeck - one-click Release build. Run from any prompt (or double-click); it locates Visual Studio itself.
cd /d "%~dp0"
set "LOG=%~dp0build.log"
echo IsoDeck build started %DATE% %TIME% > "%LOG%"

if not exist third_party\ventoy.zip (
    echo [!] third_party\ventoy.zip not found - building WITHOUT the boot engine.
    echo     Download ventoy-1.1.xx-windows.zip from https://sourceforge.net/projects/ventoy/files/
    echo     and copy it to third_party\ventoy.zip, then run build.bat again.
    echo NOTE: third_party\ventoy.zip missing - engine not embedded >> "%LOG%"
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio Installer not found. Install Visual Studio with "Desktop development with C++".
    goto :fail
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if "%VSPATH%"=="" (
    echo No Visual Studio installation with the C++ toolset was found. >> "%LOG%"
    echo No Visual Studio installation with the C++ toolset was found.
    goto :fail
)
echo Using Visual Studio at %VSPATH% >> "%LOG%"
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto :fail

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake not found. Install the "C++ CMake tools for Windows" component or CMake from cmake.org.
    goto :fail
)

echo Configuring with Ninja...
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release >> "%LOG%" 2>&1
if errorlevel 1 (
    echo Ninja generator unavailable, falling back to the Visual Studio generator... >> "%LOG%"
    cmake -S . -B build -A x64 >> "%LOG%" 2>&1
    if errorlevel 1 goto :fail
    echo Building Release...
    cmake --build build --config Release >> "%LOG%" 2>&1
    if errorlevel 1 goto :fail
    set "EXE=build\Release\IsoDeck.exe"
    set "ISSDIR=..\build\Release"
    goto :ok
)
echo Building Release...
echo [build step] >> "%LOG%"
cmake --build build >> "%LOG%" 2>&1
echo [build exit code %errorlevel%] >> "%LOG%"
if errorlevel 1 goto :fail
set "EXE=build\IsoDeck.exe"
set "ISSDIR=..\build"

:ok
echo BUILD OK: %EXE% >> "%LOG%"
echo.
echo Build finished: %EXE%
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" (
    echo Building installer...
    "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" /DBuildDir=%ISSDIR% installer\IsoDeck.iss >> "%LOG%" 2>&1
    if errorlevel 1 (echo Installer build failed - see build.log) else (echo Installer: dist\IsoDeck-Setup-1.0.0.exe)
)
echo Full log: build.log
if "%1"=="" pause
exit /b 0

:fail
echo BUILD FAILED >> "%LOG%"
echo.
echo BUILD FAILED - last lines of build.log:
echo ---------------------------------------
powershell -NoProfile -Command "Get-Content -Tail 40 '%LOG%'"
if "%1"=="" pause
exit /b 1
