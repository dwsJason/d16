// Really dumb embedded eyedropper
// based on SDL2 Example

// https://wiki.libsdl.org/SDL_CreateCursor

#include "cursor.h"
#include <stdio.h>

/* XPM */
static const char* dropper[] = {
/* width height num_colors chars_per_pixel */
"16 16 3 1",
/* colors */
" 	c None",
".	c #000000",
"X	c #FFFFFF",
"           .... ",
"           .....",
"           .....",
"          ......",
"       . .......",
"       X.....   ",
"       .X...    ",
"      .XXX.     ",
"     .XXX.X.    ",
"    .XXX.       ",
"   .XXX.        ",
"  .XXX.         ",
"  .XX.          ",
" .X..           ",
".X.             ",
" .              ",
"0,15"};

SDL_Cursor* SDL_CursorFromXPM(const char *image[])
{
	const int width = 16;
	const int height = 16;

  int i, row, col;
  Uint8 data[16*16/sizeof(char)];
  Uint8 mask[16*16/sizeof(char)];
  int hot_x, hot_y;

  i = -1;
  for (row=0; row<height; ++row) {
    for (col=0; col<width; ++col) {
      if (col % 8) {
        data[i] <<= 1;
        mask[i] <<= 1;
      } else {
        ++i;
        data[i] = mask[i] = 0;
      }
      switch (image[4+row][col]) {
        case '.':
          data[i] |= 0x01;
          mask[i] |= 0x01;
          break;
        case 'X':
          mask[i] |= 0x01;
          break;
        case ' ':
          break;
      }
    }
  }
  sscanf(image[4+row], "%d,%d", &hot_x, &hot_y);
  return SDL_CreateCursor(data, mask, width, height, hot_x, hot_y);
}

SDL_Cursor* SDL_CreateEyeDropperCursor()
{
	return SDL_CursorFromXPM( dropper );
}

