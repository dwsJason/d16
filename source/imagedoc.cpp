// ImageDocument Class Implementation

#include "imagedoc.h"

#include "imgui.h"
// Including internal for the eyedropper
#include "imgui_internal.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "ImGuiFileDialog.h"

#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include "log.h"
#include "libimagequant.h"
#include "limage.h"
#include "avir.h"
#include "lancir.h"

#include "toolbar.h"
#include "cursor.h"

#include <map>
#include <vector>

#include "sdl_helpers.h"

// Statics
int ImageDocument::s_uniqueId = 0;

static SDL_Cursor* pEyeDropperCursor = nullptr;

//------------------------------------------------------------------------------
// Animation Supporting Version
ImageDocument::ImageDocument(std::string filename, std::string pathname, std::vector<SDL_Surface*> Images)
	: ImageDocument(filename, pathname, Images[0])
{
	m_iDelayTimes.push_back(((long long)m_pSurfaces[0]->userdata)&0xFFFF);

	// Copy the the remainng Images / and surfaces into the ImageDocument
	for (int idx = 1; idx < Images.size(); ++idx)
	{
		m_pSurfaces.push_back(Images[idx]);
		m_images.push_back(SDL_GL_LoadTexture(m_pSurfaces[idx], m_image_uv));
		m_iDelayTimes.push_back(((long long)m_pSurfaces[idx]->userdata)&0xFFFF);
	}

	m_numSourceColors = CountUniqueColors();

	m_bPlaying = true;

}

// Just a Static Image Version
ImageDocument::ImageDocument(std::string filename, std::string pathname, SDL_Surface *pImage)
	: m_filename(filename)
	, m_pathname(pathname)
	, m_bPlaying(false)
	, m_fDelayTime(0.0f)
	, m_iFrameNo(0)
	, m_zoom(1)
	, m_pTargetSurface(nullptr)
	, m_numTargetColors(16)
	, m_iDither(50)
	, m_iPosterize(ePosterize444)
	, m_iTargetColorCount(256)
	, m_bOpen(true)
	, m_bPanActive(false)
	, m_bShowResizeUI(false)
	, m_bEyeDropDrag(false)
{
	// Make sure the surface is in a supported format for eyedropper
	//if (SDL_PIXELFORMAT_RGBA8888 != pImage->format->format)
	//if (4 != pImage->format->BytesPerPixel)
	//{
	//	m_pSurface = SDL_SurfaceToRGBA( pImage );
	//	SDL_FreeSurface( pImage );
	//	pImage = m_pSurface;
	//}

	m_images.push_back(SDL_GL_LoadTexture(pImage, m_image_uv));
	m_pSurfaces.push_back(pImage);

	m_width  = m_pSurfaces[0]->w;
	m_height = m_pSurfaces[0]->h;

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

	if (nullptr == pEyeDropperCursor)
	{
		pEyeDropperCursor = SDL_CreateEyeDropperCursor();
	}
}

ImageDocument::~ImageDocument()
{
	// unregister / free the m_image
	if (m_images.size())
	{
		glDeleteTextures((GLsizei)m_images.size(), &m_images[0]);
		m_images.clear();
	}

	// unregister / free the m_pSurface
	while (m_pSurfaces.size())
	{
		SDL_FreeSurface(m_pSurfaces[ m_pSurfaces.size() - 1 ]);
		m_pSurfaces.pop_back();
	}
}

//------------------------------------------------------------------------------

