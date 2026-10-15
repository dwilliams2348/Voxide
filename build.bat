@echo off

set "CONFIG=Debug"
set "COMPILER=clang-cl"
set "SANITIZER=none"
set "DO_TEST=0"
set "DO_CLEAN=0"

:parse
if "%~1"=="" goto done

if /i "%~1"=="--config" (
    if "%~2"=="" goto usage_error
    set "CONFIG=%~2"
    shift
    shift
    goto parse
)

if /i "%~1"=="--compiler" (
    if "%~2"=="" goto usage_error
    set "COMPILER=%~2"
    shift
    shift
    goto parse
)

if /i "%~1"=="--sanitizer" (
    if "%~2"=="" goto usage_error
    set "SANITIZER=%~2"
    shift
    shift
    goto parse
)

if /i "%~1"=="--test" (
    set "DO_TEST=1"
    shift
    goto parse
)

if /i "%~1"=="--clean" (
    set "DO_CLEAN=1"
    shift
    goto parse
)

goto usage_error

:done
if /i not "%COMPILER%"=="clang-cl" (
    echo Error: Windows builds currently support --compiler clang-cl only.
    exit /b 1
)

if /i not "%CONFIG%"=="Debug" if /i not "%CONFIG%"=="Release" if /i not "%CONFIG%"=="RelDeb" if /i not "%CONFIG%"=="MinSize" goto usage_error

if /i not "%SANITIZER%"=="none" if /i not "%SANITIZER%"=="asan" goto usage_error

for %%I in ("%~dp0.") do set "ROOT=%%~fI"
set "BUILD_DIR=%ROOT%\bin-int\%COMPILER%-%CONFIG%-%SANITIZER%"
set "OUTPUT_DIR=%ROOT%\bin\Clang-%CONFIG%-%SANITIZER%"

if "%DO_CLEAN%"=="1" (
    if exist "%ROOT%\bin-int" rmdir /s /q "%ROOT%\bin-int"
    if exist "%ROOT%\bin" rmdir /s /q "%ROOT%\bin"
)

cmake -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja ^
    -DCMAKE_CXX_COMPILER=clang-cl ^
    -DCMAKE_BUILD_TYPE=%CONFIG% ^
    -DSANITIZER=%SANITIZER% ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

if /I "%SANITIZER%"=="asan" (
    set "PATH=C:\Program Files\LLVM\lib\clang\23\lib\windows;%PATH%"
    set "ASAN_SYMBOLIZER_PATH=C:\Program Files\LLVM\bin\llvm-symbolizer.exe"
    if not defined ASAN_OPTIONS set "ASAN_OPTIONS=halt_on_error=1:symbolize=1"
)

if "%DO_TEST%"=="1" (
    ctest --test-dir "%BUILD_DIR%" -C %CONFIG% --output-on-failure
    if errorlevel 1 exit /b 1
)

exit /b 0

:usage_error
echo Usage: build.bat [--config Debug^|Release^|RelDeb^|MinSize]
echo                   [--compiler clang-cl] [--sanitizer none^|asan]
echo                   [--test] [--clean]
exit /b 1