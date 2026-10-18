# Removes Metroid for the current user. Conversion engines installed with
# winget are kept; remove them with "winget uninstall" if you no longer need them.
$ErrorActionPreference = 'Stop'
$destination = Join-Path $env:LOCALAPPDATA 'Programs\Metroid'
$core = Join-Path $destination 'metroid.exe'
if (Test-Path $core) { & $core autostart disable | Out-Null }
Get-Process metroid-desktop -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -like "$destination*" } | Stop-Process -Force
foreach ($folder in @([Environment]::GetFolderPath('Programs'), [Environment]::GetFolderPath('SendTo'))) {
    Remove-Item (Join-Path $folder 'Metroid.lnk') -ErrorAction SilentlyContinue
}
Remove-Item $destination -Recurse -Force -ErrorAction SilentlyContinue
Write-Host 'Metroid is uninstalled. Your settings remain in %LOCALAPPDATA%\metroid.'
