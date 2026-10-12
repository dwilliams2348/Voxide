@echo off
setlocal enabledelayedexpansion

set CONFIG=Debug
set SANITIZER=none
set DO_TEST=0
set DO_CLEAN=0

:parse
if "%~1"=="" goto done
if /i "%~1"=="--config" ( set CONFIG=%~2& shift & shift & goto parse )
if /i "%~1"=="--sanitizer" ( set SANITIZER=%~2& shift & shift & goto parse )
if /i "%~1"=="--test" ( set DO_TEST=1& shift & goto parse )
if /i "%~1"=="--clean" ( set DO_TEST=1& shift & goto parse )
echo usage: build.bat [--config Debug^|RelDeb^|Release^|Minsize] [--sanitizer none^|asan] [--test] [--clean]
exit /b 1
:done

set ROOT=%~dp0
set BUILD_DIR=%ROOT%bin-int\msvc-%CONFIG%-%SANITIZER%

if %DO_CLEAN%==1 if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%" if exist "bin" rmdir /s /q "bin"

cmake -S "%ROOT%." -B "%BUILD_DIR%" -DSANITIZER=%SANITIZER%
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 exit /b 1

if %DO_TEST%==1 (
    ctest --test-dir "%BUILD_DIR%" -C %CONFIG% --output-on-failure
    if errorlevel 1 exit /b 1
)

exit /b 0
