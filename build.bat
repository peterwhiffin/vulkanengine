@echo off
call cmpshaders.bat
REM pushd build\win
set SRC=src\main.c
set FLAGS=-std=c23 -g
set OUT=-o build\win\vulkanengine.exe
set LIBS=-lSDL3.lib -luser32.lib -lgdi32.lib -lshell32.lib -lslang.lib -lvma.lib
set DEF=-DLOG_LVL=3
set INC=^
-Iinc\ ^
-I..\SDL\include ^
-I..\glm ^
-I..\volk\ ^
-I..\VulkanMemoryAllocator\include ^
-I..\cglm\include ^
-I..\..\vulkanSDK\Include\ ^
-L..\..\vulkanSDK\Lib ^
-Llib\win 

clang %FLAGS% %OUT% %DEF% %SRC% %INC% %LIBS%
REM popd
