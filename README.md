# ShaderLoom - ASCII + Retro Image Effects Editor

[![CMake](https://img.shields.io/badge/build-cmake-blue.svg)](https://cmake.org)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![OpenGL](https://img.shields.io/badge/preview-OpenGL-green.svg)](https://www.opengl.org/)

ShaderLoom is a compact C++/OpenGL image-effects workbench for building ASCII, halftone, dithering, and retro display treatments with an interactive native editor.

It features:
- Live OpenGL preview with framebuffer-based processing and post-processing
- CPU-backed ASCII, dithering, and pixel-sort paths for export-friendly effects
- ImGui editor with drag-and-drop image loading, zoom/pan preview, and raster export
- Procedural processing tools including noise-field tone warping and image distortion
- Retro finishing passes such as bloom, grain, chromatic offset, scanlines, vignette, CRT curve, and phosphor mask

---

## Active Editor

<img width="2560" height="1377" alt="image" src="https://github.com/user-attachments/assets/3dd2227b-90eb-4807-9427-c5f5ebb9adac" />
<img width="2560" height="1371" alt="image" src="https://github.com/user-attachments/assets/b3ecf733-f2bb-4233-8152-4a2123c2f973" />
<img width="2560" height="1368" alt="image" src="https://github.com/user-attachments/assets/5f018c1e-959b-4f90-bb8a-7a9a38647d72" />
<img width="2560" height="1368" alt="image" src="https://github.com/user-attachments/assets/9baa2e4d-5659-4705-8c48-7aaf436e46fd" />


ShaderLoom is built around a three-panel editor:
- Left rail for image input, effect selection, and project navigation
- Center viewport for the rendered OpenGL preview
- Right rail for effect controls, image adjustments, processing, post-processing, and export

The editor accepts PNG, JPG, JPEG, JFIF, and GIF input through drag-and-drop, the Windows file picker, or an optional startup image path.

---

## Features

### Effects
- ASCII renderer with multiple character ramps: Standard, Blocks, Binary, Detailed, Minimal, Alphabetic, Numeric, Math, and Symbols
- Dithering algorithms including Floyd-Steinberg, Atkinson, Jarvis-Judice-Ninke, Stucki, Burkes, Sierra, Sierra Two-Row, Sierra Lite, and Bayer 2x2
- Halftone renderer with circle, square, diamond, and line shapes
- Dots renderer with square and hexagonal grid layouts
- Contour bands for posterized tonal rendering
- Pixel sort pass with horizontal, vertical, and diagonal modes

### Processing
- Brightness, contrast, saturation, hue rotation, sharpness, and gamma controls
- Invert, brightness mapping, edge enhance, blur, color quantization, and shape matching
- Noise Field processing with:
  - interactive radial direction dial
  - tonal field strength
  - source-image distortion
  - scale and animation speed controls
- CPU and GPU processing parity for static exports and live preview paths where practical

### Post-Processing
- Bloom with threshold, soft threshold, intensity, and radius controls
- Film grain with Fine, Soft, Coarse, and Monochrome modes
- Chromatic offset
- Scanlines
- Vignette
- CRT curvature
- Phosphor mask

### Editor Features
- Native ImGui interface with compact rail-based controls
- OpenGL preview rendered through preprocess, effect, and post-process framebuffers
- Zoom and pan in the preview without mutating source or export dimensions
- Windows file picker support for input and output
- PNG and JPEG export from the final rendered framebuffer
- GIF, MP4, and WebM animation export through FFmpeg
- Planned export targets visible in the UI: SVG, Text, and Three.js

---

## What I Learned Building This

- Building a small C++ image-processing core around reusable effect modules
- Keeping CPU and GPU effect behavior close enough for preview/export parity
- Designing glyph ramps that avoid atlas dropouts and preserve brightness mapping
- Writing shader pipelines for preprocess, effect, and post-process passes
- Tuning procedural grain so it feels random rather than like a screen-space sweep
- Rebuilding pixel sort around contiguous active runs instead of broad whole-frame sorting
- Using noise fields both as tonal modulation and as image-space distortion
- Designing compact ImGui controls for visual parameters, including an interactive direction dial

---

## Technical Highlights

- C++20 core library for image IO, processing, ASCII, dithering, and pixel sorting
- stb_image / stb_image_write based PNG, JPEG, and GIF frame loading
- OpenGL 3.3 shader pipeline for live GPU effects
- Full printable ASCII glyph atlas support for shader-rendered text effects
- Noise-field distortion implemented as a procedural vector field before effect sampling
- Halftone and dots minimum-mark handling so dark regions still render visible structure
- Post-processing shader with bloom, smoothed grain, chromatic offset, scanlines, vignette, CRT curve, and phosphor mask
- CPU effect caching for heavier paths while controls are being adjusted

---

## Build / Run (Windows, PowerShell)

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

Run the editor:

```powershell
.\build-gui\Release\ShaderLoom_editor.exe
```

Run the editor with an optional startup image:

```powershell
.\build-gui\Release\ShaderLoom_editor.exe input.png
```

Notes:
- The core build requires stb headers. Install stb through vcpkg or pass `-DSTB_INCLUDE_DIR=<path>`.
- The GUI build expects OpenGL, glfw3, and imgui from the configured vcpkg toolchain.
- PNG and JPEG still export are wired.
- GIF, MP4, and WebM animation export require `ffmpeg` on PATH.
- SVG, Text, and Three.js export tiles are visible as planned formats.

---
