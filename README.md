gltf file Renderer through DX12 API
=============


## Requirment
#### you need Hardware which support DX12
#### Windows SDK
#### cmake

## User environment
#### visual studio 2022 version 17.14.8
#### Windows SDK(10.0.26100.0)
#### c++20 standard
#### cmake 3.31.4
#### cpu : 12th Gen Intel(R) Core(TM) i5-12400F
#### gpu : NVIDIA GeForce RTX 3060 Ti

# Set up  
> **if you have another scene to render, skip this step 1.      
> 1. download sponza scene
>   > This repo demo scene is sponza base scene.     
>   > Download scene .zip file in (https://www.intel.com/content/www/us/en/developer/topic-technology/graphics-research/samples.html)     
>   > Unzip them in the directory where this repo placed (if you want to place scene in other directory, see bellow "Other Scene Load").     
>   > Expected result of directory tree is like this.     
>   >   > -main_sponza    
>   >   > --(main sponza scene files...)    
>   >   > -MiniEngine     
>   >   > --(MiniEngine repo code files...)       

> 2. build
>   > run cmd. 
>   > enter "scripts\build.bat"    
>   > default build setting is Debug. if you want to build release profile, enter "scripts\build.bat release"    
>   > solution file will place in build\     
>   > .exe file will place in build\bin\{profile_name}\MiniEngineCore.exe     

# Other Scene Load
> If you want to load other scene, parse your path useing --scene_path="your_scene_path" in cmd.     
> Example: MiniEngineCore.exe --scene_path="your_scene_path.gltf"     

# example result
> Caution: Scene Loading can take several minutes, depending on the hardware.

![](example/ex1.gif)
![](example/ex2.gif)

## Third Party Licenses
- Dear ImGui : Copyright (c) 2014-2025 Omar Cornut — MIT
- DirectXTex : Copyright (c) Microsoft Corporation. — MIT
- DirectXTK12 : Copyright (c) Microsoft Corporation. — MIT
- WinPixEventRuntime : Copyright (c) Microsoft Corporation. — MIT
- TinyGLTF : Copyright (c) 2017 Syoyo Fujita, Aurélien Chatelain and many contributors — MIT