@echo off
setlocal enabledelayedexpansion

set CONFIG=RelDeb
set SANITIZER=none
set DO_TEST=0
set DO_CLEAN=0

:parse
if "%~1"=="" goto done
if /i "%~1"=="--config" (
    set CONFIG=%~2
    shift
    shift
    goto parse
)
if /i "%~1"=="--sanitizer" (
    set SANITIZER=%~2
    shift
    shift
    goto parse
)
if /i "%~1"=="--test" (
    set DO_TEST=1
    shift
    goto parse
)
if /i "%~1"=="--clean" (
    set DO_CLEAN=1
    shift
    goto parse
)
echo usage: build.bat [--config Debug^|RelDeb^|Release^|MinSize] [--sanitizer none^|asan] [--test] [--clean]
exit /b 1

:done
set ROOT=%~dp0
set BUILD_DIR=%ROOT%bin-int

if "%DO_CLEAN%"=="1" (
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    if exist "%ROOT%bin" rmdir /s /q "%ROOT%bin"
)

cmake -S "%ROOT%." -B "%BUILD_DIR%" -G Ninja ^
  -DCMAKE_C_COMPILER=clang-cl ^
  -DCMAKE_CXX_COMPILER=clang-cl ^
  -DCMAKE_BUILD_TYPE=%CONFIG% ^
  -DSANITIZER=%SANITIZER% ^
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

if "%DO_TEST%"=="1" (
    ctest --test-dir "%BUILD_DIR%" -C %CONFIG% --output-on-failure
    if errorlevel 1 exit /b 1
)

exit /b 0