int ImageDocument::CountUniqueColors()
{
	int totalColors = 0;

	for (int idx = 0; idx < m_pSurfaces.size(); ++idx)
	{
		totalColors += SDL_Surface_CountUniqueColors(m_pSurfaces[ idx ]);
	}

	return totalColors;
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

	ImTextureID tex_id = (ImTextureID)((size_t) m_images[0] ); 
	ImVec2 uv0 = ImVec2(m_image_uv[0],m_image_uv[1]);
	ImVec2 uv1 = ImVec2(m_image_uv[2],m_image_uv[3]);

	// Animation Support
	if (m_bPlaying && m_images.size() > 1)
	{
		m_fDelayTime -= (100.0f / ImGui::GetIO().Framerate);

		if (m_fDelayTime <= 0.0f)
		{
			m_iFrameNo++;
			if (m_iFrameNo >= m_images.size())
			{
				m_iFrameNo = 0;
			}

			//m_iDelayTime = ((int)m_pSurfaces[m_iFrameNo]->userdata)&0xFFFF;
			m_fDelayTime += (float)m_iDelayTimes[m_iFrameNo];
		}

		tex_id = (ImTextureID)((size_t)m_images[m_iFrameNo]);
	}

	ImGuiStyle& style = ImGui::GetStyle();

	float padding_w = (style.WindowPadding.x + style.FrameBorderSize + style.ChildBorderSize) * 2.0f;
	float padding_h = (style.WindowPadding.y + style.FrameBorderSize + style.ChildBorderSize) * 2.0f;
	padding_h += TOOLBAR_HEIGHT;

	ImGui::SetNextWindowSize(ImVec2((m_width*m_zoom)+padding_w, (m_height*m_zoom)+padding_h), ImGuiCond_FirstUseEver);

	ImGui::Begin(m_windowName.c_str(),&m_bOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		// Make sure the toolbar gives us focus
		Toolbar::GToolbar->SetFocusWindow(m_windowName.c_str());
	}


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

	if (ImGui::IsItemHovered() && !bHasTip)
	{
		bHasTip = true;
		ImGui::BeginTooltip();
			ImGui::Text("Hover Image");
			ImGui::Text("Use Mouse Wheel");
		ImGui::EndTooltip();
	}


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

	if (ImGui::Button("16"))
	{
		// Make it 16 colors
		Quant16();
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text("Reduce / Remap Colors");
		ImGui::EndTooltip();
	}

	ImGui::SameLine();
	if (ImGui::Button("256"))
	{
		Quant256();
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

	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	ImGui::InputInt("Colors", &m_iTargetColorCount);

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

//-------------------------------- Resize Image --------------------------------

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
			if (eEyeDropper == Toolbar::GToolbar->GetCurrentMode())
			{
//---------------------------------- EyeDropper Image ---------------------------------
				if (!m_bPanActive)
					RenderEyeDropper();
			}
			else
			{
//---------------------------------- Pan Image ---------------------------------
				//RenderPanAndZoom();
			}
		}

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

		if (m_targetImages.size())
		{
			ImTextureID target_tex_id = (ImTextureID)((size_t) m_targetImages[ m_iFrameNo ] ); 

			ImGui::SameLine();
			ImGui::Image(target_tex_id, ImVec2((float)m_width*m_zoom, (float)m_height*m_zoom), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));

//-------------------  Target Image Context Menu -------------------------------

			if (ImGui::BeginPopupContextItem("Target"))
			{
				if (ImGui::MenuItem("Keep Image"))
				{
					SetDocumentSurface( /*SDL_SurfaceToRGBA(m_pTargetSurface)*/ m_pTargetSurfaces );
				}
				if (ImGui::MenuItem("Save as $C1"))
				{
					std::string defaultFilename = m_filename;

					if (defaultFilename.size() > 4)
					{
						defaultFilename  = defaultFilename.substr(0, defaultFilename.size()-4);
					}

					ImGuiFileDialog::Instance()->OpenModal("SaveC1Key", "Save as $C1", "#C10000\0.c1\0\0",
														   ".",
															defaultFilename);

				}
				//if (ImGui::MenuItem("Save as (32Bpp)PNG+PAL"))
				//{
				//}
				if (ImGui::MenuItem("Save as PNG"))
				{
					std::string defaultFilename = m_filename;

					if (defaultFilename.size() > 4)
					{
						defaultFilename  = defaultFilename.substr(0, defaultFilename.size()-4);
					}

					ImGuiFileDialog::Instance()->OpenModal("SavePNGKey", "Save as PNG", ".png\0\0",
														   ".",
															defaultFilename);

				}
				ImGui::EndPopup();
			}
		}


		// Glue for the Toolbar button
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
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

