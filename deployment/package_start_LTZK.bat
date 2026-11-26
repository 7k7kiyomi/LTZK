@echo off
setlocal

set "APP_DIR=%~dp0"

if not exist "%APP_DIR%bin\LTZK.exe" (
    echo LTZK.exe not found: "%APP_DIR%bin\LTZK.exe"
    pause
    exit /b 1
)

if not exist "%APP_DIR%.env" (
    echo .env not found: "%APP_DIR%.env"
    echo Please edit .env and set QGIS_PREFIX_PATH and OSGEO4W_ROOT for this computer.
    pause
    exit /b 1
)

for /f "usebackq eol=# tokens=1,* delims==" %%A in ("%APP_DIR%.env") do (
    if not "%%~A"=="" set "%%~A=%%~B"
)

if not defined QGIS_PREFIX_PATH (
    echo QGIS_PREFIX_PATH is not configured in "%APP_DIR%.env"
    pause
    exit /b 1
)

if not defined OSGEO4W_ROOT (
    echo OSGEO4W_ROOT is not configured in "%APP_DIR%.env"
    pause
    exit /b 1
)

set "QGIS_PREFIX_PATH=%QGIS_PREFIX_PATH:"=%"
set "OSGEO4W_ROOT=%OSGEO4W_ROOT:"=%"

if not "%QGIS_PREFIX_PATH:~1,1%"==":" if not "%QGIS_PREFIX_PATH:~0,2%"=="\\" set "QGIS_PREFIX_PATH=%APP_DIR%%QGIS_PREFIX_PATH%"
if not "%OSGEO4W_ROOT:~1,1%"==":" if not "%OSGEO4W_ROOT:~0,2%"=="\\" set "OSGEO4W_ROOT=%APP_DIR%%OSGEO4W_ROOT%"

for %%I in ("%QGIS_PREFIX_PATH%") do set "QGIS_PREFIX_PATH=%%~fI"
for %%I in ("%OSGEO4W_ROOT%") do set "OSGEO4W_ROOT=%%~fI"

if not exist "%QGIS_PREFIX_PATH%" (
    echo QGIS prefix path not found: "%QGIS_PREFIX_PATH%"
    pause
    exit /b 1
)

if not exist "%OSGEO4W_ROOT%" (
    echo OSGeo4W root not found: "%OSGEO4W_ROOT%"
    pause
    exit /b 1
)

if not exist "%QGIS_PREFIX_PATH%\bin" (
    echo QGIS bin folder not found: "%QGIS_PREFIX_PATH%\bin"
    pause
    exit /b 1
)

if not exist "%OSGEO4W_ROOT%\bin" (
    echo OSGeo4W bin folder not found: "%OSGEO4W_ROOT%\bin"
    pause
    exit /b 1
)

if not exist "%OSGEO4W_ROOT%\apps\Qt5\bin" (
    echo Qt bin folder not found: "%OSGEO4W_ROOT%\apps\Qt5\bin"
    pause
    exit /b 1
)

if not exist "%OSGEO4W_ROOT%\apps\Qt5\plugins\platforms" (
    echo Qt platform plugins folder not found: "%OSGEO4W_ROOT%\apps\Qt5\plugins\platforms"
    pause
    exit /b 1
)

set "PATH=%APP_DIR%bin;%QGIS_PREFIX_PATH%\bin;%OSGEO4W_ROOT%\bin;%OSGEO4W_ROOT%\apps\Qt5\bin;%PATH%"
set "QT_PLUGIN_PATH=%OSGEO4W_ROOT%\apps\Qt5\plugins"
set "QT_QPA_PLATFORM_PLUGIN_PATH=%OSGEO4W_ROOT%\apps\Qt5\plugins\platforms"
set "GDAL_DATA=%OSGEO4W_ROOT%\share\gdal"
set "PROJ_LIB=%OSGEO4W_ROOT%\share\proj"
set "QGIS_PLUGINPATH=%QGIS_PREFIX_PATH%\plugins"

cd /d "%APP_DIR%bin"
start "" "%APP_DIR%bin\LTZK.exe"
