# Minimal Example to Trigger a Potential Bug in Zink

The `Makefile` provides three targets:

* `run-working` This example does not make use of bindless textures and seams to work fine
* `run-failing` This example replaces the texture bind mechanism with [bindless textures](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_bindless_texture.txt)
* `run-hack` This example might provide a pointer to whats going on by regenerating mipmaps (not a real solution...)

