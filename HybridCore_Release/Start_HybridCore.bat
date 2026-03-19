@echo off 
setlocal 
cd /d "%~dp0" 
echo [HybridCore] Asimetrik motor baslatiliyor... 
bin\HybridCoreDiscovery.exe 
endlocal 
