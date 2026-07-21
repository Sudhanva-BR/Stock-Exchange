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
    echo  Please open this project in CLion and build it first:
    echo    Build ^> Build Project  [Ctrl+F9]
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
    echo       CLion MinGW not found at expected path — DLLs may already be present
)

REM ── Launch C++ server in background ──────────────────────────────
echo [3/4] Starting C++ Exchange Server on port 8080...
start "MiniExchangeServer" /min cmd /c "cmake-build-debug\MiniExchangeServer.exe 8080"
timeout /t 2 /nobreak >nul

REM Quick health check
curl -s --max-time 3 http://localhost:8080/api/symbols >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo       Server is UP ^& healthy
) else (
    echo       Server starting... ^(may take a moment^)
)

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
