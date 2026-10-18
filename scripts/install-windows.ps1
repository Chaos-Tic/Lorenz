# Builds Metroid and installs it for the current user in %LOCALAPPDATA%\Metroid.
# Requirements: Rust, MSVC C++ build tools, CMake and Qt6 (CMAKE_PREFIX_PATH).
param(
    [switch]$Autostart,
    [switch]$NoAutostart
)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
cargo build --release --locked
if ($LASTEXITCODE) { throw 'Rust build failed' }
cmake -S . -B build
if ($LASTEXITCODE) { throw 'CMake configuration failed. Set CMAKE_PREFIX_PATH to Qt6.' }
cmake --build build --config Release
if ($LASTEXITCODE) { throw 'Qt build failed' }
$destination = Join-Path $env:LOCALAPPDATA 'Metroid'
New-Item -ItemType Directory -Force $destination | Out-Null
Get-Process metroid-desktop -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$destination*" } | Stop-Process
Copy-Item target/release/metroid.exe $destination
Copy-Item build/Release/metroid-desktop.exe $destination
windeployqt --release (Join-Path $destination 'metroid-desktop.exe')
if ($LASTEXITCODE) { throw 'Qt deployment failed' }
$shell = New-Object -ComObject WScript.Shell
foreach ($folder in @([Environment]::GetFolderPath('Programs'), [Environment]::GetFolderPath('SendTo'))) {
    $shortcut = $shell.CreateShortcut((Join-Path $folder 'Metroid.lnk'))
    $shortcut.TargetPath = Join-Path $destination 'metroid-desktop.exe'
    $shortcut.Save()
}
Write-Host 'Metroid is installed in Start and Send to.'

# Start at login: asked unless -Autostart or -NoAutostart is given.
$core = Join-Path $destination 'metroid.exe'
if (-not $Autostart -and -not $NoAutostart) {
    $answer = Read-Host 'Start Metroid automatically when you sign in? [Y/n]'
    $Autostart = $answer -notmatch '^(n|no)$'
}
if ($Autostart) {
    & $core autostart enable
    Start-Process (Join-Path $destination 'metroid-desktop.exe') -ArgumentList '--background'
} else {
    & $core autostart disable | Out-Null
}
Write-Host 'Change keys and startup later from the Metroid tray icon > Settings.'
