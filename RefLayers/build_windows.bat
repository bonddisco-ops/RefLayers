@echo off
cd /d "%~dp0"

cmake -B build -A x64 || goto :err
cmake --build build --config Release --target RefLayers_VST3 RefLayers_Standalone || goto :err

echo.
echo Done:
echo   build\RefLayers_artefacts\Release\VST3\RefLayers.vst3
echo Copy RefLayers.vst3 to C:\Program Files\Common Files\VST3 and rescan plugins in your DAW.
pause
exit /b 0

:err
echo.
echo BUILD FAILED - copy the error above and send it to me.
pause
exit /b 1
