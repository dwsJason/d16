//
// SDL Helper Functions
//
// Stuff that's specifically for SDL, that I wish was built in
//

#include "sdl_helpers.h"

// include the oldest, crustiest gif library out there
#include "gif_lib.h"

//------------------------------------------------------------------------------
/* Quick utility function for texture creation */
static int
power_of_two(int input)
{
    int value = 1;

    while (value < input) {
        value <<= 1;
    }
    return value;
}

GLuint
SDL_GL_LoadTexture(SDL_Surface * surface, GLfloat * texcoord)
{
    GLuint texture;
    int w, h;
    SDL_Surface *image;
    SDL_Rect area;
    SDL_BlendMode saved_mode;

    /* Use the surface width and height expanded to powers of 2 */
    w = power_of_two(surface->w);
    h = power_of_two(surface->h);
    texcoord[0] = 0.0f;         /* Min X */
    texcoord[1] = 0.0f;         /* Min Y */
    texcoord[2] = (GLfloat) surface->w / w;     /* Max X */
    texcoord[3] = (GLfloat) surface->h / h;     /* Max Y */

    image = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN     /* OpenGL RGBA masks */
                                 0x000000FF,
                                 0x0000FF00, 0x00FF0000, 0xFF000000
#else
                                 0xFF000000,
                                 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
        );
    if (image == NULL) {
        return 0;
    }

    /* Save the alpha blending attributes */
    SDL_GetSurfaceBlendMode(surface, &saved_mode);
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);

    /* Copy the surface into the GL texture image */
    area.x = 0;
    area.y = 0;
    area.w = surface->w;
    area.h = surface->h;
    SDL_BlitSurface(surface, &area, image, &area);

    /* Restore the alpha blending attributes */
    SDL_SetSurfaceBlendMode(surface, saved_mode);

    /* Create an OpenGL texture for the image */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    SDL_FreeSurface(image);     /* No longer needed */

    return texture;
}

//------------------------------------------------------------------------------
//
// !!$$ Not Thread SAFE
// !!$$ Must be called starting with frameNo 0, then sequentially
//
SDL_Surface* SDL_GifGetSurface(GifFileType* pGif, int frameNo)
{
// I know this is a bad, bad, bad idea
static unsigned char* pPreviousCanvas = nullptr;

	SavedImage *pGifImage = &pGif->SavedImages[ frameNo ];

	unsigned char* pRawPixels = (unsigned char*)malloc( pGif->SWidth * pGif->SHeight );

	if (0 == frameNo)
	{
		// First Frame, don't start with Garbage
		memset(pRawPixels, 0, pGif->SWidth * pGif->SHeight);
	}
	else
	{
		// Copy the Previous Canvas, onto the current Canvas?
		// Probably
		memcpy(pRawPixels, pPreviousCanvas, pGif->SWidth * pGif->SHeight);
	}

	pPreviousCanvas = pRawPixels;


	// Copy the Rect from here, onto the canvas
	for (int srcY = 0; srcY < pGifImage->ImageDesc.Height; ++srcY)
	{
		for (int srcX = 0; srcX < pGifImage->ImageDesc.Width; ++srcX)
		{
			int srcIndex = (srcY * pGifImage->ImageDesc.Width) + srcX;
			int dstIndex = ((srcY + pGifImage->ImageDesc.Top) * pGif->SWidth) +
							 srcX + pGifImage->ImageDesc.Left;

			pRawPixels[dstIndex] = pGifImage->RasterBits[ srcIndex ];
		}
	}

	SDL_Surface *pTargetSurface = SDL_CreateRGBSurfaceWithFormatFrom(
		pRawPixels, pGif->SWidth, pGif->SHeight,
		8, pGif->SWidth, SDL_PIXELFORMAT_INDEX8);


	// Assign Global Color Map
	ColorMapObject *pColorMap = pGif->SColorMap;

	if (!pColorMap)
	{
		// No Global Color Map, Assign Local Color Map
		pColorMap = pGifImage->ImageDesc.ColorMap;
	}

	if (pColorMap)
	{
		// We need to set some colors, if we have colors
		SDL_Palette *pPalette = SDL_AllocPalette(pColorMap->ColorCount);

		// Gif Colors to SDL Colors
		for (int idx = 0; idx < pColorMap->ColorCount; ++idx)
		{
			GifColorType inColor = pColorMap->Colors[ idx ];
			SDL_Color outColor;
			outColor.r = inColor.Red;
			outColor.g = inColor.Green;
			outColor.b = inColor.Blue;
			outColor.a = 255;

			SDL_SetPaletteColors(pPalette, (const SDL_Color *)&outColor, idx, 1);
		}

		SDL_SetSurfacePalette(pTargetSurface, pPalette);
	}

	// It turns out there's a userdata field in the surface
	// I might as well use this to convey the play-speed information

	int delayTime = 0;
	int transparentColor = NO_TRANSPARENT_COLOR;

	for (int idx = 0; idx < pGifImage->ExtensionBlockCount; ++idx)
	{
		if ( GRAPHICS_EXT_FUNC_CODE == pGifImage->ExtensionBlocks[idx].Function )
		{
			// We have a Grapics Control Block, which holds the transparent color ? (not sure we care)
			// and the DelayTime  (I definitely care)
			GraphicsControlBlock GCB;

			if (GIF_OK == DGifExtensionToGCB(pGifImage->ExtensionBlocks[idx].ByteCount,
							   pGifImage->ExtensionBlocks[idx].Bytes,
							   &GCB))
			{
				delayTime = GCB.DelayTime;
				transparentColor = GCB.TransparentColor;
			}
		}
	}

	// Stuff in the Delay Time, and the Transparent Color Index
	pTargetSurface->userdata = (void *)((delayTime & 0xFFFF) | (transparentColor << 16));

	return pTargetSurface;
}

std::vector<SDL_Surface*> SDL_GIF_Load(const char* pFilePath)
{
	std::vector<SDL_Surface*> results;

	int error = 0; // yes, I need to init this
	GifFileType* pGifFile = DGifOpenFileName(pFilePath, &error);

	if ((D_GIF_SUCCEEDED == error) && pGifFile)
	{
        if(GIF_OK == DGifSlurp(pGifFile))
		{
			const int imageCount   = pGifFile->ImageCount;

			for (int idx = 0; idx < imageCount; ++idx)
			{
				SDL_Surface* pSurface = SDL_GifGetSurface(pGifFile, idx);
				results.push_back(pSurface);
			}
		}

        DGifCloseFile(pGifFile, &error);
	}

	return results;
}

//------------------------------------------------------------------------------

