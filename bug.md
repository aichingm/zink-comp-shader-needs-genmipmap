# [zink] updating texture in compute shader needs mipmap generation even with MIN_FILTER=LINEAR when using bindless textures

When writing to a texture (level 0) in a compute shader texturing something with that texture using bindless textures in the fragment shader does not use the updated values but the previous ones. For some reason even though the texture was created with `GL_TEXTURE_MIN_FILTER=GL_LINEAR` generating new mipmaps after the compute shader is done seams to work around the issue (which should not be necessary since there are no mipmaps without using `GL_TEXTURE_MIN_FILTER=GL_*_MIPMAP_*`).

The basic flow to reproduce is


```
// create a texture
glGenTextures(1, &texture);
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, texture);
glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 100, 100, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

// write to the texture in a compute shader
glUseProgram(comp_program);
glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
glDispatchCompute(100, 100, 1);  // YEAH not good but this is just a bug report
glMemoryBarrier(GL_ALL_BARRIER_BITS);

//potential hack (makes the whole idea of bindless textures useless since it binds them)
/*
glBindTexture(GL_TEXTURE_2D, texture);
glActiveTexture(GL_TEXTURE0);
glGenerateMipmap(GL_TEXTURE_2D);
*/

// draw
GLuint64 texture_handle = glGetTextureHandleARB(texture);
glUseProgram(draw_program);
if (!glIsTextureHandleResidentARB(texture_handle)) {
    glMakeTextureHandleResidentARB(texture_handle);
}
glProgramUniformHandleui64ARB(draw_program, fragment_shader_tex_location, texture_handle);
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

```