//-------------------------------- Resize Image --------------------------------

		if (m_bShowResizeUI)
		{
			//if (ImGui::BeginPopupModal("Resize Image##modal", &m_bShowResizeUI,
			//			 ImGuiWindowFlags_AlwaysAutoResize))
			//{
			//	RenderResizeDialog();
			//	ImGui::EndPopup();
			//}

		}
		else
		{
			if (eEyeDropper == Toolbar::GToolbar->GetCurrentMode())
			{
//---------------------------------- EyeDropper Image ---------------------------------
				if (!m_bEyeDropDrag)
					RenderPanAndZoom(2); // Allow pan and zoom on button 2
			}
			else
			{
//---------------------------------- Pan Image ---------------------------------
				RenderPanAndZoom();
			}
		}
//------------------------------------------------------------------------------



	ImGui::EndChild();

	ImGui::End();

// Save File Dialog Stuff

	if (ImGuiFileDialog::Instance()->FileDialog("SaveC1Key"))
	{
		if (ImGuiFileDialog::Instance()->IsOk == true)
		{
			SaveC1( ImGuiFileDialog::Instance()->GetFilepathName() );
		}

		ImGuiFileDialog::Instance()->CloseDialog("SaveC1Key");
	}

	if (ImGuiFileDialog::Instance()->FileDialog("SavePNGKey"))
	{
		if (ImGuiFileDialog::Instance()->IsOk == true)
		{
			SavePNG( ImGuiFileDialog::Instance()->GetFilepathName() );
		}

		ImGuiFileDialog::Instance()->CloseDialog("SavePNGKey");
	}

}

//------------------------------------------------------------------------------
void ImageDocument::RenderEyeDropper()
{
	bool bHasFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	bHasFocus = bHasFocus && ImGui::IsWindowHovered();

	if (bHasFocus || m_bEyeDropDrag)
	{
		SDL_SetCursor(pEyeDropperCursor);

		ImGui::PushID("EyeDropper");
		//ImGui::BeginTooltip();
		//ImGui::EndTooltip();
		static ImVec4 eyeColor = ImVec4(1,1,1,1);


		ImGuiIO& io = ImGui::GetIO();

		ImVec2 winPos = ImGui::GetWindowPos();
		float scrollX = ImGui::GetScrollX();
		float scrollY = ImGui::GetScrollY();
		float cursorX = io.MousePos.x - winPos.x;
		float cursorY = io.MousePos.y - winPos.y;

		// There's a 1 pixel border around the image
		cursorX-=1.0f;
		cursorY-=1.0f;

		// Which pixel on the canvas is the mouse over?
		float px = cursorX/m_zoom + scrollX/m_zoom;
		float py = cursorY/m_zoom + scrollY/m_zoom;

		px = floor(px);
		py = floor(py);

		if ((px >= m_pSurfaces[m_iFrameNo]->w) || (py >= m_pSurfaces[m_iFrameNo]->h))
		{
			ImGui::PopID();
			return;	// Bail out if we're in a case that just doesn't work
		}

		Uint32 pixel = SDL_GetPixel(m_pSurfaces[m_iFrameNo], (int)px, (int)py);

		if (!m_bEyeDropDrag)
		{
			eyeColor.x = (pixel & 0xFF) / 255.0f;
			eyeColor.y = ((pixel>>8)&0xFF) / 255.0f;
			eyeColor.z = ((pixel>>16)&0xFF) / 255.0f;


		// Tip String
		std::string tipString = m_filename
								+ "    x="
								+ std::to_string((int)px)
								+ " y="
								+ std::to_string((int)py);

		ImGui::ColorTooltip(tipString.c_str(),
							(float*)&eyeColor,
							ImGuiColorEditFlags_NoAlpha);

		}

		m_bEyeDropDrag = false;

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			m_bEyeDropDrag = true;

			ImGui::SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F, (float*)&eyeColor, sizeof(float) * 3, ImGuiCond_Once);
			ImGui::ColorButton(m_windowName.c_str(), eyeColor,
						ImGuiColorEditFlags_NoLabel |
						ImGuiColorEditFlags_NoAlpha |
						ImGuiColorEditFlags_NoBorder
						);
			ImGui::SameLine();
			ImGui::Text("Color");
			ImGui::EndDragDropSource();
		}

		ImGui::PopID();

	}
}
//------------------------------------------------------------------------------
void ImageDocument::RenderPanAndZoom(int iButtonIndex)
{
	bool bHasFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	bHasFocus = bHasFocus && ImGui::IsWindowHovered();

	// Show the Hand Cursor
	if ((bHasFocus&&!iButtonIndex) || m_bPanActive)
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

	if (io.MouseClicked[ iButtonIndex ] && bHasFocus)
	{
		OriginalScrollX = ImGui::GetScrollX();
		OriginalScrollY = ImGui::GetScrollY();
		m_bPanActive = true;
	}

	if (io.MouseDown[ iButtonIndex ] && m_bPanActive)
	{
		float dx = io.MouseClickedPos[ iButtonIndex ].x - io.MousePos.x;
		float dy = io.MouseClickedPos[ iButtonIndex ].y - io.MousePos.y;

		ImGui::SetScrollX(OriginalScrollX + dx);
		ImGui::SetScrollY(OriginalScrollY + dy);
	}
	else
	{
		m_bPanActive = false;
	}

}
//------------------------------------------------------------------------------

