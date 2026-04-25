# Milestones

## Milestone 1: Processing Foundation

- CMake project.
- stb-based PNG/JPG loading and writing.
- CPU ASCII generation with TXT/SVG export.
- CPU Floyd-Steinberg dithering.
- CPU horizontal brightness pixel sort.
- CLI verification executable.

## Milestone 2: Native Editor Shell

- GLFW/OpenGL/ImGui editor executable.
- Fixed left input/effects rail, central preview, and scrollable right settings/export rail.
- Layout follows the ShaderLoom reference screenshots.
- Image loading through startup path, Windows browse dialog, and drag/drop.
- Real OpenGL texture preview for loaded PNG/JPG files.
- Preview-only zoom and pan state.

## Milestone 3: Render Pipeline

- Shader, texture, framebuffer, and fullscreen-quad wrappers.
- Local OpenGL function loader for the Windows GLFW context.
- First pass-through framebuffer pipeline:
  source texture -> preview framebuffer -> ImGui preview texture.
- Future ping-pong render passes:
  source -> adjustment -> processing -> effect -> post -> screen.
- Keep preview pan/zoom separate from export output.

## Milestone 4: First GPU Effects

- Halftone, dots, and contour fragment shaders.
- Dynamic right-panel settings for the selected GPU effect.
- Settings structs passed to shader uniforms.
- Other effects remain pass-through until their modules are wired.

## Milestone 5: Framebuffer Export

- Export the final rendered framebuffer, not the preview viewport.
- PNG and JPEG export through `stb_image_write`.
- Keep planned export tiles visible for GIF, Video, SVG, Text, and Three.js.
- Surface export status in the right settings rail.
