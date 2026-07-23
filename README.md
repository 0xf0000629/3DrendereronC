# 3D Renderer

This is my 3D renderer with an integrated ray tracer, written in one really big C++ file. <br>
Uses WinAPI to do pretty much everything.

## Features

* Load and display object files.
* Free-fly camera controls using mouse and keyboard.
* Light source placing.
* Cube placing (with some customizable material properties, for ray-tracing).
* Ray-traced frame rendering from the current camera position.

## Controls

| Key            | Action                    |
| -------------- | ------------------------- |
| **Mouse**      | Look around               |
| **W A S D**    | Move camera               |
| **Up Arrow**   | Create a light source     |
| **Down Arrow** | Render a ray-traced frame |
| **C**          | Create a cube             |

### Lightsource Creation

Press **Up Arrow**, then enter the values into the terminal:

```
R G B Power
```

Example:

```
255 255 255 2.5
```

### Cube Creation

Press **C**, then enter:

```
Size R G B Diffusion Mirror
```

Parameters:

* **Size** – Cube size
* **R G B** – Cube color
* **Diffusion** – Diffuse reflectivity
* **Mirror** – Mirror reflectivity

Example:

```
2.0 255 0 0 0.8 0.2
```

## Ray Tracing

Press **Down Arrow** to render a ray-traced image from the camera's current position and orientation.

## Building

Compile using this:

```bash
g++.exe -O3 -std=c++17 -c 3DRENDERER.cpp -o 3DRENDERER.o
g++.exe -o 3DRENDERER.exe 3DRENDERER.o -O3 -static-libstdc++ -static-libgcc -static -lwinmm -lgdi32 -ldwmapi -lgdiplus
```

## Requirements

* Have to be on Windows
* Use MinGW g++ with C++17 support (you could try using something else if you're brave enough)
* GDI+, WinMM, GDI32, and DWM libraries

## Usage

1. Compile the project.
2. Launch `DRENDERER.exe.
3. Open an object file by typing it into the terminal window.
4. Fly through the scene using the mouse and **WASD**.
5. Add lights or cubes as needed.
6. Press **Down Arrow** to produce a ray-traced render of the current view.
