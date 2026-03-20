@echo off

call compile_vulkan_shaders.bat

vendor\premake\premake5.exe vs2026

msbuild fps.slnx -v:m -p:Configuration=Debug
REM msbuild fps.slnx -v:m -p:Configuration=Release
REM msbuild fps.slnx -v:m -p:Configuration=Dist

copy external\lib\*.dll build\Debug
REM copy external\lib\*.dll build\Release
REM copy external\lib\*.dll build\Dist
