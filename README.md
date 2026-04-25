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

Loaded images are uploaded as OpenGL textures, rendered through a framebuffer pipeline, then displayed in the center preview. Halftone, Dots, and Contour are live GPU fragment-shader effects. Preview zoom and pan are viewport-only state; they do not mutate source or export dimensions.

PNG and JPEG export read back the final rendered framebuffer. The remaining export formats are visible as planned formats but are not wired yet.
