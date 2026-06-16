@echo off
setlocal EnableExtensions

if /I "%~1"=="--internal" (
    shift
    goto initialize
)

set "LOG_DIR=%~dp0build\logs"
set "LOG_FILE=%LOG_DIR%\build-windows.log"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
echo Running Windows build. Full log: %LOG_FILE%
call "%~f0" --internal %* > "%LOG_FILE%" 2>&1
set "BUILD_ERROR=%ERRORLEVEL%"
type "%LOG_FILE%"
echo.
echo Full build log: %LOG_FILE%
exit /b %BUILD_ERROR%

:initialize
set "CONFIG=Release"
set "BOOTSTRAP=0"
set "BOOTSTRAP_ONLY=0"
set "CLEAN=0"
set "RUN_TESTS=1"
set "PACKAGE=1"

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="Debug" (
    set "CONFIG=Debug"
    shift
    goto parse_args
)
if /I "%~1"=="Release" (
    set "CONFIG=Release"
    shift
    goto parse_args
)
if /I "%~1"=="--bootstrap" (
    set "BOOTSTRAP=1"
    shift
    goto parse_args
)
if /I "%~1"=="--bootstrap-only" (
    set "BOOTSTRAP=1"
    set "BOOTSTRAP_ONLY=1"
    shift
    goto parse_args
)
if /I "%~1"=="--clean" (
    set "CLEAN=1"
    shift
    goto parse_args
)
if /I "%~1"=="--no-tests" (
    set "RUN_TESTS=0"
    shift
    goto parse_args
)
if /I "%~1"=="--no-package" (
    set "PACKAGE=0"
    shift
    goto parse_args
)
if /I "%~1"=="--help" goto show_help
if /I "%~1"=="-h" goto show_help
echo Error: Unknown argument: %~1
echo.
goto show_help_error

:show_help
echo Usage: build-windows.bat [Debug^|Release] [options]
echo.
echo Options:
echo   --bootstrap       Install missing tools, then build.
echo   --bootstrap-only  Install missing tools without building.
echo   --clean           Remove the Windows build directory before configuring.
echo   --no-tests        Skip automated tests.
echo   --no-package      Skip portable ZIP creation.
echo   --help, -h        Show this help.
echo.
echo Complete output is written to build\logs\build-windows.log.
exit /b 0

:show_help_error
echo Usage: build-windows.bat [Debug^|Release] [--bootstrap] [--clean] [--no-tests] [--no-package]
exit /b 2

:args_done
if "%BOOTSTRAP%"=="1" (
    call :bootstrap_dependencies
    if errorlevel 1 exit /b 1
)
if "%BOOTSTRAP_ONLY%"=="1" (
    echo Dependencies are ready.
    exit /b 0
)

if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
if exist "%ProgramFiles%\Git\cmd\git.exe" set "PATH=%ProgramFiles%\Git\cmd;%PATH%"

where git >nul 2>nul
if errorlevel 1 (
    echo Error: Git is not available in PATH.
    echo CMake requires Git to download project dependencies.
    echo Re-run with --bootstrap to install build dependencies.
    exit /b 2
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: CMake is not available in PATH.
    echo Re-run with --bootstrap to install build dependencies.
    exit /b 2
)

if "%RUN_TESTS%"=="1" (
    where ctest >nul 2>nul
    if errorlevel 1 (
        echo Error: CTest is not available in PATH.
        exit /b 1
    )
)

if "%PACKAGE%"=="1" (
    where certutil >nul 2>nul
    if errorlevel 1 (
        echo Error: certutil is required to generate the package checksum.
        exit /b 1
    )
)

call :find_visual_studio
if errorlevel 1 exit /b 1

set "BUILD_DIR=%~dp0build\windows"
if exist "%BUILD_DIR%\CMakeCache.txt" (
    findstr /I /C:"vcpkg.cmake" "%BUILD_DIR%\CMakeCache.txt" >nul 2>nul
    if not errorlevel 1 (
        echo Removing previous vcpkg-based Windows build...
        cmake -E remove_directory "%BUILD_DIR%"
        if errorlevel 1 exit /b 1
    )
)
if "%CLEAN%"=="1" (
    echo Removing previous Windows build...
    cmake -E remove_directory "%BUILD_DIR%"
    if errorlevel 1 exit /b 1
)

set "BUILD_TESTING=OFF"
if "%RUN_TESTS%"=="1" set "BUILD_TESTING=ON"

echo Configuring %CONFIG% build...
cmake -S "%~dp0." -B "%BUILD_DIR%" ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DBUILD_TESTING=%BUILD_TESTING%
if errorlevel 1 exit /b 1

echo Building %CONFIG%...
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --parallel
if errorlevel 1 exit /b 1

if "%RUN_TESTS%"=="1" (
    echo Running tests...
    ctest --test-dir "%BUILD_DIR%" --build-config "%CONFIG%" --output-on-failure
    if errorlevel 1 exit /b 1
)

