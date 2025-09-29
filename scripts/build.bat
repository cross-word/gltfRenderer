@echo off
setlocal
set CFG=%1
if "%CFG%"=="" set CFG=Debug
set GEN="Visual Studio 17 2022"
set ARCH=x64

set VCPKG_LOCAL=%CD%\.vcpkg
if not exist "%VCPKG_LOCAL%\vcpkg.exe" (
  echo [vcpkg] cloning...
  git clone --depth=1 https://github.com/microsoft/vcpkg "%VCPKG_LOCAL%" || exit /b 1
  call "%VCPKG_LOCAL%\bootstrap-vcpkg.bat" || exit /b 1
)

set VCPKG_ROOT=%VCPKG_LOCAL%
set TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
set VCPKG_INSTALLED_DIR=%VCPKG_LOCAL%\installed

if exist "vcpkg_cache\" (
  set VCPKG_BINARY_SOURCES=clear;files,%CD%\vcpkg_cache,readwrite
)

cmake -S . -B build -G %GEN% -A %ARCH% ^
  -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN%" ^
  -DVCPKG_FEATURE_FLAGS=manifests,versions ^
  -DVCPKG_MANIFEST_DIR="%CD%" ^
  -DVCPKG_INSTALLED_DIR="%VCPKG_INSTALLED_DIR%" || exit /b 1

cmake --build build --config %CFG% || exit /b 1
echo.
echo Output: %CD%\build\bin\%CFG%
endlocal