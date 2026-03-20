@echo off

call compile_vulkan_shaders.bat

vendor\premake\premake5.exe vs2026

REM msbuild fps.slnx -v:m -p:Configuration=Debug
REM msbuild fps.slnx -v:m -p:Configuration=Release
msbuild fps.slnx -v:m -p:Configuration=Dist

REM xcopy /y /d external\lib\*.dll build\Debug
REM xcopy /y /d external\lib\*.dll build\Release
xcopy /y /d external\lib\*.dll build\Dist
