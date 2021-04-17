//
// SDL Helper Functions
//
// Stuff that's specifically for SDL, that I wish was built in
//

#include "sdl_helpers.h"

#include "256_file.h"  // Support C256 Image File
#include "anm_file.h"  // Support Deluxe Animation File
#include "c2_file.h"   // Support Paintworks Animation File
#include "fan_file.h"  // Support for Foenix Animation File
#include "gsla_file.h" // Support for GSLA Animation File

// include the oldest, crustiest gif library out there, it forces you to
// learn all the little details about gif, when it should just make it easy
#include "gif_lib.h"

#include "log.h"

#include <map>

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

	// Get the transparent color, before the copy
	int delayTime = 0;
	int transparentColor = NO_TRANSPARENT_COLOR;
	int disposalMode = DISPOSAL_UNSPECIFIED;

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
				disposalMode = GCB.DisposalMode;
			}
		}
	}

	// Frame Clear Stuff
	if (0 == frameNo)
	{
		pPreviousCanvas = (unsigned char*)realloc( pPreviousCanvas, pGif->SWidth * pGif->SHeight );
		// First Frame, don't start with Garbage
		memset(pRawPixels, pGif->SBackGroundColor, pGif->SWidth * pGif->SHeight);
		memset(pPreviousCanvas, pGif->SBackGroundColor, pGif->SWidth * pGif->SHeight);
	}
	else
	{
		memcpy(pRawPixels, pPreviousCanvas, pGif->SWidth * pGif->SHeight);
	}

	// Copy the Rect from here, onto the canvas
	for (int srcY = 0; srcY < pGifImage->ImageDesc.Height; ++srcY)
	{
		for (int srcX = 0; srcX < pGifImage->ImageDesc.Width; ++srcX)
		{
			int srcIndex = (srcY * pGifImage->ImageDesc.Width) + srcX;
			GifByteType pixel = pGifImage->RasterBits[ srcIndex ];

			if (pixel != transparentColor)
			{
				int dstIndex = ((srcY + pGifImage->ImageDesc.Top) * pGif->SWidth) +
								 srcX + pGifImage->ImageDesc.Left;

				pRawPixels[dstIndex] = pixel;
			}
		}
	}

	SDL_Surface *pTargetSurface = SDL_CreateRGBSurfaceWithFormatFrom(
		pRawPixels, pGif->SWidth, pGif->SHeight,
		8, pGif->SWidth, SDL_PIXELFORMAT_INDEX8);


	// Assign Local Color Map
	ColorMapObject* pColorMap = pGifImage->ImageDesc.ColorMap;

	if (!pColorMap)
	{
		// No Local Color Map, Assign Global Color Map
		pColorMap = pGif->SColorMap;
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


	// Stuff in the Delay Time, and the Transparent Color Index
	pTargetSurface->userdata = (void *)((long long)((delayTime & 0xFFFF) | (transparentColor << 16)));

	switch (disposalMode)
	{
	case DISPOSE_BACKGROUND:
		//$$JGA Note, this might supposed to be just the sub-region of the current
		//$$JGA Frame
		//memset(pPreviousCanvas, pGif->SBackGroundColor, pGif->SWidth * pGif->SHeight);
		{
			int top   = pGifImage->ImageDesc.Top;
			int left  = pGifImage->ImageDesc.Left;
			int width = pGifImage->ImageDesc.Width;
			int height= pGifImage->ImageDesc.Height;

			int canvasIndex = top * pGif->SWidth;
			canvasIndex += left;

			// Clear only the rect
			for (int y = 0; y < height; ++y)
			{
				memset(pPreviousCanvas + canvasIndex, pGif->SBackGroundColor, width);
				canvasIndex += pGif->SWidth;
			}

		}
		break;

	case DISPOSE_PREVIOUS:
		break;

	case DISPOSAL_UNSPECIFIED:
	case DISPOSE_DO_NOT:
	default:
		memcpy(pPreviousCanvas, pRawPixels, pGif->SWidth * pGif->SHeight);
		break;
	}
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
//------------------------------------------------------------------------------

SDL_Surface* SDL_FanGetSurface(FanFile& fanFile, int frameNo)
{
	SDL_Surface* pTargetSurface = nullptr;

	const FAN_Palette& clut = fanFile.GetPalette();
	const std::vector<unsigned char*>& pPixelMaps = fanFile.GetPixelMaps();

	int width  = fanFile.GetWidth();
	int height = fanFile.GetHeight();

	unsigned char* pRawPixels = new unsigned char[ width * height ];
	memcpy(pRawPixels, pPixelMaps[ frameNo ], width * height);

	pTargetSurface = SDL_CreateRGBSurfaceWithFormatFrom(
		pRawPixels, width, height,
		8, width, SDL_PIXELFORMAT_INDEX8);

	SDL_Palette *pPalette = SDL_AllocPalette(clut.iNumColors);

	// FAN Colors to SDL Colors
	for (int idx = 0; idx < clut.iNumColors; ++idx)
	{
		FAN_Color inColor = clut.pColors[ idx ];
		SDL_Color outColor;
		outColor.r = inColor.r;
		outColor.g = inColor.g;
		outColor.b = inColor.b;
		outColor.a = inColor.a;

		SDL_SetPaletteColors(pPalette, (const SDL_Color *)&outColor, idx, 1);
	}

	SDL_SetSurfacePalette(pTargetSurface, pPalette);

	// Stuff in the Delay Time
	int delayTime = 4; 	// hard coded for now
	pTargetSurface->userdata = (void *)((long long)(delayTime & 0xFFFF));

	return pTargetSurface;
}

//------------------------------------------------------------------------------

SDL_Surface* SDL_256GetSurface(C256File& c256File, int frameNo)
{
	SDL_Surface* pTargetSurface = nullptr;

	const C256_Palette& clut = c256File.GetPalette();
	const std::vector<unsigned char*>& pPixelMaps = c256File.GetPixelMaps();

	int width  = c256File.GetWidth();
	int height = c256File.GetHeight();

	unsigned char* pRawPixels = new unsigned char[ width * height ];
	memcpy(pRawPixels, pPixelMaps[ frameNo ], width * height);

	pTargetSurface = SDL_CreateRGBSurfaceWithFormatFrom(
		pRawPixels, width, height,
		8, width, SDL_PIXELFORMAT_INDEX8);

	SDL_Palette *pPalette = SDL_AllocPalette(clut.iNumColors);

	// C256 Colors to SDL Colors
	for (int idx = 0; idx < clut.iNumColors; ++idx)
	{
		C256_Color inColor = clut.pColors[ idx ];
		SDL_Color  outColor;
		outColor.r = inColor.r;
		outColor.g = inColor.g;
		outColor.b = inColor.b;
		outColor.a = inColor.a;

		SDL_SetPaletteColors(pPalette, (const SDL_Color *)&outColor, idx, 1);
	}

	SDL_SetSurfacePalette(pTargetSurface, pPalette);

	// Stuff in the Delay Time
	int delayTime = 4; 	// hard coded for now
	pTargetSurface->userdata = (void *)((long long)(delayTime & 0xFFFF));

	return pTargetSurface;
}

//------------------------------------------------------------------------------

//
//  Helpers
//
std::vector<SDL_Surface*> SDL_FAN_Load(const char* pFilePath)
{
	std::vector<SDL_Surface*> results;

	FanFile fanFile(pFilePath);

	int numFrames = fanFile.GetFrameCount();

	for (int idx = 0; idx < numFrames; ++idx)
	{
		SDL_Surface* pSurface = SDL_FanGetSurface(fanFile, idx);
		results.push_back(pSurface);
	}

	return results;
}


//------------------------------------------------------------------------------

//
//  Helpers
//
std::vector<SDL_Surface*> SDL_256_Load(const char* pFilePath)
{
	std::vector<SDL_Surface*> results;

	C256File c256File(pFilePath);

	int numFrames = c256File.GetFrameCount();

	for (int idx = 0; idx < numFrames; ++idx)
	{
		SDL_Surface* pSurface = SDL_256GetSurface(c256File, idx);
		results.push_back(pSurface);
	}

	return results;
}

//------------------------------------------------------------------------------

int SDL_Surface_CountUniqueColors(SDL_Surface* pSurface, std::map<Uint32,Uint32>* pGlobalHistogram )
{
	int width  = pSurface->w;
	int height = pSurface->h;

    SDL_Surface *pImage = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height,
											   32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN     /* OpenGL RGBA masks */
                                 0x000000FF,
                                 0x0000FF00, 0x00FF0000, 0xFF000000
#else
                                 0xFF000000,
                                 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
											   );

	if (nullptr == pImage)
	{
		return 0;
	}

	// Copy the source image into 32bpp format
	{
		/* Save the alpha blending attributes */
		SDL_Rect area;
		SDL_BlendMode saved_mode;
		SDL_GetSurfaceBlendMode(pSurface, &saved_mode);
		SDL_SetSurfaceBlendMode(pSurface, SDL_BLENDMODE_NONE);

		/* Copy the surface into the GL texture image */
		area.x = 0;
		area.y = 0;
		area.w = pSurface->w;
		area.h = pSurface->h;
		SDL_BlitSurface(pSurface, &area, pImage, &area);

		/* Restore the alpha blending attributes */
		SDL_SetSurfaceBlendMode(pSurface, saved_mode);
	}

    if( SDL_MUSTLOCK(pImage) )
        SDL_LockSurface(pImage);

	std::map<Uint32,Uint32> local_histogram;

	for (int y = 0; y < pImage->h; ++y)
	{
		for (int x = 0; x < pImage->w; ++x)
		{
			Uint8 * pixel = (Uint8*)pImage->pixels;
			pixel += (y * pImage->pitch) + (x * sizeof(Uint32));

			Uint32 color = *((Uint32*)pixel);

			local_histogram[ color ] = 1;
			if (pGlobalHistogram)
			{
				(*pGlobalHistogram)[color] = 1;
			}
		}
	}

    if( SDL_MUSTLOCK(pImage) )
        SDL_UnlockSurface(pImage);

    SDL_FreeSurface(pImage);     /* No longer needed */

	return (int)local_histogram.size();
}

