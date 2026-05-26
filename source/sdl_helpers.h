// SDL Helper Functions
//
// Stuff that's specifically for SDL, that I wish was built in
//
#ifndef SDL_HELPERS_H
#define SDL_HELPERS_H

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <SDL_opengl.h>

#include <map>
#include <vector>
#include <SDL_surface.h>

#ifndef GLuint
typedef unsigned int	GLuint;		/* 4-byte unsigned */
typedef float		GLfloat;	/* single precision float */
#endif

GLuint SDL_GL_LoadTexture(SDL_Surface * surface, GLfloat * texcoord);
std::vector<SDL_Surface*> SDL_ANM_Load(const char* pFilePath);
std::vector<SDL_Surface*> SDL_C2_Load(const char* pFilePath);
std::vector<SDL_Surface*> SDL_16_Load(const char* pFilePath);
std::vector<SDL_Surface*> SDL_256_Load(const char* pFilePath);
std::vector<SDL_Surface*> SDL_FAN_Load(const char* pFilePath);
std::vector<SDL_Surface*> SDL_FLC_Load(const char* pFilePath);
std::vector<SDL_Surface*> SDL_GIF_Load(const char* pFilePath);
std::vector<SDL_Surface*> SDL_GSLA_Load(const char* pFilePath);
SDL_Surface* SDL_C1_Load(const char* pFilePath);

int SDL_Surface_CountUniqueColors(SDL_Surface* pSurface, std::map<Uint32,Uint32>* pGlobalHistogram = nullptr );

void SDL_IMG_Save16(std::vector<SDL_Surface*> pSurfaces, const char* pFilePath);
void SDL_IMG_Save256(std::vector<SDL_Surface*> pSurfaces, const char* pFilePath);
void SDL_IMG_SaveFAN(std::vector<SDL_Surface*> pSurfaces, const char* pFilePath, bool bTiled=false);

#endif // SDL_HELPERS_H

