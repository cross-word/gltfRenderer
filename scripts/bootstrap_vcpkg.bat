@echo off
setlocal
set VCPKG_DIR=%~dp0..\ .vcpkg
set VCPKG_DIR=%VCPKG_DIR:\ =%
if not exist "%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake" (
  git clone https://github.com/microsoft/vcpkg "%VCPKG_DIR%" || exit /b 1
  call "%VCPKG_DIR%\bootstrap-vcpkg.bat" || exit /b 1
)