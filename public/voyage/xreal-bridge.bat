@echo off
title XREAL head tracking for Leviathan Voyage
rem Runs xreal-bridge.py (keep both files in the same folder). Needs Python 3 (python.org).
where py >nul 2>nul && (py -3 "%~dp0xreal-bridge.py" & goto end)
where python >nul 2>nul && (python "%~dp0xreal-bridge.py" & goto end)
echo Python 3 is not installed. Get it from https://www.python.org/downloads/ (tick "Add to PATH"), then run this again.
:end
pause
