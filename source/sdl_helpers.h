// SDL Helper Functions
//
// Stuff that's specifically for SDL, that I wish was built in
//
#ifndef SDL_HELPERS_H
#define SDL_HELPERS_H

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

// About Desktop OpenGL function loaders:
//  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
//  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
//  You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>    // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>    // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>  // Initialize with gladLoadGL()
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

#include <map>
#include <vector>
#include <SDL_Surface.h>

#ifndef GLuint
typedef unsigned int	GLuint;		/* 4-byte unsigned */
typedef float		GLfloat;	/* single precision float */
#endif

GLuint SDL_GL_LoadTexture(SDL_Surface * surface, GLfloat * texcoord);
std::vector<SDL_Surface*> SDL_GIF_Load(const char* pFilePath);
std::vector<SDL_Surface*> SDL_FAN_Load(const char* pFilePath);

int SDL_Surface_CountUniqueColors(SDL_Surface* pSurface, std::map<Uint32,Uint32>* pGlobalHistogram = nullptr );

void SDL_IMG_SaveFAN(std::vector<SDL_Surface*> pSurfaces, const char* pFilePath, bool bTiled=false);

#endif // SDL_HELPERS_H

