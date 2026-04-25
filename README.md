# ShaderLoom Editor

A C++/OpenGL image-effects workbench for shader-based retro processing.

## Build

Core CLI:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Native editor:

```powershell
cmake -S . -B build-gui -G "Visual Studio 17 2022" -DShaderLoom_BUILD_GUI=ON -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build-gui --config Release
```

Run the editor with an optional startup image:

```powershell
.\build-gui\Release\ShaderLoom_editor.exe input.png
```

The editor also accepts PNG/JPG drag-and-drop, plus the Windows browse button in the left input panel.

## Current Pipeline

Loaded images are uploaded as OpenGL textures, rendered through a pass-through framebuffer pipeline, then displayed in the center preview. Preview zoom and pan are viewport-only state; they do not mutate source or export dimensions.
