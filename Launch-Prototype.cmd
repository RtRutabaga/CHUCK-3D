@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\Launch-Prototype.ps1"
if errorlevel 1 (
  pause
  exit /b 1
)