//------------------------------------------------------------------------------

void SDL_IMG_Save256(std::vector<SDL_Surface*> pSurfaces, const char* pFilePath)
{
	if (pSurfaces.size())
	{
		// 1. Convert Surfaces into a format that the C256File can accept
		SDL_Surface* pSurface = pSurfaces[0];

		int width  = pSurface->w;
		int height = pSurface->h;

		// Check for palette right now
		SDL_Palette *pPal = pSurface->format->palette;

		if (!pPal)
		{
			LOG("FAILED: Save C256 Bitmap: %s\n", pFilePath);
			LOG("SDL_Surface does not contain a palette!\n");
			return;
		}

		// Ok, to keep the types as simple as possible, the C256File class
		// requires a list of pointer to a char array, that contains the data it's going to process
		// the char array contains the 8 bit index data used to represent the pixels

		// As it happens, each our surfaces, contains such an array, already

		std::vector<unsigned char*> pixelMaps;

		for (int idx = 0; idx < pSurfaces.size(); ++idx)
		{
			pSurface = pSurfaces[ idx ];

			if( SDL_MUSTLOCK(pSurface) )
				SDL_LockSurface(pSurface);

			pixelMaps.push_back( (unsigned char*)pSurface->pixels );
		}

		// 2. Convert Color Table into a format that the C256File can accept;
		//
		// Convert the surface palette, into a Foenix palette B G R FF, format

		C256_Palette palette;
		palette.iNumColors = pPal->ncolors;
		palette.pColors = new C256_Color[ pPal->ncolors ];

		for (int idx = 0; idx < pPal->ncolors; ++idx)
		{
			palette.pColors[ idx ].r = pPal->colors[ idx ].r;
			palette.pColors[ idx ].g = pPal->colors[ idx ].g;
			palette.pColors[ idx ].b = pPal->colors[ idx ].b;
			palette.pColors[ idx ].a = pPal->colors[ idx ].a;
		}


		// Create the C256File Object

		C256File c256File(width, height, palette.iNumColors);

		// Add the colors
		c256File.SetPalette( palette );
		// free the palette
		delete[] palette.pColors;
		palette.pColors = nullptr;

		// Add the pixels
		c256File.AddImages( pixelMaps );

		// Save the File
		c256File.SaveToFile( pFilePath );

		// Unlock Surfaces
		for (int idx = 0; idx < pSurfaces.size(); ++idx)
		{
			pSurface = pSurfaces[ idx ];

			if( SDL_MUSTLOCK(pSurface) )
				SDL_UnlockSurface(pSurface);
		}

	}
}

