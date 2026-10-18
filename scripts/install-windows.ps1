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
Write-Host 'Metroid installé dans Démarrer et Envoyer vers.'
