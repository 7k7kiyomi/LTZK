@echo off
setlocal

rem The OSGeo4W Qt package resolves its qt.conf paths from O4W_QT_* variables.
rem Load those variables without losing the Visual Studio compiler environment.
set "LTZK_SAVED_PATH=%PATH%"
set "LTZK_OSGEO4W_ROOT=%~dp0..\..\extern\OSGeo4W_32815"
call "%LTZK_OSGEO4W_ROOT%\bin\o4w_env.bat"
if errorlevel 1 (
    set "LTZK_EXIT_CODE=%ERRORLEVEL%"
    endlocal & exit /b %LTZK_EXIT_CODE%
)
set "PATH=%PATH%;%LTZK_SAVED_PATH%"

"%LTZK_OSGEO4W_ROOT%\apps\Qt5\bin\qmake.exe" %*
set "LTZK_EXIT_CODE=%ERRORLEVEL%"
endlocal & exit /b %LTZK_EXIT_CODE%
