# Removes Lorenz for the current user. Conversion engines installed with
# winget are kept; remove them with "winget uninstall" if you no longer need them.
$ErrorActionPreference = 'Stop'
$destination = Join-Path $env:LOCALAPPDATA 'Programs\Lorenz'
$core = Join-Path $destination 'lorenz.exe'
if (Test-Path $core) { & $core autostart disable | Out-Null }
Get-Process lorenz-desktop -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -like "$destination*" } | Stop-Process -Force
foreach ($folder in @([Environment]::GetFolderPath('Programs'), [Environment]::GetFolderPath('SendTo'))) {
    Remove-Item (Join-Path $folder 'Lorenz.lnk') -ErrorAction SilentlyContinue
}
Remove-Item $destination -Recurse -Force -ErrorAction SilentlyContinue
Write-Host 'Lorenz is uninstalled. Your settings remain in %LOCALAPPDATA%\lorenz.'
