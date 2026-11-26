@echo off
setlocal

set "SRC_DIR=%~dp0"
set "DEBUG_EXE=%SRC_DIR%x64\Debug\LTZK.exe"
set "RELEASE_EXE=%SRC_DIR%x64\Release\LTZK.exe"

if exist "%DEBUG_EXE%" (
    set "TARGET_EXE=%DEBUG_EXE%"
    set "EXE_DIR=%SRC_DIR%x64\Debug\"
) else if exist "%RELEASE_EXE%" (
    set "TARGET_EXE=%RELEASE_EXE%"
    set "EXE_DIR=%SRC_DIR%x64\Release\"
) else (
    echo LTZK.exe not found.
    echo Expected first: "%DEBUG_EXE%"
    echo Fallback path:  "%RELEASE_EXE%"
    echo Please build Debug x64 or Release x64 in Visual Studio first.
    pause
    exit /b 1
)

if not exist "%SRC_DIR%.env" (
    echo .env not found: "%SRC_DIR%.env"
    echo Please edit .env and set QGIS_PREFIX_PATH and OSGEO4W_ROOT for this computer.
    pause
    exit /b 1
)

for /f "usebackq eol=# tokens=1,* delims==" %%A in ("%SRC_DIR%.env") do (
    if not "%%~A"=="" set "%%~A=%%~B"
)

if not defined QGIS_PREFIX_PATH (
    echo QGIS_PREFIX_PATH is not configured in "%SRC_DIR%.env"
    pause
    exit /b 1
)

if not defined OSGEO4W_ROOT (
    echo OSGEO4W_ROOT is not configured in "%SRC_DIR%.env"
    pause
    exit /b 1
)

set "QGIS_PREFIX_PATH=%QGIS_PREFIX_PATH:"=%"
set "OSGEO4W_ROOT=%OSGEO4W_ROOT:"=%"

if not "%QGIS_PREFIX_PATH:~1,1%"==":" if not "%QGIS_PREFIX_PATH:~0,2%"=="\\" set "QGIS_PREFIX_PATH=%SRC_DIR%%QGIS_PREFIX_PATH%"
if not "%OSGEO4W_ROOT:~1,1%"==":" if not "%OSGEO4W_ROOT:~0,2%"=="\\" set "OSGEO4W_ROOT=%SRC_DIR%%OSGEO4W_ROOT%"

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

set "PATH=%EXE_DIR%;%QGIS_PREFIX_PATH%\bin;%OSGEO4W_ROOT%\bin;%OSGEO4W_ROOT%\apps\Qt5\bin;%PATH%"
set "QT_PLUGIN_PATH=%OSGEO4W_ROOT%\apps\Qt5\plugins"
set "QT_QPA_PLATFORM_PLUGIN_PATH=%OSGEO4W_ROOT%\apps\Qt5\plugins\platforms"
set "GDAL_DATA=%OSGEO4W_ROOT%\share\gdal"
set "PROJ_LIB=%OSGEO4W_ROOT%\share\proj"
set "QGIS_PLUGINPATH=%QGIS_PREFIX_PATH%\plugins"

cd /d "%EXE_DIR%"
start "" "%TARGET_EXE%"
