// ImageDocument Class Implementation

#include "imagedoc.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include "log.h"
#include "libimagequant.h"

#include <map>

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

// Statics
int ImageDocument::s_uniqueId = 0;

// Prototype for helper function, that should live in a some sort of helper
// module, but so far does not
GLuint
SDL_GL_LoadTexture(SDL_Surface * surface, GLfloat * texcoord);

//------------------------------------------------------------------------------

ImageDocument::ImageDocument(std::string filename, std::string pathname, SDL_Surface *pImage)
	: m_filename(filename)
	, m_pathname(pathname)
	, m_pSurface( pImage )
	, m_zoom(1)
	, m_targetImage(0)
	, m_pTargetSurface(nullptr)
	, m_numTargetColors(16)
	, m_bOpen(true)
{
	m_image = SDL_GL_LoadTexture(pImage, m_image_uv);

	m_width  = pImage->w;
	m_height = pImage->h;

	// If the image is small, automatically make it a little bigger
	if (m_width < 640)
	{
		m_zoom = 2;

		if (m_width <= 160)
		{
			m_zoom = 4;
		}
	}

	// Assign a unique Window Name
	m_windowName = filename + "##" + std::to_string(s_uniqueId++);

	m_numSourceColors = CountUniqueColors();
}

ImageDocument::~ImageDocument()
{
	// unregister / free the m_image
	if (m_image)
	{
		glDeleteTextures(1, &m_image);
		m_image = 0;
	}
	// unregister / free the m_pSurface
	if (m_pSurface)
	{
		SDL_FreeSurface(m_pSurface);
		m_pSurface = nullptr;
	}
}
#if 0
void PutPixel32_nolock(SDL_Surface * surface, int x, int y, Uint32 color)
{
    Uint8 * pixel = (Uint8*)surface->pixels;
    pixel += (y * surface->pitch) + (x * sizeof(Uint32));
    *((Uint32*)pixel) = color;
}

void PutPixel24_nolock(SDL_Surface * surface, int x, int y, Uint32 color)
{
    Uint8 * pixel = (Uint8*)surface->pixels;
    pixel += (y * surface->pitch) + (x * sizeof(Uint8) * 3);
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    pixel[0] = (color >> 24) & 0xFF;
    pixel[1] = (color >> 16) & 0xFF;
    pixel[2] = (color >> 8) & 0xFF;
#else
    pixel[0] = color & 0xFF;
    pixel[1] = (color >> 8) & 0xFF;
    pixel[2] = (color >> 16) & 0xFF;
#endif
}

void PutPixel16_nolock(SDL_Surface * surface, int x, int y, Uint32 color)
{
    Uint8 * pixel = (Uint8*)surface->pixels;
    pixel += (y * surface->pitch) + (x * sizeof(Uint16));
    *((Uint16*)pixel) = color & 0xFFFF;
}

void PutPixel8_nolock(SDL_Surface * surface, int x, int y, Uint32 color)
{
    Uint8 * pixel = (Uint8*)surface->pixels;
    pixel += (y * surface->pitch) + (x * sizeof(Uint8));
    *pixel = color & 0xFF;
}

void PutPixel32(SDL_Surface * surface, int x, int y, Uint32 color)
{
    if( SDL_MUSTLOCK(surface) )
        SDL_LockSurface(surface);
    PutPixel32_nolock(surface, x, y, color);
    if( SDL_MUSTLOCK(surface) )
        SDL_UnlockSurface(surface);
}

void PutPixel24(SDL_Surface * surface, int x, int y, Uint32 color)
{
    if( SDL_MUSTLOCK(surface) )
        SDL_LockSurface(surface);
    PutPixel24_nolock(surface, x, y, color);
    if( SDL_MUSTLOCK(surface) )
        SDL_LockSurface(surface);
}

void PutPixel16(SDL_Surface * surface, int x, int y, Uint32 color)
{
    if( SDL_MUSTLOCK(surface) )
        SDL_LockSurface(surface);
    PutPixel16_nolock(surface, x, y, color);
    if( SDL_MUSTLOCK(surface) )
        SDL_UnlockSurface(surface);
}

void PutPixel8(SDL_Surface * surface, int x, int y, Uint32 color)
{
    if( SDL_MUSTLOCK(surface) )
        SDL_LockSurface(surface);
    PutPixel8_nolock(surface, x, y, color);
    if( SDL_MUSTLOCK(surface) )
        SDL_UnlockSurface(surface);
}
#endif

//------------------------------------------------------------------------------

