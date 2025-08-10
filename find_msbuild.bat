@echo off
setlocal enabledelayedexpansion
REM Find MSBuild.exe in various Visual Studio installations

REM Try VS 2022 Community
set "MSBUILD_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if exist "!MSBUILD_PATH!" (
    echo "!MSBUILD_PATH!"
    exit /b 0
)

REM Try VS 2022 Professional
set "MSBUILD_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
if exist "!MSBUILD_PATH!" (
    echo "!MSBUILD_PATH!"
    exit /b 0
)

REM Try VS 2022 Enterprise
set "MSBUILD_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if exist "!MSBUILD_PATH!" (
    echo "!MSBUILD_PATH!"
    exit /b 0
)

REM Try VS 2019 Community
set "MSBUILD_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
if exist "!MSBUILD_PATH!" (
    echo "!MSBUILD_PATH!"
    exit /b 0
)

REM Try VS 2019 Professional
set "MSBUILD_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe"
if exist "!MSBUILD_PATH!" (
    echo "!MSBUILD_PATH!"
    exit /b 0
)

REM Try Build Tools
set "MSBUILD_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
if exist "!MSBUILD_PATH!" (
    echo "!MSBUILD_PATH!"
    exit /b 0
)

echo MSBuild.exe not found
exit /b 1