@echo off
call "%VS_VCVARS%" x64
if not exist build mkdir build

set "BUILD_ROOT=%~dp0build"
set "EXIV2_PATH=%BUILD_ROOT%\x64-windows"
set "INCLUDE=%EXIV2_PATH%\include;%INCLUDE%"
set "LIB=%EXIV2_PATH%\lib;%LIB%"

pushd src
    cl /std:c++17 /O2 /GL /LD /EHsc exivQ.cpp /I"%EXIV2_PATH%\include" /Fe:"%BUILD_ROOT%\exivQ.dll" /link /LTCG /LIBPATH:"%EXIV2_PATH%\lib" exiv2.lib Ole32.lib 1> "%temp%\error.txt"





REM Check if the command failed
if errorlevel 1 (
    type "%temp%\error.txt" | clip
    del "%temp%\error.txt"
    pause
)

popd

echo.
echo Build complete: %BUILD_ROOT%\exivQ.dll