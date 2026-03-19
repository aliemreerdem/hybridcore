@echo off 
setlocal 
cd /d "%~dp0" 
echo [HybridCore] Initializing asymmetric hardware engine... 
bin\HybridCoreDiscovery.exe 
endlocal 
