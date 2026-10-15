@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

echo =======================================================
echo   Building Inkwell Loader Core (MSVC C++20 / x64)
echo =======================================================

:: Dynamic MSBuild discovery via vswhere or standard search
set "MSBUILD_EXE="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
        set "MSBUILD_EXE=%%i"
    )
)

if not defined MSBUILD_EXE (
    if exist "%ProgramFiles%\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD_EXE=%ProgramFiles%\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD_EXE=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD_EXE=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD_EXE=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
)

if not defined MSBUILD_EXE (
    echo [ERROR] MSBuild.exe could not be located on this machine.
    echo Please install Visual Studio with "Desktop development with C++".
    pause
    exit /b 1
)

echo [*] Compiling InkwellLoader.sln using: "!MSBUILD_EXE!"

:: Attempt build with default project toolset
"!MSBUILD_EXE!" "%~dp0InkwellLoader.sln" /p:Configuration=Release /p:Platform=x64 /nologo /v:m
if %ERRORLEVEL% NEQ 0 (
    echo [*] Default toolset unavailable, retrying with standard v143 - VS 2022...
    "!MSBUILD_EXE!" "%~dp0InkwellLoader.sln" /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /nologo /v:m
)

if %ERRORLEVEL% NEQ 0 (
    echo [-] Build Failed with exit code %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo =======================================================
echo [+] Build Complete:
echo     Output: %~dp0bin\Release\InkwellLoader.exe
echo =======================================================

exit /b 0
