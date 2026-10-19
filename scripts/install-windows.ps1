# Installs Metroid for the current user in %LOCALAPPDATA%\Programs\Metroid.
#
# Most people should use Metroid-Setup-x64.exe from the Releases page instead.
# This script builds from a source checkout and needs Rust, MSVC C++ build tools, CMake and Qt 6
# (CMAKE_PREFIX_PATH set to Qt, windeployqt on PATH).
#
# Without options, each step asks first. Options answer in advance:
#   -Engines / -NoEngines     conversion engines through winget
#   -Office / -NoOffice       LibreOffice (Word, PowerPoint, spreadsheets)
#   -Autostart / -NoAutostart start Metroid when you sign in
#   -Yes                      accept every default (engines, no office, autostart)
param(
    [switch]$Engines, [switch]$NoEngines,
    [switch]$Office, [switch]$NoOffice,
    [switch]$Autostart, [switch]$NoAutostart,
    [switch]$Yes
)
$ErrorActionPreference = 'Stop'

function Ask([string]$question, [bool]$default) {
    if ($Yes) { return $default }
    $hint = if ($default) { '[Y/n]' } else { '[y/N]' }
    $answer = Read-Host "$question $hint"
    if ($answer -match '^(y|yes|o|oui)$') { return $true }
    if ($answer -match '^(n|no|non)$') { return $false }
    return $default
}
function Decide([bool]$on, [bool]$off, [string]$question, [bool]$default) {
    if ($on) { return $true }
    if ($off) { return $false }
    return Ask $question $default
}

# 1. Binaries: shipped beside this script, or built from the checkout.
$package = Join-Path $PSScriptRoot 'bin'
if (-not (Test-Path (Join-Path $package 'metroid-desktop.exe'))) {
    $root = Split-Path $PSScriptRoot -Parent
    if (-not (Test-Path (Join-Path $root 'Cargo.toml'))) { throw 'Metroid binaries not found next to this script.' }
    Push-Location $root
    try {
        cargo build --release --locked
        if ($LASTEXITCODE) { throw 'Rust build failed' }
        cmake -S . -B build
        if ($LASTEXITCODE) { throw 'CMake configuration failed. Set CMAKE_PREFIX_PATH to Qt6.' }
        cmake --build build --config Release
        if ($LASTEXITCODE) { throw 'Qt build failed' }
        $package = Join-Path $root 'build/package'
        New-Item -ItemType Directory -Force $package | Out-Null
        Copy-Item target/release/metroid.exe, build/Release/metroid-desktop.exe $package
        windeployqt --release --no-translations (Join-Path $package 'metroid-desktop.exe')
        if ($LASTEXITCODE) { throw 'Qt deployment failed' }
    } finally { Pop-Location }
}

$destination = Join-Path $env:LOCALAPPDATA 'Programs\Metroid'
Get-Process metroid-desktop -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -like "$destination*" } | Stop-Process -Force
New-Item -ItemType Directory -Force $destination | Out-Null
Copy-Item (Join-Path $package '*') $destination -Recurse -Force
# A downloaded package carries the "from the Internet" mark; the user chose to install it.
Get-ChildItem $destination -Recurse -File | Unblock-File
$desktop = Join-Path $destination 'metroid-desktop.exe'
$core = Join-Path $destination 'metroid.exe'

$shell = New-Object -ComObject WScript.Shell
foreach ($folder in @([Environment]::GetFolderPath('Programs'), [Environment]::GetFolderPath('SendTo'))) {
    $shortcut = $shell.CreateShortcut((Join-Path $folder 'Metroid.lnk'))
    $shortcut.TargetPath = $desktop
    $shortcut.WorkingDirectory = $destination
    $shortcut.Save()
}
Write-Host "Metroid is installed in $destination, in Start and in Send to."

# 2. Conversion engines, one at a time: a missing package never stops the rest.
$engineIds = [ordered]@{
    'ImageMagick (images)'      = 'ImageMagick.ImageMagick'
    'FFmpeg (video and audio)'  = 'Gyan.FFmpeg'
    'Pandoc (documents)'        = 'JohnMacFarlane.Pandoc'
    'Poppler (PDF text, pages)' = 'oschwartz10612.Poppler'
    'qpdf (merge, split PDF)'   = 'QPDF.QPDF'
}
$wantEngines = Decide $Engines $NoEngines 'Install the conversion engines (ImageMagick, FFmpeg, Pandoc, Poppler, qpdf) with winget?' $true
$wantOffice = Decide $Office $NoOffice 'Also install LibreOffice for Word, PowerPoint and spreadsheet files (about 350 MB)?' $false
if ($wantOffice) { $engineIds['LibreOffice (office files)'] = 'TheDocumentFoundation.LibreOffice' }
if ($wantEngines -or $wantOffice) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        Write-Warning 'winget is missing. Install "App Installer" from the Microsoft Store, then run this installer again.'
    } else {
        foreach ($entry in $engineIds.GetEnumerator()) {
            if (-not $wantEngines -and $entry.Value -ne 'TheDocumentFoundation.LibreOffice') { continue }
            winget list --id $entry.Value --exact --accept-source-agreements *> $null
            if ($LASTEXITCODE -eq 0) { Write-Host "Already installed: $($entry.Key)"; continue }
            Write-Host "Installing $($entry.Key)..."
            winget install --id $entry.Value --exact --silent --accept-package-agreements --accept-source-agreements
            if ($LASTEXITCODE) { Write-Warning "Could not install $($entry.Key); the related formats stay hidden." }
        }
    }
}
# Engines installed just now are on PATH only for processes started after a refresh.
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
Write-Host 'Archives use the tar included with Windows. PDF compression needs Ghostscript (optional, not on winget).'
& $core doctor

# 3. Start at sign-in.
if (Decide $Autostart $NoAutostart 'Start Metroid automatically when you sign in?' $true) {
    & $core autostart enable
} else {
    & $core autostart disable | Out-Null
}
Start-Process $desktop -ArgumentList '--background'
Write-Host 'Done. Right-click files > Send to > Metroid. Settings: Metroid tray icon > Settings.'
