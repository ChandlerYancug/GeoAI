@echo off
setlocal

rem ============================================================
rem  GeoAI launcher
rem
rem  Set QGIS_DIR to the root of your QGIS installation, e.g.
rem    set QGIS_DIR=C:\Program Files\QGIS 3.34.1
rem    set QGIS_DIR=E:\OSGeo4W
rem  If Qt lives outside the QGIS tree, also set QT_DIR, e.g.
rem    set QT_DIR=E:\OSGeo4W\apps\Qt5
rem ============================================================

if "%QGIS_DIR%"=="" set "QGIS_DIR=C:\Program Files\QGIS 3.34.1"

rem Standalone QGIS installer layout: <root>\apps\qgis-ltr
if exist "%QGIS_DIR%\apps\qgis-ltr\bin" (
    set "QGIS_PREFIX=%QGIS_DIR%\apps\qgis-ltr"
    set "QGIS_BIN=%QGIS_DIR%\apps\qgis-ltr\bin;%QGIS_DIR%\bin"
    set "QGIS_QT_PLUGINS=%QGIS_DIR%\apps\qgis-ltr\qtplugins;%QGIS_DIR%\plugins;%QGIS_DIR%\sqldrivers;%QGIS_DIR%\bin\sqldrivers"
) else (
    set "QGIS_PREFIX=%QGIS_DIR%"
    set "QGIS_BIN=%QGIS_DIR%\bin"
    set "QGIS_QT_PLUGINS=%QGIS_DIR%\qtplugins"
)

if not "%QT_DIR%"=="" set "QGIS_BIN=%QT_DIR%\bin;%QGIS_BIN%"
if not "%QT_DIR%"=="" set "QGIS_QT_PLUGINS=%QT_DIR%\plugins;%QGIS_QT_PLUGINS%"

rem OSGeo4W dev bundle keeps GDAL/PROJ DLLs in ..\3rdParty\bin
if exist "%QGIS_DIR%\..\3rdParty\bin" set "QGIS_BIN=%QGIS_DIR%\..\3rdParty\bin;%QGIS_BIN%"

set "PATH=%QGIS_BIN%;%PATH%"
set "QGIS_PREFIX_PATH=%QGIS_PREFIX%"
set "QT_PLUGIN_PATH=%QGIS_QT_PLUGINS%"

if exist "%QGIS_DIR%\share\gdal" set "GDAL_DATA=%QGIS_DIR%\share\gdal"
if exist "%QGIS_DIR%\..\3rdParty\bin\gdal_data" set "GDAL_DATA=%QGIS_DIR%\..\3rdParty\bin\gdal_data"
if exist "%QGIS_DIR%\share\proj" set "PROJ_LIB=%QGIS_DIR%\share\proj"
if exist "%QGIS_DIR%\bin\gdalplugins" set "GDAL_DRIVER_PATH=%QGIS_DIR%\bin\gdalplugins"

if not exist "%~dp0x64\Release\GeoAI.exe" (
    echo GeoAI.exe not found. Build the Release ^| x64 configuration first.
    pause
    exit /b 1
)

cd /d "%~dp0x64\Release"
GeoAI.exe