if not exist "%BUILD_DIR%\%CONFIG%\pcolonist.exe" (
    echo Error: pcolonist.exe was not produced.
    exit /b 1
)

call :validate_runtime_assets "%BUILD_DIR%\%CONFIG%\assets"
if errorlevel 1 exit /b 1

if "%PACKAGE%"=="0" goto build_complete

set "DIST_DIR=%~dp0dist\windows-x64-%CONFIG%"
set "ARCHIVE=%~dp0dist\pcolonist-windows-x64-%CONFIG%.zip"
echo Creating portable package...
cmake -E remove_directory "%DIST_DIR%"
cmake -E make_directory "%DIST_DIR%"
cmake -E copy_if_different "%BUILD_DIR%\%CONFIG%\pcolonist.exe" "%DIST_DIR%\pcolonist.exe"
if errorlevel 1 exit /b 1
cmake -E copy_directory "%BUILD_DIR%\%CONFIG%\assets" "%DIST_DIR%\assets"
if errorlevel 1 exit /b 1
cmake -E copy_if_different "%~dp0README.md" "%DIST_DIR%\README.md"
if errorlevel 1 exit /b 1
for %%F in ("%BUILD_DIR%\%CONFIG%\*.dll") do if exist "%%~fF" cmake -E copy_if_different "%%~fF" "%DIST_DIR%\%%~nxF"

if exist "%ARCHIVE%" del /q "%ARCHIVE%"
pushd "%DIST_DIR%"
cmake -E tar cf "%ARCHIVE%" --format=zip .
set "PACKAGE_ERROR=%ERRORLEVEL%"
popd
if not "%PACKAGE_ERROR%"=="0" exit /b %PACKAGE_ERROR%
certutil -hashfile "%ARCHIVE%" SHA256 > "%ARCHIVE%.sha256"
if errorlevel 1 exit /b 1

:build_complete
echo.
echo Build completed: %BUILD_DIR%\%CONFIG%\pcolonist.exe
if "%PACKAGE%"=="1" echo Portable package: %ARCHIVE%
if "%PACKAGE%"=="1" echo SHA256 checksum: %ARCHIVE%.sha256
exit /b 0

:validate_runtime_assets
set "ASSET_ROOT=%~1"
echo Validating runtime assets...
call :require_asset "%ASSET_ROOT%\maps\demo_map.obj"
if errorlevel 1 exit /b 1
call :require_asset "%ASSET_ROOT%\scripts\startup.script"
if errorlevel 1 exit /b 1
call :require_asset "%ASSET_ROOT%\scripts\models.scene"
if errorlevel 1 exit /b 1
call :require_asset "%ASSET_ROOT%\shaders\basic.vert"
if errorlevel 1 exit /b 1
call :require_asset "%ASSET_ROOT%\shaders\basic.frag"
if errorlevel 1 exit /b 1
call :require_asset "%ASSET_ROOT%\textures\terrain\earth.png"
if errorlevel 1 exit /b 1
call :require_asset "%ASSET_ROOT%\textures\terrain\sand.png"
if errorlevel 1 exit /b 1
call :require_asset "%ASSET_ROOT%\textures\terrain\basalt.png"
if errorlevel 1 exit /b 1
exit /b 0

:require_asset
if exist "%~1" exit /b 0
echo Error: Required runtime asset is missing: %~1
exit /b 1

:find_visual_studio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Error: Visual Studio Installer was not found.
    echo Re-run with --bootstrap to install Visual Studio Build Tools.
    exit /b 1
)
set "VS_TOOLS="
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_TOOLS=%%I"
if not defined VS_TOOLS (
    echo Error: Visual Studio C++ x64 tools were not found.
    echo Re-run with --bootstrap to install the required workload.
    exit /b 1
)
echo Using Visual Studio: %VS_TOOLS%
exit /b 0

:bootstrap_dependencies
where winget >nul 2>nul
if errorlevel 1 (
    echo Error: winget is required for --bootstrap.
    echo Install App Installer from Microsoft Store, then retry.
    exit /b 1
)

echo Installing Windows build tools when missing...
if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
if exist "%ProgramFiles%\Git\cmd\git.exe" set "PATH=%ProgramFiles%\Git\cmd;%PATH%"
where git >nul 2>nul
if errorlevel 1 (
    winget install --id Git.Git --exact --silent --accept-package-agreements --accept-source-agreements
    if errorlevel 1 exit /b 1
)
where cmake >nul 2>nul
if errorlevel 1 (
    winget install --id Kitware.CMake --exact --silent --accept-package-agreements --accept-source-agreements
    if errorlevel 1 exit /b 1
)
call :find_visual_studio >nul 2>nul
if errorlevel 1 (
    winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --silent ^
        --accept-package-agreements --accept-source-agreements ^
        --override "--wait --quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    if errorlevel 1 exit /b 1
)

if exist "%ProgramFiles%\Git\cmd\git.exe" set "PATH=%ProgramFiles%\Git\cmd;%PATH%"
call :find_visual_studio
if errorlevel 1 exit /b 1
exit /b 0
