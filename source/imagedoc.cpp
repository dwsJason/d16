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
#include "limage.h"
#include "avir.h"
#include "lancir.h"

#include "toolbar.h"

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
	, m_bShowResizeUI(false)
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

	for (int y = 0; y < pImage->h; ++y)
	{
		for (int x = 0; x < pImage->w; ++x)
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

//	bool bHasFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

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


		// Allow user to drop colors into each palette entry
		// (Note that ColorButton is already a drag source by default, unless using ImGuiColorEditFlags_NoDragDrop)
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
				memcpy((float*)&m_targetColors[idx], payload->Data, sizeof(float) * 3);
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
				memcpy((float*)&m_targetColors[idx], payload->Data, sizeof(float) * 4);

			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("xPalette16"))
			{
				memcpy((float*)&m_targetColors[0], payload->Data, sizeof(float) * 4 * 16);
			}

			ImGui::EndDragDropTarget();
		}

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

			#if 0 // put some kind of shared palette here?
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

//------------------------------------------------------------------------------
// Render Images

		ImGui::Image(tex_id, ImVec2((float)m_width*m_zoom, (float)m_height*m_zoom), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));

//----------------- Source Image Context Menu ----------------------------------

		bool bOpenResizeModal = false;

		if (ImGui::BeginPopupContextItem("Source")) // <-- This is using IsItemHovered()
		{
		    if (ImGui::MenuItem("Resize Image"))
			{
				bOpenResizeModal = true;
			}
		    ImGui::EndPopup();
		}

		if (m_targetImage)
		{
			ImTextureID target_tex_id = (ImTextureID)((size_t) m_targetImage ); 

			ImGui::SameLine();
			ImGui::Image(target_tex_id, ImVec2((float)m_width*m_zoom, (float)m_height*m_zoom), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));

//-------------------  Target Image Context Menu -------------------------------

			if (ImGui::BeginPopupContextItem("Target"))
			{
				if (ImGui::MenuItem("Keep Image"))
				{
					SetDocumentSurface( SDL_SurfaceToRGBA(m_pTargetSurface) );
				}
				ImGui::EndPopup();
			}
		}

//-------------------------------- Resize Image --------------------------------

		// Glue for the Toolbar button
		if (ImGui::IsWindowFocused())
		if (eResizeImage == Toolbar::GToolbar->GetCurrentMode())
		{
			bOpenResizeModal = true;
			Toolbar::GToolbar->SetPreviousMode();
		}

		if (bOpenResizeModal)
		{
			m_bShowResizeUI = true;
			ImGui::OpenPopup("Resize Image##modal");
		}

		if (m_bShowResizeUI)
		{
			if (ImGui::BeginPopupModal("Resize Image##modal", &m_bShowResizeUI,
						 ImGuiWindowFlags_AlwaysAutoResize))
			{
				RenderResizeDialog();
				ImGui::EndPopup();
			}

		}
		else
		{
//---------------------------------- Pan Image ---------------------------------
			RenderPanAndZoom();
		}

	ImGui::EndChild();

	ImGui::End();
}

