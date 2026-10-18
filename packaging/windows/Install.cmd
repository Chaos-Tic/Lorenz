@echo off
rem Installs Metroid for the current user; no administrator rights are needed
rem except for engines whose installers ask for them.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
pause