//------------------------------------------------------------------------------

void SDL_IMG_SaveFAN(std::vector<SDL_Surface*> pSurfaces, const char* pFilePath, bool bTiled)
{
	(void)bTiled;

	if (pSurfaces.size())
	{
		// 1. Convert Surfaces into a format that the FanFile can accept
		SDL_Surface* pSurface = pSurfaces[0];

		int width  = pSurface->w;
		int height = pSurface->h;

		if (bTiled)
		{
			if ((width & 0xF) || (height & 0xF))
			{
				LOG("FAILED: Save Tiled FAN: %s\n", pFilePath);
				LOG("Save Tile Based FAN, Image must be a multiple of 16 pixels\n");
				LOG("Width = %d, Height = %d\n", width, height );
				return;
			}
		}

		// Check for palette right now
		SDL_Palette *pPal = pSurface->format->palette;

		if (!pPal)
		{
			LOG("FAILED: Save Tiled FAN: %s\n", pFilePath);
			LOG("SDL_Surface does not contain a palette!\n");
			return;
		}

		// Ok, to keep the types as simple as possible, the FanFile class
		// requires a list of pointer to a char array, that contains the data it's going to process
		// the char array contains the 8 bit index data used to represent the pixels

		// As it happens, each our surfaces, contains such an array, already

		std::vector<unsigned char*> pixelMaps;

		for (int idx = 0; idx < pSurfaces.size(); ++idx)
		{
			pSurface = pSurfaces[ idx ];

			if( SDL_MUSTLOCK(pSurface) )
				SDL_LockSurface(pSurface);

			pixelMaps.push_back( (unsigned char*)pSurface->pixels );
		}

		// 2. Convert Color Table into a format that the FanFile can accept;
		//
		//  FAN Files technically support palette animation, but I'm not doing this yet
		//

		// Convert the surface palette, into a Foenix palette B G R FF, format

		FAN_Palette palette;
		palette.iNumColors = pPal->ncolors;
		palette.pColors = new FAN_Color[ pPal->ncolors ];

		for (int idx = 0; idx < pPal->ncolors; ++idx)
		{
			palette.pColors[ idx ].r = pPal->colors[ idx ].r;
			palette.pColors[ idx ].g = pPal->colors[ idx ].g;
			palette.pColors[ idx ].b = pPal->colors[ idx ].b;
			palette.pColors[ idx ].a = pPal->colors[ idx ].a;
		}


		// Create the FanFile Object

		FanFile fanFile(width, height, palette.iNumColors);

		// Add the colors
		fanFile.SetPalette( palette );
		// free the palette
		delete[] palette.pColors;
		palette.pColors = nullptr;

		// Add the pixels
		fanFile.AddImages( pixelMaps );

		// Save the File
		fanFile.SaveToFile( pFilePath );

		// Unlock Surfaces
		for (int idx = 0; idx < pSurfaces.size(); ++idx)
		{
			pSurface = pSurfaces[ idx ];

			if( SDL_MUSTLOCK(pSurface) )
				SDL_UnlockSurface(pSurface);
		}

	}
}