void ImageDocument::Quant16()
{
	// Do an actual color reduction on the source Image
	// then generate an OGL Texture
	LOG("Quant16 Color Reduce - Go!\n");

	int saveTargetColorCount = m_iTargetColorCount;

	m_iTargetColorCount = 16;

	Quant256();

	m_iTargetColorCount = saveTargetColorCount;
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
		int saveFrameNo = m_iFrameNo;

		for (int idx = 0; idx < m_pSurfaces.size(); ++idx)
		{
			m_iFrameNo = idx;

			m_width  = m_pSurfaces[ idx ]->w;
			m_height = m_pSurfaces[ idx ]->h;

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

		}

		m_iFrameNo = saveFrameNo;

		m_numSourceColors = CountUniqueColors();

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
	SDL_Surface *m_pSurface = m_pSurfaces[ m_iFrameNo ];

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
	SetDocumentSurface( pImage, m_iFrameNo );
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
void ImageDocument::PointSampleResize(int iNewWidth, int iNewHeight)
{
	SDL_Surface *m_pSurface = m_pSurfaces[ m_iFrameNo ];

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
	SetDocumentSurface( pImage, m_iFrameNo );
}
//------------------------------------------------------------------------------
void ImageDocument::LinearSampleResize(int iNewWidth, int iNewHeight)
{
	SDL_Surface *m_pSurface = m_pSurfaces[ m_iFrameNo ];

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

		SetDocumentSurface( pImage, m_iFrameNo );

	}
}
//------------------------------------------------------------------------------
void ImageDocument::LanczosResize(int iNewWidth, int iNewHeight)
{
	SDL_Surface *m_pSurface = m_pSurfaces[ m_iFrameNo ];

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
			SetDocumentSurface( pSurface, m_iFrameNo );
		}
	}
}