int ImageDocument::CountUniqueColors()
{
    SDL_Surface *pImage = SDL_CreateRGBSurface(SDL_SWSURFACE, m_width, m_height,
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
		SDL_GetSurfaceBlendMode(m_pSurface, &saved_mode);
		SDL_SetSurfaceBlendMode(m_pSurface, SDL_BLENDMODE_NONE);

		/* Copy the surface into the GL texture image */
		area.x = 0;
		area.y = 0;
		area.w = m_pSurface->w;
		area.h = m_pSurface->h;
		SDL_BlitSurface(m_pSurface, &area, pImage, &area);

		/* Restore the alpha blending attributes */
		SDL_SetSurfaceBlendMode(m_pSurface, saved_mode);
	}

    if( SDL_MUSTLOCK(pImage) )
        SDL_LockSurface(pImage);

	std::map<Uint32,Uint32> histogram;

	for (int y = 0; y < m_pSurface->h; ++y)
	{
		for (int x = 0; x < m_pSurface->w; ++x)
		{
			Uint8 * pixel = (Uint8*)pImage->pixels;
			pixel += (y * pImage->pitch) + (x * sizeof(Uint32));

			Uint32 color = *((Uint32*)pixel);

			histogram[ color ] = 1;
		}
	}

    if( SDL_MUSTLOCK(pImage) )
        SDL_UnlockSurface(pImage);

    SDL_FreeSurface(pImage);     /* No longer needed */

	return (int)histogram.size();
}

//------------------------------------------------------------------------------

void ImageDocument::Render()
{
	// Number of Colors
	// Dithering
	// Quality
	// Posterize
	// Force Target Palette

	ImTextureID tex_id = (ImTextureID)((size_t) m_image ); 
	ImVec2 uv0 = ImVec2(m_image_uv[0],m_image_uv[1]);
	ImVec2 uv1 = ImVec2(m_image_uv[2],m_image_uv[3]);

	ImGuiStyle& style = ImGui::GetStyle();

	float padding_w = (style.WindowPadding.x + style.FrameBorderSize + style.ChildBorderSize) * 2.0f;
	float padding_h = (style.WindowPadding.y + style.FrameBorderSize + style.ChildBorderSize) * 2.0f;
	padding_h += 48.0f;

	ImGui::SetNextWindowSize(ImVec2((m_width*m_zoom)+padding_w, (m_height*m_zoom)+padding_h), ImGuiCond_FirstUseEver);

	//ImGui::Begin(m_windowName.c_str(),&m_bOpen,ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_HorizontalScrollbar);
	ImGui::Begin(m_windowName.c_str(),&m_bOpen, ImGuiWindowFlags_NoScrollbar);

	ImGui::Text("%d Colors", m_numSourceColors);
	ImGui::SameLine();
	ImGui::Text("%d x %d Pixels",m_width,m_height);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(1);
	ImGui::InputInt("", &m_zoom);
	if (m_zoom < 1) m_zoom = 1;
	if (m_zoom >16) m_zoom = 16;
	ImGui::SameLine(); ImGui::Text("%dx zoom", m_zoom);

	ImGui::SameLine();

	if (ImGui::Button("Quant"))
	{
		//$$TODO - free the target texture
		Quant();
	}

	// Width and Height here needs to be based on the parent Window

	ImVec2 window_size = ImGui::GetWindowSize();
	window_size.x -= padding_w;
	window_size.y -= padding_h;
	ImGui::BeginChild("Source", ImVec2(window_size.x,
									   window_size.y ), false,
						  ImGuiWindowFlags_NoMove |
						  ImGuiWindowFlags_HorizontalScrollbar |
						  ImGuiWindowFlags_AlwaysAutoResize);


		ImGui::Image(tex_id, ImVec2((float)m_width*m_zoom, (float)m_height*m_zoom), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));

		if (m_targetImage)
		{
			ImTextureID target_tex_id = (ImTextureID)((size_t) m_targetImage ); 

			ImGui::SameLine();
			ImGui::Image(target_tex_id, ImVec2((float)m_width*m_zoom, (float)m_height*m_zoom), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
		}

//-----------------------------  Hand and Pan ----------------------------------
		static bool bPanActive = false;

		// Show the Hand Cursor
		if (ImGui::IsWindowHovered() || bPanActive)
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

		// Scroll the window around using mouse
		ImGuiIO& io = ImGui::GetIO();

		static float OriginalScrollY = 0.0f;
		static float OriginalScrollX = 0.0f;

		if (io.MouseClicked[0] && ImGui::IsWindowHovered())
		{
			OriginalScrollX = ImGui::GetScrollX();
			OriginalScrollY = ImGui::GetScrollY();
			bPanActive = true;
		}

		if (io.MouseDown[0] && bPanActive)
		{
			float dx = io.MouseClickedPos[0].x - io.MousePos.x;
			float dy = io.MouseClickedPos[0].y - io.MousePos.y;

			ImGui::SetScrollX(OriginalScrollX + dx);
			ImGui::SetScrollY(OriginalScrollY + dy);
		}
		else
		{
			bPanActive = false;
		}
//-----------------------------  Hand and Pan ----------------------------------


	ImGui::EndChild();

	ImGui::End();
}