//------------------------------------------------------------------------------

SDL_Surface* SDL_C1DataToSurface(unsigned char* bytes)
{
	SDL_Surface* pSurface = nullptr;

	// Convert to 256 color index image. then take that into a surface
	unsigned char* pRawPixels = new unsigned char[ 320 * 200 ];

	// Get the pixels (no 640, and no fill mode)
	for (int y = 0; y < 200; ++y)
	{
		unsigned char pal_index = bytes[ 0x7D00 + y ] << 4;

		int source_index = y * 160;
		int dest_index = y * 320;

		for (int x = 0; x < 320; x+=2)
		{
			unsigned char source_pixel = bytes[ source_index + (x>>1) ];

			pRawPixels[ dest_index + x + 0 ] = ((source_pixel>>4) & 0xF) | pal_index;
			pRawPixels[ dest_index + x + 1 ] = ((source_pixel>>0) & 0xF) | pal_index;
		}
	}

	// GS Colors to SDL Colors
	unsigned short* pColors = (unsigned short*)&bytes[0x7E00];
	SDL_Palette *pPalette = SDL_AllocPalette(256);

	for (int idx = 0; idx < 256; ++idx)
	{
		SDL_Color outColor;
		unsigned short inColor = pColors[ idx ];

		outColor.r = ((inColor >> 8) & 0xF);
		outColor.r|= outColor.r<<4;
		outColor.g = ((inColor >> 4) & 0xF);
		outColor.g|= outColor.g<<4;
		outColor.b = ((inColor >> 0) & 0xF);
		outColor.b|= outColor.b<<4;
		outColor.a = 255;

		SDL_SetPaletteColors(pPalette, (const SDL_Color *)&outColor, idx, 1);
	}

	pSurface = SDL_CreateRGBSurfaceWithFormatFrom(
		pRawPixels, 320, 200,
		8, 320, SDL_PIXELFORMAT_INDEX8);

	SDL_SetSurfacePalette(pSurface, pPalette);

	return pSurface;
}

//------------------------------------------------------------------------------

