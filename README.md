# Grainrad Offline Editor

Native C++ visual-effects editor inspired by Grainrad.

The first milestone in this workspace is the compileable processing foundation:

- CMake project layout
- stb-based PNG/JPG image loading and writing
- shared adjustment settings
- CPU ASCII generation with TXT/SVG export
- CPU Floyd-Steinberg dithering
- CPU horizontal brightness pixel sort
- optional GLFW/OpenGL/ImGui editor target, disabled until GUI dependencies are installed

Build the core verification CLI:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Example CLI usage:

```powershell
.\build\Release\grainrad_cli.exe input.png output.txt ascii
.\build\Release\grainrad_cli.exe input.png output.png dither
.\build\Release\grainrad_cli.exe input.jpg output.jpg pixelsort
```

Enable the GUI shell after installing GLFW and ImGui:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -DGRAINRAD_BUILD_GUI=ON -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake
```