//------------------------------------------------------------------------------

void ImageDocument::Quant()
{
	// Do an actual color reduction on the source Image
	// then generate an OGL Texture
	LOG("Color Reduce - Go!\n");

    unsigned int width=(unsigned int)m_width;
	unsigned int height=(unsigned int)m_height;
    unsigned char *raw_rgba_pixels = nullptr;

	//-----------------------------------------------

    SDL_Surface *pImage = SDL_CreateRGBSurface(SDL_SWSURFACE, m_width, m_height,
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
		return;
	}

	// Copy the source image into 32bpp format
	{
		/* Save the alpha blending attributes */
		SDL_Rect area;
		SDL_BlendMode saved_mode;
		SDL_GetSurfaceBlendMode(m_pSurface, &saved_mode);
		SDL_SetSurfaceBlendMode(m_pSurface, SDL_BLENDMODE_NONE);

		/* Copy the surface into the GL texture image */
		area.x = 0;
		area.y = 0;
		area.w = m_pSurface->w;
		area.h = m_pSurface->h;
		SDL_BlitSurface(m_pSurface, &area, pImage, &area);

		/* Restore the alpha blending attributes */
		SDL_SetSurfaceBlendMode(m_pSurface, saved_mode);
	}

    if( SDL_MUSTLOCK(pImage) )
        SDL_LockSurface(pImage);

	raw_rgba_pixels = (unsigned char*) pImage->pixels;

	//-----------------------------------------------

    liq_attr *handle = liq_attr_create();

	liq_set_max_colors(handle, 16);
	liq_set_speed(handle, 1);   // 1-10  (1 best quality)
	liq_set_min_posterization(handle, 4);

	//$$JGA Fixed Colors can be added to the input_image
	//$$JGA which is going to be sweet
    liq_image *input_image = liq_image_create_rgba(handle, raw_rgba_pixels, width, height, 0);
    // You could set more options here, like liq_set_quality
    liq_result *quantization_result;
    if (liq_image_quantize(input_image, handle, &quantization_result) != LIQ_OK) {
        LOG("Quantization failed\n");
		return;
    }

    // Use libimagequant to make new image pixels from the palette

    size_t pixels_size = width * height;
    unsigned char *raw_8bit_pixels = (unsigned char*)malloc(pixels_size);
    liq_set_dithering_level(quantization_result, 1.0);  // 0.0->1.0
//	liq_set_output_gamma(quantization_result, 1.0);

    liq_write_remapped_image(quantization_result, input_image, raw_8bit_pixels, pixels_size);
    const liq_palette *palette = liq_get_palette(quantization_result);

	// Convert Results into a Surface ------------------------------------------

	SDL_Surface *pTargetSurface = SDL_CreateRGBSurfaceWithFormatFrom(
									raw_8bit_pixels, width, height,
									8, width, SDL_PIXELFORMAT_INDEX8);
	SDL_Palette *pPalette = SDL_AllocPalette( 16 );

	SDL_SetPaletteColors(pPalette, (const SDL_Color*)palette->entries, 0, 16);

	SDL_SetSurfacePalette(pTargetSurface, pPalette);

	GLfloat about_image_uv[4];
	m_targetImage = SDL_GL_LoadTexture(pTargetSurface, about_image_uv);

    m_pTargetSurface = pTargetSurface;

	// Free up the memory used by libquant -------------------------------------
    liq_result_destroy(quantization_result); // Must be freed only after you're done using the palette
    liq_image_destroy(input_image);
    liq_attr_destroy(handle);

    free(raw_8bit_pixels);

	//-----------------------------------------------

    if( SDL_MUSTLOCK(pImage) )
        SDL_UnlockSurface(pImage);

    SDL_FreeSurface(pImage);     /* No longer needed */

}

//------------------------------------------------------------------------------

