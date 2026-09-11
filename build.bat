@echo off
setlocal enabledelayedexpansion

REM config
set NAME=fdtd

REM External libraries
set GLFW_DIR=external\glfw
set GLFW_INCLUDE=%GLFW_DIR%\include
set GLFW_LIB=%GLFW_DIR%\lib

REM Collect source files
set SOURCES=

for /r "src" %%f in (*.c) do (
set SOURCES=!SOURCES! "%%f"
)

for /r "common" %%f in (*.c) do (
set SOURCES=!SOURCES! "%%f"
)

for /r "render" %%f in (*.c) do (
set SOURCES=!SOURCES! "%%f"
)



set SOURCES=!SOURCES! "external\glad\glad.c"
set SOURCES=!SOURCES! "main.c"

REM Include paths
set INCLUDES=
set INCLUDES=!INCLUDES! /I"include"
set INCLUDES=!INCLUDES! /I"."
set INCLUDES=!INCLUDES! /I"external\glad\include"
set INCLUDES=!INCLUDES! /I"%GLFW_INCLUDE%"

REM Debug build
if "%~1"=="-d" (
echo.
echo ========================================
echo Building DEBUG
echo ========================================
echo.

cl ^
    /nologo ^
    /std:c11 ^
    /Od ^
    /Zi ^
    /MDd ^
    /Wall ^
    /W4 ^
    /EHsc ^
    /openmp ^
    /D_DEBUG ^
    /D_CRT_SECURE_NO_WARNINGS ^
    !INCLUDES! ^
    /Fe:"%NAME%_db.exe" ^
    !SOURCES! ^
    /link ^
    /LIBPATH:"%GLFW_LIB%" ^
    glfw3.lib ^
    opengl32.lib ^
    user32.lib ^
    gdi32.lib ^
    shell32.lib ^
    advapi32.lib

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

echo.
echo ========================================
echo DEBUG BUILD COMPLETE
echo ========================================
echo Output: %NAME%_db.exe
echo.

exit /b 0


)


REM Release build
echo.
echo ========================================
echo Building RELEASE
echo ========================================
echo.

cl ^
/nologo ^
/std:c11 ^
/O2 ^
/GL ^
/Oi ^
/Ot ^
/fp:fast ^
/Gy ^
/Gw ^
/openmp ^
/MD ^
/DNDEBUG ^
/D_CRT_SECURE_NO_WARNINGS ^
/W4 ^
!INCLUDES! ^
/Fe:"%NAME%.exe" ^
!SOURCES! ^
/link ^
/LTCG ^
/OPT:REF ^
/OPT:ICF ^
/LIBPATH:"%GLFW_LIB%" ^
glfw3.lib ^
opengl32.lib ^
user32.lib ^
gdi32.lib ^
shell32.lib ^
advapi32.lib

if errorlevel 1 (
echo.
echo BUILD FAILED
exit /b 1
)

echo.
echo ========================================
echo RELEASE BUILD COMPLETE
echo ========================================
echo Output: %NAME%.exe
echo.

"%NAME%.exe"

if errorlevel 1 (
echo.
echo PROGRAM EXITED WITH ERROR CODE %ERRORLEVEL%
exit /b %ERRORLEVEL%
)

endlocal