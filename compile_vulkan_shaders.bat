@echo off

if not exist data\shaders\compiled mkdir data\shaders\compiled

glslangValidator -Idata\shaders -V -S vert -DVERTEX_SHADER -DCOMM=out -o data\shaders\compiled\basic.vert.spv data\shaders\basic.glsl
glslangValidator -Idata\shaders -V -S frag -DFRAGMENT_SHADER -DCOMM=in -o data\shaders\compiled\basic.frag.spv data\shaders\basic.glsl

glslangValidator -Idata\shaders -V -S vert -DVERTEX_SHADER -DCOMM=out -o data\shaders\compiled\basic_instanced.vert.spv data\shaders\basic_instanced.glsl
glslangValidator -Idata\shaders -V -S frag -DFRAGMENT_SHADER -DCOMM=in -o data\shaders\compiled\basic_instanced.frag.spv data\shaders\basic_instanced.glsl

glslangValidator -Idata\shaders -V -S vert -DVERTEX_SHADER -DCOMM=out -o data\shaders\compiled\quad.vert.spv data\shaders\quad.glsl
glslangValidator -Idata\shaders -V -S frag -DFRAGMENT_SHADER -DCOMM=in -o data\shaders\compiled\quad.frag.spv data\shaders\quad.glsl

glslangValidator -Idata\shaders -V -S vert -DVERTEX_SHADER -DCOMM=out -o data\shaders\compiled\shadow.vert.spv data\shaders\shadow.glsl
glslangValidator -Idata\shaders -V -S frag -DFRAGMENT_SHADER -DCOMM=in -o data\shaders\compiled\shadow.frag.spv data\shaders\shadow.glsl

glslangValidator -Idata\shaders -V -S vert -DVERTEX_SHADER -DCOMM=out -o data\shaders\compiled\shadow_instanced.vert.spv data\shaders\shadow_instanced.glsl
glslangValidator -Idata\shaders -V -S frag -DFRAGMENT_SHADER -DCOMM=in -o data\shaders\compiled\shadow_instanced.frag.spv data\shaders\shadow_instanced.glsl

glslangValidator -Idata\shaders -V -S vert -DVERTEX_SHADER -DCOMM=out -o data\shaders\compiled\resolve.vert.spv data\shaders\resolve.glsl
glslangValidator -Idata\shaders -V -S frag -DFRAGMENT_SHADER -DCOMM=in -o data\shaders\compiled\resolve.frag.spv data\shaders\resolve.glsl

glslangValidator -Idata\shaders -V -S vert -DVERTEX_SHADER -DCOMM=out -o data\shaders\compiled\ssao.vert.spv data\shaders\ssao.glsl
glslangValidator -Idata\shaders -V -S frag -DFRAGMENT_SHADER -DCOMM=in -o data\shaders\compiled\ssao.frag.spv data\shaders\ssao.glsl

glslangValidator -Idata\shaders -V -S vert -DVERTEX_SHADER -DCOMM=out -o data\shaders\compiled\ssao_blur.vert.spv data\shaders\ssao_blur.glsl
glslangValidator -Idata\shaders -V -S frag -DFRAGMENT_SHADER -DCOMM=in -o data\shaders\compiled\ssao_blur.frag.spv data\shaders\ssao_blur.glsl

glslangValidator -Idata\shaders -V -S vert -DVERTEX_SHADER -DCOMM=out -o data\shaders\compiled\z_prepass.vert.spv data\shaders\z_prepass.glsl
glslangValidator -Idata\shaders -V -S frag -DFRAGMENT_SHADER -DCOMM=in -o data\shaders\compiled\z_prepass.frag.spv data\shaders\z_prepass.glsl
