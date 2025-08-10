# EarPerkOSC Installer

This directory contains the NSIS installer script and resources for creating a Windows installer for EarPerkOSC.

## Building the Installer

### Automatic (Recommended)
When you build the Release|x64 configuration in Visual Studio, the installer will be automatically built after a successful compilation.

### Manual Build
1. Run `build_installer.bat` from the project root directory
2. Or manually:
   - Build the Release|x64 configuration first
   - Run: `installer\NSIS\makensis.exe installer\app_installer.nsi`

## Output
The installer will be created as `EarPerkOSC v1.0 Setup.exe` in the project root directory.

## What the Installer Does

### Installation
- Installs EarPerkOSC.exe to Program Files
- Installs a default config.ini file
- Creates Start Menu shortcuts
- Creates Desktop shortcut
- Optionally downloads and installs Visual C++ Redistributable if needed
- Registers the application for proper uninstallation

### Uninstallation
- Removes all installed files
- Removes shortcuts
- Cleans up registry entries

## Requirements
- NSIS (Nullsoft Scriptable Install System) is included in the `installer/NSIS/` directory
- Visual Studio 2022 for building the main application
- Internet connection (for downloading Visual C++ Redistributable if needed)

## Customization
To customize the installer, edit `installer/app_installer.nsi`:
- Change version number in the filename and registry entries
- Modify installation directory
- Add/remove files to be installed
- Change publisher information
- Modify UI text and behavior

## Files Included
- `app_installer.nsi` - Main installer script
- `LICENSE.txt` - License file shown during installation
- `NSIS/` - Portable NSIS installation for building installers