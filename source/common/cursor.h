//
// Function to use SDL to create an SDL_Cursor
// for my eye dropper tool
//
#ifndef CURSOR_H_
#define CURSOR_H_

#include <SDL.h>

//Note, hardcoded to suport 16 x 16 XPM, with an extra row
//for the cursor hotspot
SDL_Cursor* SDL_CursorFromXPM(const char *image[]);
SDL_Cursor* SDL_CreateEyeDropperCursor();

#endif // CURSOR_H_H




