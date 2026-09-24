@echo off
setlocal
set "CHUCK_BUILD=%~dp0Builds\Windows\Chuck3D.exe"
if not exist "%CHUCK_BUILD%" (
  echo The Windows prototype has not been packaged yet.
  echo See docs\PLAYTEST.md for build instructions.
  pause
  exit /b 1
)
start "CHUCK 3D" "%CHUCK_BUILD%" -windowed -ResX=1280 -ResY=720