//------------------------------------------------------------------------------
void ImageDocument::RenderPanAndZoom()
{
	bool bHasFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

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

	SDL_Surface *pImage = SDL_SurfaceToRGBA(m_pSurface);

	if (nullptr == pImage)
	{
		return;
	}

    if( SDL_MUSTLOCK(pImage) )
        SDL_LockSurface(pImage);

	raw_rgba_pixels = (unsigned char*) pImage->pixels;

	//-----------------------------------------------

    liq_attr *handle = liq_attr_create();

	liq_set_max_colors(handle, 16);
	liq_set_speed(handle, 1);   // 1-10  (1 best quality)

	int min_posterize = 4;
	switch (m_iPosterize)
	{
	case ePosterize444:
		min_posterize = 4;
		break;
	case ePosterize555:
		min_posterize = 3;
		break;
	case ePosterize888:
		min_posterize = 0;
		break;
	}

	liq_set_min_posterization(handle, min_posterize);

	//$$JGA Fixed Colors can be added to the input_image
	//$$JGA which is going to be sweet
    liq_image *input_image = liq_image_create_rgba(handle, raw_rgba_pixels, width, height, 0);

	// Add the fixed colors
	for (int idx = 0; idx < m_bLocks.size(); ++idx)
	{
		if (m_bLocks[idx])
		{
			liq_color color;
			color.r = (unsigned char) (m_targetColors[idx].x * 255.0f);
			color.g = (unsigned char) (m_targetColors[idx].y * 255.0f); 
			color.b = (unsigned char) (m_targetColors[idx].z * 255.0f); 
			color.a = (unsigned char) (m_targetColors[idx].w * 255.0f);
			
			// Add a Color
			liq_image_add_fixed_color(input_image, color);
		}
	}

	// You could set more options here, like liq_set_quality
    liq_result *quantization_result;
    if (liq_image_quantize(input_image, handle, &quantization_result) != LIQ_OK) {
        LOG("Quantization failed\n");
		return;
    }

    // Use libimagequant to make new image pixels from the palette

    size_t pixels_size = width * height;
    unsigned char *raw_8bit_pixels = (unsigned char*)malloc(pixels_size);
	liq_set_dithering_level(quantization_result, m_iDither / 100.0f);  // 0.0->1.0
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

	// Put the result colors back up in the tray, so we can see them
	{
		// take advantage, I know the locked colors all get grouped on the end of the result
				// count the number of locked colors
		int numLocked = 0;
		for (int idx = 0; idx < m_bLocks.size(); ++idx)
		{
			if (m_bLocks[idx]) numLocked++;
		}

		// locked colors start at this index
		int lockedBaseIndex = (int)m_bLocks.size() - numLocked;

		int lockedIndex = 0;
		int palIndex = 0;

		for (int idx = 0; idx < m_targetColors.size(); ++idx)
		{
			liq_color color;

			if (m_bLocks[idx])
			{
				color = palette->entries[lockedBaseIndex + lockedIndex];
				lockedIndex++;
			}
			else
				color = palette->entries[ palIndex++ ];

			m_targetColors[ idx ].x = color.r / 255.0f;
			m_targetColors[ idx ].y = color.g / 255.0f;
			m_targetColors[ idx ].z = color.b / 255.0f;
			m_targetColors[ idx ].w = color.a / 255.0f;
		}
	}

	GLfloat about_image_uv[4];
	m_targetImage = SDL_GL_LoadTexture(pTargetSurface, about_image_uv);

    m_pTargetSurface = pTargetSurface;

	// Free up the memory used by libquant -------------------------------------
    liq_result_destroy(quantization_result); // Must be freed only after you're done using the palette
    liq_image_destroy(input_image);
    liq_attr_destroy(handle);

	// SDL_CreateRGBSurfaceWithFormatFrom, makes you manage the raw pixels buffer
	// instead of make a copy of it, so I'm supposed to free it manually, after
	// the surface is free!
    //free(raw_8bit_pixels);  // The surface owns these now

	// Tell SDL to free it for me
	//pTargetSurface->flags &= ~SDL_PREALLOC;

	//-----------------------------------------------

    if( SDL_MUSTLOCK(pImage) )
        SDL_UnlockSurface(pImage);

    SDL_FreeSurface(pImage);     /* No longer needed */

}

//------------------------------------------------------------------------------

