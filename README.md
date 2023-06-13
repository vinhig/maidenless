# Maidenless Engine

Maidenless Engine is an MIT multi-platform 3D engine that enables developers and content creators to build BBB games, cartoon-quality 3D worlds, and low-fidelity simulations without any fees or commercial obligations (to be updated).

## Dependencies

* C compiler (clang or gcc).
* Latest [Vulkan SDK](https://vulkan.lunarg.com/).
* [Meson](https://mesonbuild.com/) build system.

## Requirements

* A Vulkan 1.3 GPU with ray-tracing support.

## Build & Run

```
git clone https://github.com/vinhig/maidenless
cd maidenless
meson setup build
meson compile -C build
cd build
./maidenless
```