//------------------------------------------------------------------------------
void ImageDocument::AvirSampleResize(int iNewWidth, int iNewHeight, bool bDither)
{
	SDL_Surface *m_pSurface = m_pSurfaces[ m_iFrameNo ];

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
			SetDocumentSurface( pSurface, m_iFrameNo );
		}
	}
}
//------------------------------------------------------------------------------
SDL_Surface* ImageDocument::SDL_SurfaceToRGBA(SDL_Surface* pSurface)
{
    SDL_Surface* pImage = SDL_CreateRGBSurface(SDL_SWSURFACE, pSurface->w, pSurface->h, 32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN     /* OpenGL RGBA masks */
                                 0x000000FF,
                                 0x0000FF00, 0x00FF0000, 0xFF000000
#else
                                 0xFF000000,
                                 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
        );

	if (nullptr != pImage)
	{
		SDL_Rect area;
		SDL_BlendMode saved_mode;

		/* Save the alpha blending attributes */
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

	return pImage;
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

void ImageDocument::SetDocumentSurface(SDL_Surface* pSurface, int iFrameNo)
{
	// Free up the source image, and opengl texture

		// unregister / free the m_image
		if (m_images[ iFrameNo ])
		{
			glDeleteTextures(1, &m_images[ iFrameNo ]);
			m_images[ iFrameNo ] = 0;
		}

		// unregister / free the m_pSurface
		if (m_pSurfaces[ iFrameNo ])
		{
			SDL_FreeSurface(m_pSurfaces[ iFrameNo ]);
			// Free the pixels, because I allocated them?
			if (m_pSurfaces[ iFrameNo ]->flags & SDL_PREALLOC)
				free(m_pSurfaces[ iFrameNo ]->pixels);

			m_pSurfaces[ iFrameNo ] = nullptr;
		}

		// Set, and Register the new image
		m_pSurfaces[ iFrameNo ] = pSurface;

		m_width  = pSurface->w;
		m_height = pSurface->h;

		m_images[iFrameNo] = SDL_GL_LoadTexture(pSurface, m_image_uv);

		// Update colors
		//m_numSourceColors = CountUniqueColors();
}

//------------------------------------------------------------------------------



//------------------------------------------------------------------------------

void ImageDocument::SetDocumentSurface(std::vector<SDL_Surface*> pSurfaces)
{
	// Free up the target, because it won't work right after a resize
		if (m_targetImages.size())
		{
			glDeleteTextures((GLsizei) m_targetImages.size(), &m_targetImages[0]);
			m_targetImages.clear();
		}

		if (m_pTargetSurface)
		{
			// I want to accept the target here
			if (pSurfaces[0] != m_pTargetSurface)
			{
				// Free the pixels, because I allocated them?
				if (pSurfaces[0]->flags & SDL_PREALLOC)
					free(pSurfaces[0]->pixels);

				SDL_FreeSurface(m_pTargetSurface);
			}

			m_pTargetSurface = nullptr;
		}

	// Free up the source image, and opengl texture

		// unregister / free the m_image
		if (m_images.size())
		{
			glDeleteTextures((GLsizei)m_images.size(), &m_images[0]);
			m_images.clear();
		}
		// unregister / free the m_pSurface
		while (m_pSurfaces.size())
		{
			SDL_FreeSurface(m_pSurfaces[ m_pSurfaces.size() - 1 ]);
			m_pSurfaces.pop_back();
		}

		// Set, and Register the new image
		m_pSurfaces = pSurfaces;
		for (int idx = 0; idx < pSurfaces.size(); ++idx)
		{
			m_images.push_back(SDL_GL_LoadTexture(pSurfaces[idx], m_image_uv));
		}

		m_width  = pSurfaces[0]->w;
		m_height = pSurfaces[0]->h;

		// Update colors
		m_numSourceColors = CountUniqueColors();
}
//------------------------------------------------------------------------------

Uint32 ImageDocument::SDL_GetPixel(SDL_Surface* pSurface, int x, int y)
{
	Uint32 color = 0;

	if (pSurface)
	{
		// Keep x and y legitimate
		if (x < 0) x = 0;
		if (x >= pSurface->w) x = pSurface->w-1;
		if (y < 0) y = 0;
		if (y >= pSurface->h) y = pSurface->h-1;

		if (pSurface->flags & SDL_PREALLOC)
		{
			// This is only true if I allocated the pixels
			// which means this has to be 8 bit indexed
			Uint8* pPixel = (Uint8*)pSurface->pixels;
			pPixel += (y * pSurface->pitch) + x;

			int index = *pPixel;

			color = *((Uint32*)&pSurface->format->palette->colors[ index ]);

		}
		else
		{
			// Better be 32 bit per pixel

			if( SDL_MUSTLOCK(pSurface) )
				SDL_LockSurface(pSurface);

			Uint8 * pixel = (Uint8*)pSurface->pixels;
			pixel += (y * pSurface->pitch) + (x * sizeof(Uint32));

			color = *((Uint32*)pixel);

			if( SDL_MUSTLOCK(pSurface) )
				SDL_UnlockSurface(pSurface);
		}
	}

	return color;
}

//------------------------------------------------------------------------------
// Just return the index, that has the closest match to the passed in color
static Uint32 ClosestIndex(Uint32* pClut, Uint32 uColor)
{
	int closestIndex = 0;
	long closestDistance;
	Uint32 color = pClut[0];
	int red,green,blue;
	int targetRed, targetGreen, targetBlue;
	int deltaRed, deltaGreen, deltaBlue;

	targetRed   = (uColor >> 0) & 0xFF;
	targetGreen = (uColor >> 8) & 0xFF;
	targetBlue  = (uColor >>16) & 0xFF;

	red   = (color >> 0) & 0xFF;
	green = (color >> 8) & 0xFF;
	blue  = (color >>16) & 0xFF;

	deltaRed   = red - targetRed;
	deltaGreen = green - targetGreen;
	deltaBlue  = blue - targetBlue;

	closestDistance = (deltaRed * deltaRed) + (deltaGreen * deltaGreen) + (deltaBlue * deltaBlue);

	for (int idx = 1; idx < 16; ++idx)
	{
		color = pClut[ idx ];

		red   = (color >> 0) & 0xFF;
		green = (color >> 8) & 0xFF;
		blue  = (color >>16) & 0xFF;

		deltaRed   = red - targetRed;
		deltaGreen = green - targetGreen;
		deltaBlue  = blue - targetBlue;

		long distance = (deltaRed * deltaRed) + (deltaGreen * deltaGreen) + (deltaBlue * deltaBlue);

		if (distance < closestDistance)
		{
			closestDistance = distance;
			closestIndex = idx;
		}
	}

	return closestIndex;
}

void ImageDocument::SaveC1(std::string filenamepath)
{
// Copy of the C1 memory
	unsigned char c1data[ 0x8000 ];
	memset(c1data, 0, 0x8000 );

// Get a copy of the clut
	Uint32 pClut[ 16 ];

	for (int idx = 0; idx < m_targetColors.size(); ++idx)
	{
		const ImVec4& floatColor = m_targetColors[ idx ];

		float red   = floatColor.x * 255.0f;
		float green = floatColor.y * 255.0f;
		float blue  = floatColor.z * 255.0f;

		Uint32 color = 0xFF000000;   					// A = 1.0
		color       |= (((Uint32)blue)&0xFF)  << 16;
		color       |= (((Uint32)green)&0xFF) << 8;
		color       |= (((Uint32)red)&0xFF)   << 0;
		
		pClut[idx] = color;
	}

// Choose a surface to save
	SDL_Surface* pImage = m_pTargetSurface ? m_pTargetSurface : m_pSurfaces[0];

	// Nibblized pixel data
	for (int y = 0; y < 200; ++y)
	{
		for (int x = 0; x < 320; x+=2)
		{
			Uint32 pixel0 = SDL_GetPixel(pImage, x, y);
			Uint32 index0 = ClosestIndex(pClut, pixel0);
			Uint32 pixel1 = SDL_GetPixel(pImage, x+1, y);
			Uint32 index1 = ClosestIndex(pClut, pixel1);

			c1data[ (y * 160) + (x>>1) ] = (unsigned char) (index1 | (index0<<4));
		}
	}

	// Color Data, just doing a floor conversion

	Uint16* pPal = (Uint16*)(&c1data[ 0x7E00 ]);
	for (int idx = 0; idx < 16; ++idx)
	{
		Uint32 sourceColor = pClut[ idx ];
		Uint16 targetColor = (Uint16)(((sourceColor>>4) & 0xF) << 8); // Red

		targetColor |= (Uint16) (((sourceColor>>12) & 0xF) << 4); // Green
		targetColor |= (Uint16) (((sourceColor>>20) & 0xF) << 0); // Blue

		pPal[ idx ] = targetColor;
	}


	// Serialize to disk
	{
		FILE* file = fopen(filenamepath.c_str(), "wb");

		fwrite(c1data, 1, 0x8000, file);

		fclose(file);
	}

}
//------------------------------------------------------------------------------

// For now, I'm just making this easy
// and using what SDL gave me

void ImageDocument::SavePNG(std::string filenamepath)
{
// Choose a surface to save
	SDL_Surface* pImage = m_pTargetSurface ? m_pTargetSurface : m_pSurfaces[0];

	IMG_SavePNG(pImage, filenamepath.c_str());
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
void ImageDocument::Quant256()
{
	LOG("Quant256\n");

    unsigned int width=(unsigned int)m_width;
	unsigned int height=(unsigned int)m_height;

	std::vector<SDL_Surface*> pImages;
	std::vector<unsigned char*> pRawPixels;


	//-----------------------------------------------
	for (int idx = 0; idx < m_pSurfaces.size(); ++idx)
	{
		SDL_Surface *pImage = SDL_SurfaceToRGBA(m_pSurfaces[idx]);

		if (nullptr == pImage) return;

		if( SDL_MUSTLOCK(pImage) )
			SDL_LockSurface(pImage);

		unsigned char* raw_rgba_pixels = (unsigned char*) pImage->pixels;

		//-----------------------------------------------
		// Since I'm not supporting Alpha, now is the time
		// to pre-multiply Alpha, and set the alpha to 1

		{
			for (int y = 0; y < pImage->h; ++y)
			{
				unsigned char *pPixel = raw_rgba_pixels + (y * pImage->pitch);

				for (int x = 0; x < pImage->w; ++x)
				{
					unsigned int a = pPixel[3];

					if (a != 0xFF)
					{
						unsigned int r = pPixel[0];
						unsigned int g = pPixel[1];
						unsigned int b = pPixel[2];

						r *= a; r>>=8;
						g *= a; g>>=8;
						b *= a; b>>=8;

						a = 255;

						pPixel[0] = (unsigned char)r;
						pPixel[1] = (unsigned char)g;
						pPixel[2] = (unsigned char)b;
						pPixel[3] = (unsigned char)a;
					}

					pPixel+=4;
				}
			}
		}

		pImages.push_back(pImage);
		pRawPixels.push_back(raw_rgba_pixels);
	}

	//-----------------------------------------------

    liq_attr* attr_handle = liq_attr_create();

	liq_set_max_colors(attr_handle, m_iTargetColorCount);
	liq_set_speed(attr_handle, 1);   // 1-10  (1 best quality)

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

	liq_set_min_posterization(attr_handle, min_posterize);

	liq_histogram* pHist = liq_histogram_create( attr_handle );

	std::vector<liq_image*> input_images;

	for (int rawIndex = 0; rawIndex < pRawPixels.size(); ++rawIndex)
	{
		//$$JGA Fixed Colors can be added to the input_image
		//$$JGA which is going to be sweet

		liq_image *input_image = liq_image_create_rgba(attr_handle, pRawPixels[ rawIndex ], width, height, 0);

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

		input_images.push_back(input_image);

		liq_histogram_add_image(pHist, attr_handle, input_image);
	}




	// You could set more options here, like liq_set_quality
    liq_result *quantization_result;
    if (liq_histogram_quantize(pHist, attr_handle, &quantization_result) != LIQ_OK) {
        LOG("Quantization failed, memory leaked\n");
		return;
    }

    // Use libimagequant to make new image pixels from the palette
	const liq_palette *palette = liq_get_palette(quantization_result);

    size_t pixels_size = width * height;
	liq_set_dithering_level(quantization_result, m_iDither / 100.0f);  // 0.0->1.0
//	liq_set_output_gamma(quantization_result, 1.0);

	std::vector<SDL_Surface*> pResults;

	for (int idx = 0; idx < input_images.size(); ++idx)
	{
		unsigned char *raw_8bit_pixels = (unsigned char*)malloc(pixels_size);

		liq_write_remapped_image(quantization_result, input_images[ idx ], raw_8bit_pixels, pixels_size);

		// Convert Results into a Surface ------------------------------------------

		SDL_Surface *pTargetSurface = SDL_CreateRGBSurfaceWithFormatFrom(
										raw_8bit_pixels, width, height,
										8, width, SDL_PIXELFORMAT_INDEX8);
		SDL_Palette *pPalette = SDL_AllocPalette( m_iTargetColorCount );

		SDL_SetPaletteColors(pPalette, (const SDL_Color*)palette->entries, 0, m_iTargetColorCount);

		SDL_SetSurfacePalette(pTargetSurface, pPalette);

		pResults.push_back(pTargetSurface);
	}

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

	// Fix up the GUI application junk

	// We need to clear and free up the target image lists
	//--------------------------------------------------------------------------
	for (int idx = 0; idx < m_targetImages.size(); ++idx)
	{
		if (m_targetImages[idx])
		{
			glDeleteTextures(1, &m_targetImages[idx]);
			m_targetImages[idx] = 0;
		}

		SDL_Surface* pSurface = m_pTargetSurfaces[idx];

		// Free the pixels, because I allocated them?
		if (pSurface->flags & SDL_PREALLOC)
			free(pSurface->pixels);

		SDL_FreeSurface(pSurface);
		m_pTargetSurfaces[idx] = nullptr;
	}
	m_targetImages.clear();
	m_pTargetSurfaces.clear();
	//--------------------------------------------------------------------------

	GLfloat about_image_uv[4];
	for (int idx = 0; idx < pResults.size(); ++idx)
	{
		m_targetImages.push_back(SDL_GL_LoadTexture(pResults[idx], about_image_uv));
		m_pTargetSurfaces.push_back(pResults[idx]);
	}

	//m_targetImage = m_targetImages[0];
    m_pTargetSurface = m_pTargetSurfaces[0];

	// Free up the memory used by libquant -------------------------------------
    liq_result_destroy(quantization_result); // Must be freed only after you're done using the palette

	while (input_images.size())
	{
		liq_image_destroy(input_images[input_images.size()-1]);
		input_images.pop_back();
	}

    liq_attr_destroy(attr_handle);

	// SDL_CreateRGBSurfaceWithFormatFrom, makes you manage the raw pixels buffer
	// instead of make a copy of it, so I'm supposed to free it manually, after
	// the surface is free!
    //free(raw_8bit_pixels);  // The surface owns these now

	//-----------------------------------------------

	while (pImages.size())
	{
		SDL_Surface *pImage = pImages[ pImages.size() - 1 ];
		pImages.pop_back();

		if( SDL_MUSTLOCK(pImage) )
			SDL_UnlockSurface(pImage);

		SDL_FreeSurface(pImage);     /* No longer needed */
	}

}

//------------------------------------------------------------------------------

