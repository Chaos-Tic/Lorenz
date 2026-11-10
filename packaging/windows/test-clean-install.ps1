# Runs inside a clean Windows container: no Visual C++ runtime, no Qt, nobody
# to answer a dialog. That is a new PC, and the machine winget validates
# packages on; the hosted CI runner has all of it preinstalled.
#   docker run --rm -v "<repository>:C:\src" mcr.microsoft.com/windows/servercore:<tag> `
#       powershell -File C:\src\packaging\windows\test-clean-install.ps1 [-Setup PATH]
param([string]$Setup = 'C:\src\dist\Lorenz-Setup-x64.exe')
$ErrorActionPreference = 'Stop'
$name = 'Lorenz'
$program = 'lorenz'
# The loader's own exit codes: a library is missing, is not valid, or misses a function.
$loader = @{ -1073741515 = 'STATUS_DLL_NOT_FOUND'; -1073741701 = 'STATUS_INVALID_IMAGE_FORMAT'; -1073741511 = 'STATUS_ENTRYPOINT_NOT_FOUND' }

function Wait-Program($process, $seconds, $what) {
    $null = $process.Handle   # keeps ExitCode readable after the exit
    if ($process.WaitForExit($seconds * 1000)) { return $true }
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    Write-Host "$what was still running after $seconds seconds"
    return $false
}

$system = Get-ChildItem "$env:windir\System32\vcruntime140*.dll", "$env:windir\System32\msvcp140*.dll" -ErrorAction SilentlyContinue
Write-Host "Visual C++ runtime in System32: $(if ($system) { $system.Name -join ', ' } else { 'none' })"

# As winget runs it: silent, no task, nobody at the screen.
$install = Start-Process $Setup -PassThru -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/TASKS=""', '/LOG=C:\setup.log'
if (-not (Wait-Program $install 300 'the setup')) {
    Get-Content C:\setup.log -Tail 30 -ErrorAction SilentlyContinue
    throw 'the setup did not finish in 5 minutes: it is waiting for something nobody will answer'
}
if ($install.ExitCode) { Get-Content C:\setup.log -Tail 30; throw "the setup failed: $($install.ExitCode)" }

$installed = Join-Path $env:LOCALAPPDATA "Programs\$name"
$cli = Start-Process (Join-Path $installed "$program.exe") -PassThru -NoNewWindow -ArgumentList '--version'
if (-not (Wait-Program $cli 60 "$program.exe")) { throw "$program.exe --version did not finish" }
if ($cli.ExitCode) { throw "$program.exe --version failed: $($cli.ExitCode) $($loader[$cli.ExitCode])" }

# The window cannot be shown here; what matters is that Windows could load
# the program with the libraries shipped next to it.
$window = Start-Process (Join-Path $installed "$program-desktop.exe") -PassThru -ArgumentList '--smoke-test'
if ((Wait-Program $window 30 "$program-desktop.exe") -and $loader.ContainsKey($window.ExitCode)) {
    throw "$program-desktop.exe could not be loaded: $($loader[$window.ExitCode])"
}
Write-Host "$name installs and starts on a clean Windows."
