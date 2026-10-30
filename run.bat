@echo off
REM Builds the C server and starts it (needs gcc / MinGW in PATH)
gcc server.c -o server.exe -lws2_32
if errorlevel 1 (
  echo Build failed. Check that gcc is installed.
  pause
  exit /b 1
)
start "" http://localhost:8080
server.exe
