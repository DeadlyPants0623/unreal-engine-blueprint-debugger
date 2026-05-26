@echo off
REM Local compile check via RunUAT BuildPlugin (verify before tagging a release).
REM Do NOT upload the Packaged\ output to Fab — releases use the CI source zip.
REM Set UE_ROOT to your UE 5.7 install before running.

if "%UE_ROOT%"=="" set UE_ROOT=C:\Program Files\Epic Games\UE_5.7

set PLUGIN=%~dp0..\Plugins\BPExecFlowViewer\BPExecFlowViewer.uplugin
if "%PACKAGE_OUT%"=="" set PACKAGE_OUT=%~dp0..\Packaged\BPExecFlowViewer
set OUT=%PACKAGE_OUT%

echo Plugin: %PLUGIN%
echo Output: %OUT%
echo UE:     %UE_ROOT%

"%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin ^
  -Plugin="%PLUGIN%" ^
  -Package="%OUT%" ^
  -TargetPlatforms=Win64

if errorlevel 1 (
  echo BuildPlugin failed.
  exit /b 1
)

echo Done. Upload folder: %OUT%
exit /b 0