void ImageDocument::RenderResizeDialog()
{
	int iOriginalWidth  = m_width;
	int iOriginalHeight = m_height;
	static int iNewWidth = 320;
	static int iNewHeight = 200;
	static bool bMaintainAspectRatio = false;
	static float fAspectRatio = 1.0f;

	ImGui::SameLine(ImGui::GetWindowWidth()/4);

	if (ImGui::Checkbox("Maintain Aspect Ratio", &bMaintainAspectRatio))
	{
		if (bMaintainAspectRatio)
		{
			fAspectRatio = (float)iNewWidth/(float)iNewHeight;
		}
	}

	ImGui::NewLine();

	ImGui::Text("New Width:");  ImGui::SameLine(100); ImGui::SetNextItemWidth(128);
	if (ImGui::InputInt("Pixels##Width", &iNewWidth))
	{
		if (iNewWidth < 1) iNewWidth = 1;
		if (bMaintainAspectRatio)
			iNewHeight = (int)((((float)iNewWidth) / fAspectRatio) + 0.5f);
		if (iNewHeight < 1) iNewHeight = 1;
	}

	ImGui::Text("New Height:");  ImGui::SameLine(100); ImGui::SetNextItemWidth(128);
	if (ImGui::InputInt("Pixels##Height", &iNewHeight))
	{
		if (iNewHeight < 1) iNewHeight = 1;
		if (bMaintainAspectRatio)
			iNewWidth = (int)((((float)iNewHeight) * fAspectRatio) + 0.5f);
		if (iNewWidth < 1) iNewWidth = 1;
	}

	ImGui::NewLine();

	if(ImGui::Button("Original Size"))
	{
		iNewWidth = iOriginalWidth;
		iNewHeight = iOriginalHeight;
		if (bMaintainAspectRatio)
			fAspectRatio = (float)iNewWidth/(float)iNewHeight;
	}
	ImGui::SameLine();
	if(ImGui::Button("Half"))
	{
		iNewWidth>>=1;
		iNewHeight>>=1;
		if (iNewWidth < 1) iNewWidth = 1;
		if (bMaintainAspectRatio)
			iNewHeight = (int)((((float)iNewWidth) / fAspectRatio) + 0.5f);
		if (iNewHeight < 1) iNewHeight = 1;
	}
	ImGui::SameLine();
	if(ImGui::Button("Double"))
	{
		iNewWidth<<=1;
		iNewHeight <<= 1;
		if (bMaintainAspectRatio)
			iNewHeight = (int)((((float)iNewWidth) / fAspectRatio) + 0.5f);
		if (iNewHeight < 1) iNewHeight = 1;
	}
	ImGui::SameLine();
	if(ImGui::Button("320x200"))
	{
		iNewWidth  = 320;
		iNewHeight = 200;
		if (bMaintainAspectRatio)
		{
			fAspectRatio = (float)iNewWidth/(float)iNewHeight;
		}
	}

	ImGui::NewLine();
	ImGui::Separator();
	ImGui::NewLine();

	static int scale_or_crop = 0;

	ImGui::RadioButton("Scale Image", &scale_or_crop, 0); ImGui::SameLine(128);

	const char* items[] = { "Point Sample", "Linear Sample", "Lanczos", "AVIR" };
	static int item_current = eAVIR;
	ImGui::SetNextItemWidth(148);
	ImGui::Combo("##SampleCombo", &item_current, items, IM_ARRAYSIZE(items));

	static bool bDither = false;
	ImGui::NewLine();
	ImGui::SameLine(128);
	ImGui::Checkbox("Dither (AVIR Only)", &bDither);

	ImGui::NewLine();
	ImGui::Separator();
	ImGui::NewLine();

	ImGui::RadioButton("Reposition", &scale_or_crop, 1);

	ImVec2 buttonSize = ImVec2(32,32);

	//  0 1 2
	//  3 4 5
	//  6 7 8
	static int iLitButtonIndex = 4;

	ImVec4 ButtonColor = ImGui::GetStyleColorVec4(ImGuiCol_Button);
	ImVec4 ActiveColor = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);


	// Draw the 9 buttons, and leave our selection lit up
	int index = 0;
	for (int y = 0; y < 3; ++y)
	{
		float xpos = 128.0f;

		for (int x = 0; x < 3; ++x)
		{
			ImGui::SameLine(xpos);

			ImGui::PushID(index);

			ImGui::PushStyleColor(ImGuiCol_Button,
								  iLitButtonIndex==index ?
								  ActiveColor :
								  ButtonColor);

			xpos += (buttonSize.x + 4.0f);
			if (ImGui::Button("", buttonSize))
				iLitButtonIndex = index;

			ImGui::PopStyleColor();
			ImGui::PopID();

			++index;
		}
		ImGui::NewLine();
	}

	//ImGui::NewLine();
	ImGui::Separator();
	ImGui::NewLine();

	ImVec2 okSize = ImVec2(90, 24);
	ImGui::SameLine(96);

	if (ImGui::Button("Ok", okSize))
	{
		if (scale_or_crop)
		{
			// Crop
			// Resize, and justified copy
			CropImage(iNewWidth, iNewHeight, iLitButtonIndex);
		}
		else
		{
			// Scale
			// Resize and Resample
			switch (item_current)
			{
			case ePointSample:
				PointSampleResize(iNewWidth,iNewHeight);
				break;
			case eBilinearSample:
				LinearSampleResize(iNewWidth,iNewHeight);
				break;
			case eLanczos:
				LanczosResize(iNewWidth, iNewHeight);
				break;
			case eAVIR:
				AvirSampleResize(iNewWidth,iNewHeight,bDither);
				break;
			}
		}

		// Put some code here to dispatch the crop/resize
		m_bShowResizeUI = false;
		ImGui::CloseCurrentPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Cancel", okSize))
	{
		m_bShowResizeUI = false;
		ImGui::CloseCurrentPopup();
	}
}

