@echo off
echo Building EarPerkOSC Release version...

REM Find MSBuild
for /f "delims=" %%i in ('find_msbuild.bat') do set MSBUILD=%%i

if "%MSBUILD%"=="" (
    echo MSBuild.exe not found! Please install Visual Studio 2019 or 2022.
    pause
    exit /b 1
)

echo Using MSBuild: %MSBUILD%

REM Build the Release version first
%MSBUILD% EarPerkOSC.sln /p:Configuration=Release /p:Platform=x64 /m

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build successful! Building installer...

REM Build the installer
"installer\NSIS\makensis.exe" "installer\app_installer.nsi"

if %ERRORLEVEL% neq 0 (
    echo Installer build failed!
    pause
    exit /b 1
)

echo Installer built successfully: EarPerkOSC v1.0 Setup.exe
pause