@echo off

if not exist data\shaders\compiled mkdir data\shaders\compiled

glslangValidator -V -S vert -DVERTEX_SHADER -DCOMM=out -o data\shaders\compiled\basic.vert.spv data\shaders\basic.glsl
glslangValidator -V -S frag -DFRAGMENT_SHADER -DCOMM=in -o data\shaders\compiled\basic.frag.spv data\shaders\basic.glsl