//------------------------------------------------------------------------------
//  iJustify
//
//  0 1 2
//  3 4 5
//  6 7 8
//
void ImageDocument::CropImage(int iNewWidth, int iNewHeight, int iJustify)
{
    SDL_Surface *pImage = SDL_CreateRGBSurface(SDL_SWSURFACE, iNewWidth, iNewHeight,
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
		return;

	/* Save the alpha blending attributes */
	SDL_Rect source_area;
	SDL_Rect dest_area;
	SDL_BlendMode saved_mode;
	SDL_GetSurfaceBlendMode(m_pSurface, &saved_mode);
	SDL_SetSurfaceBlendMode(m_pSurface, SDL_BLENDMODE_NONE);

	/* Copy the surface into the GL texture image */
	source_area.x = 0;
	source_area.y = 0;
	source_area.w = m_pSurface->w;
	source_area.h = m_pSurface->h;
	dest_area.w = iNewWidth;
	dest_area.h = iNewHeight;

	// Left Right Position

	switch (iJustify)
	{
	case eUpperLeft:
	case eCenterLeft:
	case eLowerLeft:
			dest_area.x = 0;
			break;
	case eUpperCenter:
	case eCenterCenter:
	case eLowerCenter:
			dest_area.x = (iNewWidth - m_width)/2;
			break;
	case eUpperRight:
	case eCenterRight:
	case eLowerRight:
			dest_area.x = iNewWidth - m_width;
			break;	
	}

	// Vertical Position
	switch (iJustify)
	{
	case eUpperLeft:
	case eUpperCenter:
	case eUpperRight:
			dest_area.y = 0;
			break;
	case eCenterLeft:
	case eCenterCenter:
	case eCenterRight:
			dest_area.y = (iNewHeight - m_height)/2;
			break;
	case eLowerLeft:
	case eLowerCenter:
	case eLowerRight:
			dest_area.y = iNewHeight - m_height;
			break;	
	}

	SDL_BlitSurface(m_pSurface, &source_area,
					pImage, &dest_area);

	/* Restore the alpha blending attributes */
	SDL_SetSurfaceBlendMode(m_pSurface, saved_mode);


	// Free up the source image, and opengl texture
	SetDocumentSurface( pImage );
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
void ImageDocument::PointSampleResize(int iNewWidth, int iNewHeight)
{
    SDL_Surface *pImage = SDL_CreateRGBSurface(SDL_SWSURFACE, iNewWidth, iNewHeight,
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
		return;

	/* Save the alpha blending attributes */
	SDL_Rect source_area;
	SDL_Rect dest_area;
	SDL_BlendMode saved_mode;
	SDL_GetSurfaceBlendMode(m_pSurface, &saved_mode);
	SDL_SetSurfaceBlendMode(m_pSurface, SDL_BLENDMODE_NONE);

	/* Copy the surface into the GL texture image */
	source_area.x = 0;
	source_area.y = 0;
	source_area.w = m_pSurface->w;
	source_area.h = m_pSurface->h;
	dest_area.x = 0;
	dest_area.y = 0;
	dest_area.w = iNewWidth;
	dest_area.h = iNewHeight;

	SDL_Surface *pSource = SDL_SurfaceToRGBA(m_pSurface);

	SDL_BlitScaled(pSource, &source_area,
					pImage, &dest_area);

	/* Restore the alpha blending attributes */
	SDL_SetSurfaceBlendMode(m_pSurface, saved_mode);

	// Free up the source image, and opengl texture

	if (pSource)
	{
		SDL_FreeSurface(pSource);
	}

	//--------------------------
	SetDocumentSurface( pImage );
}
//------------------------------------------------------------------------------
void ImageDocument::LinearSampleResize(int iNewWidth, int iNewHeight)
{
	SDL_Surface *pSource = SDL_SurfaceToRGBA(m_pSurface);

	if (pSource)
	{
		if( SDL_MUSTLOCK(pSource) )
			SDL_LockSurface(pSource);

		Uint32* pPixels = (Uint32*) malloc(pSource->w * pSource->h * sizeof(Uint32));

		// So do some work here
		for (int y = 0; y < pSource->h; ++y)
		{
			for (int x = 0; x < pSource->w; ++x)
			{
				Uint8 * pixel = (Uint8*)pSource->pixels;
				pixel += (y * pSource->pitch) + (x * sizeof(Uint32));

				Uint32 color = *((Uint32*)pixel);

				// Dump the Colors out into an array
				pPixels[ (y * pSource->w) + x ] = color;
			}
		}

		if( SDL_MUSTLOCK(pSource) )
			SDL_UnlockSurface(pSource);

		// Shuttle us over to the linear image class
		LinearImage sourceImage(pPixels, pSource->w, pSource->h);

		LinearImage* pDestImage = sourceImage.Scale( iNewWidth, iNewHeight );

		SDL_FreeSurface(pSource);
		free(pPixels);
//------------------------------------------
		SDL_Surface *pImage = SDL_CreateRGBSurface(SDL_SWSURFACE, iNewWidth, iNewHeight,
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
			return;

		if( SDL_MUSTLOCK(pImage) )
			SDL_LockSurface(pImage);


		for (int y = 0; y < iNewHeight; ++y)
		{
			for (int x = 0; x < iNewWidth; ++x)
			{
				FloatPixel p = pDestImage->GetPixel(x, y);

				Uint32 pixel = ((Uint32)p.a)&0xFF;
				pixel<<=8;
				pixel |= ((Uint32)p.b)&0xFF;
				pixel<<=8;
				pixel |= ((Uint32)p.g)&0xFF;
				pixel<<=8;
				pixel |= ((Uint32)p.r)&0xFF;

				Uint8* pPixel = (Uint8*)pImage->pixels;
				pPixel += (y*pImage->pitch) + (x * sizeof(Uint32));

				*((Uint32*)pPixel) = pixel;
			}
		}

		if( SDL_MUSTLOCK(pImage) )
			SDL_UnlockSurface(pImage);

		delete pDestImage;
		pDestImage = nullptr;
//------------------------------------------

		SetDocumentSurface( pImage );

	}
}
//------------------------------------------------------------------------------
void ImageDocument::LanczosResize(int iNewWidth, int iNewHeight)
{
	Uint32* pPixels = SDL_SurfaceToUint32Array(m_pSurface);

	if (pPixels)
	{
		avir::CLancIR LanczosResizer;

		Uint32* pNewPixels = new Uint32[ iNewWidth * iNewHeight ];

		LanczosResizer.resizeImage<unsigned char>((unsigned char*)pPixels,
										   m_width, m_height,
										   sizeof(Uint32)*m_width,  		// $$JGA Since this takes a stride, we might be able to pass SDL Surface directly in
											(unsigned char*)pNewPixels,
												  iNewWidth, iNewHeight,
												  sizeof(Uint32));  //RGBA 8888


		SDL_Surface* pSurface = SDL_SurfaceFromRawRGBA(pNewPixels, iNewWidth, iNewHeight);

		delete[] pNewPixels;
		delete[] pPixels;

		if (pSurface)
		{
			SetDocumentSurface( pSurface );
		}
	}
}

//------------------------------------------------------------------------------
void ImageDocument::AvirSampleResize(int iNewWidth, int iNewHeight, bool bDither)
{
	Uint32* pPixels = SDL_SurfaceToUint32Array(m_pSurface);

	if (pPixels)
	{
		Uint32* pNewPixels = new Uint32[ iNewWidth * iNewHeight ];

		if (bDither)
		{
			typedef avir::fpclass_def< float, float,
				avir::CImageResizerDithererErrdINL< float > > fpclass_dith;

			avir::CImageResizer< fpclass_dith > DitherResizer( 8 );

			DitherResizer.resizeImage<Uint8,Uint8>((Uint8*)pPixels, m_width, m_height,
												 m_width*sizeof(Uint32),
												 (Uint8*)pNewPixels,
												 iNewWidth, iNewHeight,
												 sizeof(Uint32),  // RGBA 8888
												 0);
		}
		else
		{
			avir::CImageResizer<> AvirResizer(8);

			AvirResizer.resizeImage<Uint8,Uint8>((Uint8*)pPixels, m_width, m_height,
												 m_width*sizeof(Uint32),
												 (Uint8*)pNewPixels,
												 iNewWidth, iNewHeight,
												 sizeof(Uint32),  // RGBA 8888
												 0);
		}

		SDL_Surface* pSurface = SDL_SurfaceFromRawRGBA(pNewPixels, iNewWidth, iNewHeight);

		delete[] pNewPixels;
		delete[] pPixels;

		if (pSurface)
		{
			SetDocumentSurface( pSurface );
		}
	}
}
//------------------------------------------------------------------------------
SDL_Surface* ImageDocument::SDL_SurfaceToRGBA(SDL_Surface* pSurface)
{
	SDL_PixelFormat format;

	memset(&format, 0, sizeof(SDL_PixelFormat));

	format.format = SDL_PIXELFORMAT_RGBA8888;
	format.BitsPerPixel = 32;
	format.BytesPerPixel = 4;

#if SDL_BYTEORDER == SDL_LIL_ENDIAN     /* OpenGL RGBA masks */
    format.Rmask = 0x000000FF;
    format.Gmask = 0x0000FF00;
	format.Bmask = 0x00FF0000;
	format.Amask = 0xFF000000;
#else
	format.Rmask = 0xFF000000;
	format.Gmask = 0x00FF0000;
	format.Bmask = 0x0000FF00;
	format.Amask = 0x000000FF;
#endif

	return SDL_ConvertSurface(pSurface, &format, 0);
}
//------------------------------------------------------------------------------

Uint32* ImageDocument::SDL_SurfaceToUint32Array(SDL_Surface* pSurface)
{
	Uint32* pPixels = nullptr;

	SDL_Surface *pSource = SDL_SurfaceToRGBA(pSurface);

	if (pSource)
	{
		if( SDL_MUSTLOCK(pSource) )
			SDL_LockSurface(pSource);

		pPixels = new Uint32[ pSource->w * pSource->h ];

		// So do some work here
		for (int y = 0; y < pSource->h; ++y)
		{
			for (int x = 0; x < pSource->w; ++x)
			{
				Uint8 * pixel = (Uint8*)pSource->pixels;
				pixel += (y * pSource->pitch) + (x * sizeof(Uint32));

				Uint32 color = *((Uint32*)pixel);

				// Dump the Colors out into an array
				pPixels[ (y * pSource->w) + x ] = color;
			}
		}

		if( SDL_MUSTLOCK(pSource) )
			SDL_UnlockSurface(pSource);

		SDL_FreeSurface(pSource);

	}

	return pPixels;
}

//------------------------------------------------------------------------------

SDL_Surface* ImageDocument::SDL_SurfaceFromRawRGBA(Uint32* pPixels, int iWidth, int iHeight)
{
	SDL_Surface *pImage = SDL_CreateRGBSurface(SDL_SWSURFACE, iWidth, iHeight,
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
		return nullptr;

	if( SDL_MUSTLOCK(pImage) )
		SDL_LockSurface(pImage);


	Uint32 *pRGBA = pPixels;  // Start with the first pixel

	for (int y = 0; y < iHeight; ++y)
	{
		for (int x = 0; x < iWidth; ++x)
		{
			Uint8* pPixel = (Uint8*)pImage->pixels;
			pPixel += (y*pImage->pitch) + (x * sizeof(Uint32));

			*((Uint32*)pPixel) = *pRGBA++;
		}
	}

	if( SDL_MUSTLOCK(pImage) )
		SDL_UnlockSurface(pImage);

	return pImage;
}

//------------------------------------------------------------------------------

void ImageDocument::SetDocumentSurface(SDL_Surface* pSurface)
{
	// Free up the target, because it won't work right after a resize
		if (m_targetImage)
		{
			glDeleteTextures(1, &m_targetImage);
			m_targetImage = 0;
		}

		if (m_pTargetSurface)
		{
			// I want to accept the target here
			if (pSurface != m_pTargetSurface)
			{
				// Free the pixels, because I allocated them?
				if (pSurface->flags & SDL_PREALLOC)
					free(pSurface->pixels);

				SDL_FreeSurface(m_pTargetSurface);
			}

			m_pTargetSurface = nullptr;
		}

	// Free up the source image, and opengl texture

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

		// Set, and Register the new image
		m_pSurface = pSurface;
		m_image = SDL_GL_LoadTexture(pSurface, m_image_uv);

		m_width  = pSurface->w;
		m_height = pSurface->h;

		// Update colors
		m_numSourceColors = CountUniqueColors();
}
//------------------------------------------------------------------------------