SDL_Surface* SDL_C2GetSurface(C2File& c2File, int frameNo)
{
	const std::vector<unsigned char*>& c1datas = c2File.GetPixelMaps();

	SDL_Surface* pResult = SDL_C1DataToSurface(c1datas[ frameNo ]);

	// Stuff in the Delay Time
	int delayTime = 4; 	// hard coded for now
	pResult->userdata = (void *)((long long)(delayTime & 0xFFFF));

	return pResult;
}

//------------------------------------------------------------------------------

std::vector<SDL_Surface*> SDL_C2_Load(const char* pFilePath)
{
	std::vector<SDL_Surface*> results;

	C2File c2File(pFilePath);

	int numFrames = c2File.GetFrameCount();

	for (int idx = 0; idx < numFrames; ++idx)
	{
		SDL_Surface* pSurface = SDL_C2GetSurface(c2File, idx);
		results.push_back(pSurface);
	}

	return results;
}

//------------------------------------------------------------------------------

std::vector<SDL_Surface*> SDL_GSLA_Load(const char* pFilePath)
{
	std::vector<SDL_Surface*> results;

	GSLAFile gslaFile(pFilePath);

	int numFrames = gslaFile.GetFrameCount();

	const std::vector<unsigned char*>& c1datas = gslaFile.GetPixelMaps();

	for (int idx = 0; idx < numFrames; ++idx)
	{
		SDL_Surface* pResult = SDL_C1DataToSurface(c1datas[ idx ]);

		// Stuff in the Delay Time
		int delayTime = 4; 	// hard coded for now
		pResult->userdata = (void *)((long long)(delayTime & 0xFFFF));

		results.push_back(pResult);
	}

	return results;
}

//------------------------------------------------------------------------------

SDL_Surface* SDL_C1_Load(const char* pFilePath)
{
	std::vector<unsigned char> bytes;
	SDL_Surface* pResult = nullptr;

	// Read the file into memory
	FILE* pFile = nullptr;
	errno_t err = fopen_s(&pFile, pFilePath, "rb");

	if (0==err)
	{
		fseek(pFile, 0, SEEK_END);
		size_t length = ftell(pFile);	// get file size
		fseek(pFile, 0, SEEK_SET);

		bytes.resize( length );			// make sure buffer is large enough

		// Read in the file
		fread(&bytes[0], sizeof(unsigned char), bytes.size(), pFile);
		fclose(pFile);
	}

	if (0x8000 == bytes.size())
	{
		pResult = SDL_C1DataToSurface(&bytes[0]);
	}

	return pResult;
}
//------------------------------------------------------------------------------

SDL_Surface* SDL_ANM_GetSurface(AnmFile& animFile, int frameNo)
{
	SDL_Surface* pTargetSurface = nullptr;

	const ANM_Palette& clut = animFile.GetPalette();
	const std::vector<unsigned char*>& pPixelMaps = animFile.GetPixelMaps();

	int width  = animFile.GetWidth();
	int height = animFile.GetHeight();

	unsigned char* pRawPixels = new unsigned char[ width * height ];
	memcpy(pRawPixels, pPixelMaps[ frameNo ], width * height);

	pTargetSurface = SDL_CreateRGBSurfaceWithFormatFrom(
		pRawPixels, width, height,
		8, width, SDL_PIXELFORMAT_INDEX8);

	SDL_Palette *pPalette = SDL_AllocPalette(clut.iNumColors);

	// ANM Colors to SDL Colors
	for (int idx = 0; idx < clut.iNumColors; ++idx)
	{
		ANM_Color inColor = clut.pColors[ idx ];
		SDL_Color outColor;
		outColor.r = inColor.r;
		outColor.g = inColor.g;
		outColor.b = inColor.b;
		outColor.a = inColor.a;

		SDL_SetPaletteColors(pPalette, (const SDL_Color *)&outColor, idx, 1);
	}

	SDL_SetSurfacePalette(pTargetSurface, pPalette);

	// Stuff in the Delay Time
	int delayTime = 4; 	// hard coded for now
	pTargetSurface->userdata = (void *)((long long)(delayTime & 0xFFFF));

	return pTargetSurface;
}


//------------------------------------------------------------------------------
//
// Import a Deluxe animate file as a bunch of SDL Surfaces
//
std::vector<SDL_Surface*> SDL_ANM_Load(const char* pFilePath)
{
	std::vector<SDL_Surface*> results;

	AnmFile animFile(pFilePath);

	int numFrames = animFile.GetFrameCount();

	for (int idx = 0; idx < numFrames; ++idx)
	{
		SDL_Surface* pSurface = SDL_ANM_GetSurface(animFile, idx);
		results.push_back(pSurface);
	}

	return results;
}
//------------------------------------------------------------------------------

