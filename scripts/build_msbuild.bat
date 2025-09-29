@echo off
setlocal
call "%~dp0bootstrap_vcpkg.bat" || exit /b 1
REM Visual Studio DevCmd
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
msbuild "%~dp0..\MyApp.sln" /m /p:Configuration=Release || exit /b 1
