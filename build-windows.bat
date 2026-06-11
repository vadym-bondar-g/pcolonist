@echo off
setlocal EnableExtensions

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" (
    echo Usage: build-windows.bat [Debug^|Release]
    exit /b 2
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: CMake is not available in PATH.
    exit /b 1
)

if "%VCPKG_ROOT%"=="" (
    echo Error: VCPKG_ROOT is not set.
    echo Install vcpkg, then set VCPKG_ROOT to its directory.
    exit /b 1
)

if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo Error: vcpkg toolchain was not found under VCPKG_ROOT.
    exit /b 1
)

set "BUILD_DIR=%~dp0build\windows"
cmake -S "%~dp0." -B "%BUILD_DIR%" ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config "%CONFIG%" --parallel
if errorlevel 1 exit /b 1

echo.
echo Build completed: %BUILD_DIR%\%CONFIG%\pcolonist.exe
