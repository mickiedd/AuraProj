@echo off
set UE_ENGINE_ROOT=D:\UE_5.5
cd /d D:\Users\zhouzhiqiang\Documents\GitHub\AuraProj
call D:\UE_5.5\Engine\Build\BatchFiles\Build.bat AuraServer Win64 Development D:\Users\zhouzhiqiang\Documents\GitHub\AuraProj\Aura.uproject -waitmutex
