@echo off
setlocal enabledelayedexpansion
echo Building EarPerkOSC Debug version...

REM Try to find MSBuild in common locations
set "MSBUILD="

REM Try VS 2022 Community
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

REM Try VS 2022 Professional
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

REM Try VS 2022 Enterprise
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

REM Try VS 2019 Community
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

echo MSBuild.exe not found! Please install Visual Studio 2019 or 2022.
exit /b 1

:found
echo Using MSBuild: !MSBUILD!

REM Build the Debug version
"!MSBUILD!" EarPerkOSC.sln /p:Configuration=Debug /p:Platform=x64 /m

if %ERRORLEVEL% neq 0 (
    echo Debug build failed!
    exit /b 1
)

echo Debug build successful!