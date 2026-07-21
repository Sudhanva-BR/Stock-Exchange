@echo off
title Mini Exchange — Startup
color 0A
echo.
echo  ███╗   ███╗██╗███╗   ██╗██╗    ███████╗██╗  ██╗ ██████╗██╗  ██╗ █████╗ ███╗   ██╗ ██████╗ ███████╗
echo  ████╗ ████║██║████╗  ██║██║    ██╔════╝╚██╗██╔╝██╔════╝██║  ██║██╔══██╗████╗  ██║██╔════╝ ██╔════╝
echo  ██╔████╔██║██║██╔██╗ ██║██║    █████╗   ╚███╔╝ ██║     ███████║███████║██╔██╗ ██║██║  ███╗█████╗
echo  ██║╚██╔╝██║██║██║╚██╗██║██║    ██╔══╝   ██╔██╗ ██║     ██╔══██║██╔══██║██║╚██╗██║██║   ██║██╔══╝
echo  ██║ ╚═╝ ██║██║██║ ╚████║██║    ███████╗██╔╝ ██╗╚██████╗██║  ██║██║  ██║██║ ╚████║╚██████╔╝███████╗
echo  ╚═╝     ╚═╝╚═╝╚═╝  ╚═══╝╚═╝    ╚══════╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝ ╚═════╝ ╚══════╝
echo.
echo  C++20 High-Performance Matching Engine — ~1.2M orders/sec, p99 ^< 1.2us
echo  ============================================================================
echo.

REM ── Kill any leftover processes ───────────────────────────────────
echo [1/4] Cleaning up old processes...
taskkill /F /IM MiniExchangeServer.exe >nul 2>&1
timeout /t 1 /nobreak >nul

REM ── Check the server binary ───────────────────────────────────────
if not exist "cmake-build-debug\MiniExchangeServer.exe" (
    echo.
    echo  [ERROR] Server binary not found!
    echo  Expected: cmake-build-debug\MiniExchangeServer.exe
    echo.
    echo  Please build the C++ project first (CMake + MinGW/MSVC).
    echo.
    pause
    exit /b 1
)

REM ── Copy runtime DLLs if needed ───────────────────────────────────
echo [2/4] Checking runtime DLLs...
set CLION_MINGW=%USERPROFILE%\OneDrive\Desktop\CLion 2025.3.1\bin\mingw\bin
if exist "%CLION_MINGW%\libgcc_s_seh-1.dll" (
    copy /Y "%CLION_MINGW%\libgcc_s_seh-1.dll"  "cmake-build-debug\" >nul 2>&1
    copy /Y "%CLION_MINGW%\libstdc++-6.dll"       "cmake-build-debug\" >nul 2>&1
    copy /Y "%CLION_MINGW%\libwinpthread-1.dll"   "cmake-build-debug\" >nul 2>&1
    echo       DLLs OK
) else (
    echo       DLLs already present or CLion not at expected path — skipping copy
)

REM ── Launch C++ server in a VISIBLE new window (keeps running) ─────
echo [3/4] Starting C++ Exchange Server on port 8080...
start "MiniExchangeServer" cmd /k "cd /d "%~dp0" && cmake-build-debug\MiniExchangeServer.exe 8080"
echo       Waiting for server to be ready...
timeout /t 3 /nobreak >nul

REM Health check loop — retry up to 5 times
set /a RETRIES=5
:HEALTH_CHECK
curl -s --max-time 2 http://localhost:8080/api/symbols >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo       Server is UP ^& healthy ^(http://localhost:8080^)
    goto LAUNCH_FRONTEND
)
set /a RETRIES-=1
if %RETRIES% GTR 0 (
    echo       Still starting... ^(%RETRIES% retries left^)
    timeout /t 2 /nobreak >nul
    goto HEALTH_CHECK
)
echo.
echo  [WARNING] Server did not respond in time. Check the server window for errors.
echo  The frontend will still launch — it will reconnect when the server is ready.

:LAUNCH_FRONTEND
REM ── Launch frontend ───────────────────────────────────────────────
echo [4/4] Starting React Frontend...
if not exist "frontend\node_modules" (
    echo       Installing npm dependencies...
    cd frontend && npm install && cd ..
)

echo.
echo  ============================================================================
echo   Mini Exchange is starting!
echo   Open your browser to:  http://localhost:5173
echo.
echo   Tips:
echo   - Click "Sim ON" in the top bar to auto-generate live market orders
echo   - Use Order Entry to submit your own limit/market orders
echo   - Watch the Depth Chart update in real-time
echo   - The WebSocket status shows "Live" when connected
echo  ============================================================================
echo.

start "" "http://localhost:5173"
cd frontend && npm run dev

pause
