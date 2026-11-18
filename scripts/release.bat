@echo off
rem SMOOTHEQ - doppio clic per pubblicare la versione (default 0.1.1).
rem Per un'altra versione:  release.bat -Version 0.1.2
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0release.ps1" %*
