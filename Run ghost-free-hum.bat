@echo off
setlocal
set "game=%~dp0..\ghost-free-hum\ghost-free-hum.exe"
set "assets=%~dp0..\assets\glme01.ppak"
if exist "%assets%" (
    start "" "%game%" --assets "%assets%"
) else (
    start "" "%game%"
)
