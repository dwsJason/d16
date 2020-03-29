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
#include <vector>

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
	, m_iDither(50)
	, m_iPosterize(ePosterize444)
	, m_bOpen(true)
	, m_bPanActive(false)
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

	// Initialize Target Colors
	for (int idx = 0; idx < 16; ++idx)
	{
		m_targetColors.push_back(ImVec4(idx/15.0f,idx/15.0f,idx/15.0f, 1.0f));
		m_bLocks.push_back(0);
	}

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

//
// $$JGA FIXME - NO STATIC STATE VARIABLES, WE SUPPORT MULTIPLE DOCMENTS
//
void ImageDocument::Render()
{
	// Number of Colors
	// Dithering
	// Quality
	// Posterize
	// Force Target Palette
	const float TOOLBAR_HEIGHT = 72.0f;

	ImTextureID tex_id = (ImTextureID)((size_t) m_image ); 
	ImVec2 uv0 = ImVec2(m_image_uv[0],m_image_uv[1]);
	ImVec2 uv1 = ImVec2(m_image_uv[2],m_image_uv[3]);

	ImGuiStyle& style = ImGui::GetStyle();

	float padding_w = (style.WindowPadding.x + style.FrameBorderSize + style.ChildBorderSize) * 2.0f;
	float padding_h = (style.WindowPadding.y + style.FrameBorderSize + style.ChildBorderSize) * 2.0f;
	padding_h += TOOLBAR_HEIGHT;

	ImGui::SetNextWindowSize(ImVec2((m_width*m_zoom)+padding_w, (m_height*m_zoom)+padding_h), ImGuiCond_FirstUseEver);

	ImGui::Begin(m_windowName.c_str(),&m_bOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);

	bool bHasFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

//	ImGui::Text("Source:");
//	ImGui::SameLine();

	ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1.0f),"%d Colors", m_numSourceColors);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1.0f),"%d x %d Pixels",m_width,m_height);
	//ImGui::SameLine();
	//ImGui::SetNextItemWidth(1);
	//ImGui::InputInt("##zoom", &m_zoom);

	if (m_zoom < 1) m_zoom = 1;
	if (m_zoom >16) m_zoom = 16;
	ImGui::SameLine(); ImGui::TextColored(ImVec4(0.7f,1.0f,0.7f,1.0f),"%dx zoom", m_zoom);

	bool bHasTip = false;

	for (int idx = 0; idx < m_bLocks.size(); ++idx)
	{
		ImGui::SameLine(320.0f + (idx * 20.0f));
		std::string id = "##" + std::to_string(idx);
		ImGui::Checkbox(id.c_str(), (bool*)&m_bLocks[idx]);

		if (ImGui::BeginPopupContextItem()) // <-- This is using IsItemHovered()
		{
		    if (ImGui::MenuItem("Lock All"))
			{
				for (int lockIndex = 0; lockIndex < m_bLocks.size(); ++lockIndex)
					m_bLocks[lockIndex] = true;
			}
		    if (ImGui::MenuItem("Unlock All"))
			{
				for (int lockIndex = 0; lockIndex < m_bLocks.size(); ++lockIndex)
					m_bLocks[lockIndex] = false;
			}
		    if (ImGui::MenuItem("Invert All"))
			{
				for (int lockIndex = 0; lockIndex < m_bLocks.size(); ++lockIndex)
					m_bLocks[lockIndex] = !m_bLocks[lockIndex];
			}
		    ImGui::EndPopup();
		}

		if (ImGui::IsItemHovered() && !bHasTip)
		{
			bHasTip = true;  // work around for me putting the boxes too close togeher

			{
				ImGui::BeginTooltip();
				if (m_bLocks[idx])
				{
					ImGui::Text("LOCKED");
					ImGui::Text("keep this color");
				}
				else
				{
					ImGui::Text("Unlocked");
					ImGui::Text("Q will choose a color");
				}
				ImGui::EndTooltip();
			}
		}
	}


	// Second Line of Toolbar
	//--------------------------------------------------------------------------
	ImGui::Text("Target:");
	ImGui::SameLine();

	ImGui::SetNextItemWidth(56);
	ImGui::Combo("##Posterize", &m_iPosterize, "444\0" "555\0" "888\0\0");

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text("RGB Posterize");
		ImGui::Text("Color Resolution");
		ImGui::EndTooltip();
	}

	ImGui::SameLine();

	ImGui::SetNextItemWidth(32);
	ImGui::DragInt("##Dither", &m_iDither, 1, 0, 100, "%d%%"); ImGui::SameLine();
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text("Dither (drag)");
		ImGui::EndTooltip();
	}

	ImGui::SameLine();

	if (ImGui::Button("Quantize Image"))
	{
		// Make it 16 colors
		Quant();
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text("Reduce / Remap Colors");
		ImGui::EndTooltip();
	}

	ImVec2 buttonSize = ImVec2(20,20);

	for (int idx = 0; idx < m_targetColors.size(); ++idx)
	{
		ImGui::SameLine(320.0f + (idx * buttonSize.x));

		std::string colorId = m_filename + "##" + std::to_string(idx);
		bool open_popup = ImGui::ColorButton(colorId.c_str(), m_targetColors[idx],
						   ImGuiColorEditFlags_NoLabel |
						   ImGuiColorEditFlags_NoAlpha |
						   ImGuiColorEditFlags_NoBorder, buttonSize );

		static ImVec4 backup_color;

		std::string pickerid = "picker##" + std::to_string(idx);

		if (open_popup)
		{
            ImGui::OpenPopup(pickerid.c_str());
            backup_color = m_targetColors[ idx ];
		}
        if (ImGui::BeginPopup(pickerid.c_str()))
		{
            ImGui::Text("Edit Target Color");
            ImGui::Separator();
			ImGui::ColorPicker3("##Picker", (float *)&m_targetColors[idx], ImGuiColorEditFlags_NoAlpha);

            ImGui::SameLine();

            ImGui::BeginGroup(); // Lock X position

            ImGui::Text("Current");
            ImGui::ColorButton("##current", m_targetColors[ idx ], ImGuiColorEditFlags_NoPicker, ImVec2(60,40));
            ImGui::Text("Previous");
            if (ImGui::ColorButton("##previous", backup_color, ImGuiColorEditFlags_NoPicker, ImVec2(60,40)))
                m_targetColors[idx] = backup_color;
            ImGui::Separator();

			#if 0 // put some sort of palette here?
            ImGui::Text("Palette");
            for (int n = 0; n < IM_ARRAYSIZE(saved_palette); n++)
            {
                ImGui::PushID(n);
                if ((n % 8) != 0)
                    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);
                if (ImGui::ColorButton("##palette", saved_palette[n], ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip, ImVec2(20,20)))
                    color = ImVec4(saved_palette[n].x, saved_palette[n].y, saved_palette[n].z, color.w); // Preserve alpha!

                // Allow user to drop colors into each palette entry
                // (Note that ColorButton is already a drag source by default, unless using ImGuiColorEditFlags_NoDragDrop)
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
                        memcpy((float*)&saved_palette[n], payload->Data, sizeof(float) * 3);
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
                        memcpy((float*)&saved_palette[n], payload->Data, sizeof(float) * 4);
                    ImGui::EndDragDropTarget();
                }

                ImGui::PopID();
            }
			#endif

            ImGui::EndGroup();
			ImGui::Separator();
			if (ImGui::Button("Okay"))
				ImGui::CloseCurrentPopup();
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
                m_targetColors[idx] = backup_color;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

	}
	//ImGui::NewLine();

	//--------------------------------------------------------------------------

	// Width and Height here needs to be based on the parent Window

	ImVec2 window_size = ImGui::GetWindowSize();
	window_size.x -= padding_w;
	window_size.y -= padding_h;
	window_size.x += style.ChildBorderSize*2.0f;
	window_size.y += style.ChildBorderSize*2.0f;

	ImGui::BeginChild("Source", ImVec2(window_size.x,
									   window_size.y ), false,
						  ImGuiWindowFlags_NoMove |
						  ImGuiWindowFlags_HorizontalScrollbar |
						  ImGuiWindowFlags_NoScrollWithMouse |
						  ImGuiWindowFlags_AlwaysAutoResize);


		ImGui::Image(tex_id, ImVec2((float)m_width*m_zoom, (float)m_height*m_zoom), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));

		if (m_targetImage)
		{
			ImTextureID target_tex_id = (ImTextureID)((size_t) m_targetImage ); 

			ImGui::SameLine();
			ImGui::Image(target_tex_id, ImVec2((float)m_width*m_zoom, (float)m_height*m_zoom), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
		}

//-----------------------------  Hand and Pan ----------------------------------

		bHasFocus = bHasFocus && ImGui::IsWindowHovered();

		// Show the Hand Cursor
		if (bHasFocus || m_bPanActive)
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

		// Scroll the window around using mouse
		ImGuiIO& io = ImGui::GetIO();

		static float OriginalScrollY = 0.0f;
		static float OriginalScrollX = 0.0f;

		if (bHasFocus)
		{
			ImVec2 winPos = ImGui::GetWindowPos();
			float scrollX = ImGui::GetScrollX();
			float scrollY = ImGui::GetScrollY();
			float cursorX = io.MousePos.x - winPos.x;
			float cursorY = io.MousePos.y - winPos.y;

			// Which pixel on the canvas is the mouse over?
			float px = cursorX/m_zoom + scrollX/m_zoom;
			float py = cursorY/m_zoom + scrollY/m_zoom;
			bool bZoom = false;

			if (io.MouseWheel < 0.0f)
			{
				m_zoom--;
				if (m_zoom < 1)
				{
					m_zoom = 1;
				}
				else
				{
					bZoom = true;
				}
			}
			else if (io.MouseWheel > 0.0f)
			{
				m_zoom++;
				if (m_zoom > 16)
				{
					m_zoom = 16;
				}
				else
				{
					bZoom = true;
				}
			}

			if (bZoom)
			{
				// New Scroll Position based on the new zoom
				scrollX = -(cursorX/m_zoom - px) * m_zoom;
				scrollY = -(cursorY/m_zoom - py) * m_zoom;

				ImGui::SetScrollX(scrollX);
				ImGui::SetScrollY(scrollY);

				if (m_bPanActive)
				{
					// pretend you let up off the mouse button, and clicked
					// again for the pan
					//OriginalScrollX = scrollX;
					//OriginalScrollY = scrollY;
				}
			}
		}

		if (io.MouseClicked[0] && bHasFocus)
		{
			OriginalScrollX = ImGui::GetScrollX();
			OriginalScrollY = ImGui::GetScrollY();
			m_bPanActive = true;
		}

		if (io.MouseDown[0] && m_bPanActive)
		{
			float dx = io.MouseClickedPos[0].x - io.MousePos.x;
			float dy = io.MouseClickedPos[0].y - io.MousePos.y;

			ImGui::SetScrollX(OriginalScrollX + dx);
			ImGui::SetScrollY(OriginalScrollY + dy);
		}
		else
		{
			m_bPanActive = false;
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

