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

#include "gsla_file.h"

// Stuff for Journey
#include "gsdx_file.h"

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
	, m_bIsFirstRender(true)
	, m_bPlaying(false)
	, m_fDelayTime(0.0f)
	, m_iFrameNo(0)
	, m_zoom(1)
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

	//--------------------------------------------------------------------------
	// Initial Zoom
	// If the image is small, automatically make it a little bigger
	if ((m_width <= 320) && (m_height <= 200))
	{
		m_zoom = 2;

		if ((m_width <= 160) && (m_height <= 100))
		{
			m_zoom = 4;
		}
	}
	m_previousZoom = m_zoom;
	//--------------------------------------------------------------------------

	// Assign a unique Window Name
	m_uniqId = "##" + std::to_string(s_uniqueId++);
	m_windowName = filename + m_uniqId;

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

	m_numUniqueColors.resize(m_pSurfaces.size());

	std::map<Uint32,Uint32> histogram;

	for (int idx = 0; idx < m_pSurfaces.size(); ++idx)
	{
		m_numUniqueColors[idx] = SDL_Surface_CountUniqueColors(m_pSurfaces[ idx ], &histogram);
	}

	totalColors = (int)histogram.size();

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

	ImTextureID tex_id = (ImTextureID)((size_t) m_images[m_iFrameNo] ); 
	ImVec2 uv0 = ImVec2(m_image_uv[0],m_image_uv[1]);
	ImVec2 uv1 = ImVec2(m_image_uv[2],m_image_uv[3]);

	//--------------------------------------------------------------------------
	// Animation Support
	//--------------------------------------------------------------------------
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

			m_fDelayTime += (float)m_iDelayTimes[m_iFrameNo];
		}

		tex_id = (ImTextureID)((size_t)m_images[m_iFrameNo]);
	}
	//--------------------------------------------------------------------------

	ImGuiIO& io = ImGui::GetIO();

	ImGuiStyle& style = ImGui::GetStyle();

	float padding_w = (style.WindowPadding.x + style.FrameBorderSize + style.ChildBorderSize) * 2.0f;
	float padding_h = (style.WindowPadding.y + style.FrameBorderSize + style.ChildBorderSize) * 2.0f;
	padding_h += TOOLBAR_HEIGHT;

	//--------------------------------------------------------------------------
	// Very First Render
	//--------------------------------------------------------------------------
	if (m_bIsFirstRender)
	{
		m_bIsFirstRender = false;
		//
		// I'm going crazy with some image windows, opening up, larger than the parent window
		//
		ImVec2 InitialSize((m_width*m_zoom)+padding_w, (m_height*m_zoom)+padding_h);

		if (InitialSize.x > io.DisplaySize.x) InitialSize.x = io.DisplaySize.x;
		if (InitialSize.y > io.DisplaySize.y) InitialSize.y = io.DisplaySize.y;

		ImGui::SetNextWindowSize(InitialSize, ImGuiCond_FirstUseEver);
	}
	//--------------------------------------------------------------------------

	ImGui::Begin(m_windowName.c_str(),&m_bOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		// Make sure the toolbar gives us focus
		Toolbar::GToolbar->SetFocusWindow(m_windowName.c_str());
	}


//	bool bHasFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

//	ImGui::Text("Source:");
//	ImGui::SameLine();

	if (m_numUniqueColors.size() > 1)
	{
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%d/%d Colors", m_numUniqueColors[m_iFrameNo], m_numSourceColors);
	}
	else
	{
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%d Colors", m_numSourceColors);
	}
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

	// Display the number of target Colors, on the button
	if (ImGui::Button(std::to_string(m_iTargetColorCount).c_str()))
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

	// Keep the Color Count in some kind of reasonable range
	if (m_iTargetColorCount < 2)
		m_iTargetColorCount = 2;

	if (m_iTargetColorCount > 256)
		m_iTargetColorCount = 256;

	//--------------------------------------------------------------------------

	// Width and Height here needs to be based on the parent Window

	ImVec2 window_size = ImGui::GetWindowSize();
	window_size.x -= padding_w;
	window_size.y -= padding_h;
	window_size.x += style.ChildBorderSize*2.0f;
	window_size.y += style.ChildBorderSize*2.0f;

	if (m_previousZoom != m_zoom)
	{
		ImVec2 ContentSize((float)m_width*m_zoom, (float)m_height*m_zoom);

		ContentSize.x += style.ChildBorderSize*2.0f;
		ContentSize.y += style.ChildBorderSize*2.0f;

		if (m_targetImages.size())
		{
			// If we have a second image take into account
			ContentSize.x *= 2.0f;
			// We also need to add in the space between the images, that is
//			ContentSize.x += ImGui::GetStyle().ItemSpacing.x;
		}
		ImGui::SetNextWindowContentSize( ContentSize );

	}

	ImGui::BeginChild("Source", ImVec2(window_size.x,
									   window_size.y ), false,
						  ImGuiWindowFlags_NoMove |
						  ImGuiWindowFlags_HorizontalScrollbar |
						  ImGuiWindowFlags_NoScrollWithMouse   |
						  ImGuiWindowFlags_AlwaysAutoResize );

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

			ImGui::Separator();
			ImGui::Separator();

			if (ImGui::MenuItem("Rotate 90 Right/CW"))
			{
				RotateRight();
			}
			if (ImGui::MenuItem("Rotate 90 Left/CCW"))
			{
				RotateLeft();
			}

			ImGui::Separator();
			ImGui::Separator();

			if (ImGui::MenuItem("Mirror Horizontal"))
			{
				MirrorHorizontal();
			}
			if (ImGui::MenuItem("Mirror Vertical"))
			{
				MirrorVertical();
			}

		    ImGui::EndPopup();
		}

		if (m_targetImages.size())
		{
			ImTextureID target_tex_id = (ImTextureID)((size_t) m_targetImages[ m_iFrameNo ] ); 

			ImGui::SameLine(0.0f,0.0f);
			ImGui::Image(target_tex_id, ImVec2((float)m_width*m_zoom, (float)m_height*m_zoom), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));

//-------------------  Target Image Context Menu -------------------------------

			if (ImGui::BeginPopupContextItem("Target"))
			{
				if (ImGui::MenuItem("Keep Image"))
				{
					SetDocumentSurface( m_pTargetSurfaces );
				}

				ImGui::Separator();
				ImGui::Separator();

				if (ImGui::MenuItem("Export for Journey") )
				{
					std::string defaultFilename = m_filename;
					std::string dialogKey = "SaveJourneyKey" + m_uniqId;

					if (defaultFilename.size() > 4)
					{
						defaultFilename  = defaultFilename.substr(0, defaultFilename.size()-4);
					}

					ImGuiFileDialog::Instance()->OpenModal(dialogKey, "Save as Journey", "\0\0",
														   ".",
															defaultFilename);

					ImGui::SetWindowFocus(dialogKey.c_str());
				}

				ImGui::Separator();
				ImGui::Separator();

				if (ImGui::MenuItem("Save as GSLA") )
				{
					std::string defaultFilename = m_filename;
					std::string dialogKey = "SaveGSLAKey" + m_uniqId;

					if (defaultFilename.size() > 4)
					{
						defaultFilename  = defaultFilename.substr(0, defaultFilename.size()-4);
					}

					ImGuiFileDialog::Instance()->OpenModal(dialogKey, "Save as GSLA", ".gsla\0\0",
														   ".",
															defaultFilename);

					ImGui::SetWindowFocus(dialogKey.c_str());
				}


				if (ImGui::MenuItem("Save as $C1"))
				{
					std::string defaultFilename = m_filename;
					std::string dialogKey = "SaveC1Key" + m_uniqId;


					if (defaultFilename.size() > 4)
					{
						defaultFilename  = defaultFilename.substr(0, defaultFilename.size()-4);
					}

					ImGuiFileDialog::Instance()->OpenModal(dialogKey, "Save as $C1", "#C10000\0.c1\0\0",
														   ".",
															defaultFilename);

					ImGui::SetWindowFocus(dialogKey.c_str());


				}

				if (ImGui::MenuItem("Save as $C2"))
				{
					std::string defaultFilename = m_filename;

					std::string dialogKey = "SaveC2Key" + m_uniqId;

					if (defaultFilename.size() > 4)
					{
						defaultFilename  = defaultFilename.substr(0, defaultFilename.size()-4);
					}

					ImGuiFileDialog::Instance()->OpenModal(dialogKey, "Save as $C2", ".c2\0#C20000\0\0",
														   ".",
															defaultFilename);

					ImGui::SetWindowFocus(dialogKey.c_str());
				}


				//if (ImGui::MenuItem("Save as (32Bpp)PNG+PAL"))
				//{
				//}
				if (ImGui::MenuItem("Save as PNG"))
				{
					std::string defaultFilename = m_filename;
					std::string dialogKey = "SavePNGKey" + m_uniqId;

					if (defaultFilename.size() > 4)
					{
						defaultFilename  = defaultFilename.substr(0, defaultFilename.size()-4);
					}

					ImGuiFileDialog::Instance()->OpenModal(dialogKey, "Save as PNG", ".png\0\0",
														   ".",
															defaultFilename);

					ImGui::SetWindowFocus(dialogKey.c_str());
				}

				if (ImGui::MenuItem("Save as 256 (Foenix Bitmap)"))
				{
					std::string defaultFilename = m_filename;
					std::string dialogKey = "Save256Key" + m_uniqId;

					if (defaultFilename.size() > 4)
					{
						defaultFilename  = defaultFilename.substr(0, defaultFilename.size()-4);
					}

					ImGuiFileDialog::Instance()->OpenModal(dialogKey, "Save as 256", ".256\0\0",
														   ".",
															defaultFilename);

					ImGui::SetWindowFocus(dialogKey.c_str());
				}


				if (ImGui::MenuItem("Save as FAN(Foenix Anim - Bitmap)"))
				{
					std::string defaultFilename = m_filename;
					std::string dialogKey = "SaveFANKey" + m_uniqId;

					if (defaultFilename.size() > 4)
					{
						defaultFilename  = defaultFilename.substr(0, defaultFilename.size()-4);
					}

					ImGuiFileDialog::Instance()->OpenModal(dialogKey, "Save as FAN(bitmap)", ".fan\0\0",
														   ".",
															defaultFilename);

					ImGui::SetWindowFocus(dialogKey.c_str());


				}

				if (ImGui::MenuItem("Save as FAN(Foenix Anim - Tiles)"))
				{
					std::string defaultFilename = m_filename;
					std::string dialogKey = "SaveFANTileKey" + m_uniqId;

					if (defaultFilename.size() > 4)
					{
						defaultFilename  = defaultFilename.substr(0, defaultFilename.size()-4);
					}

					ImGuiFileDialog::Instance()->OpenModal(dialogKey, "Save as FAN(tiles)", ".fan\0\0",
														   ".",
															defaultFilename);

					ImGui::SetWindowFocus(dialogKey.c_str());
				}


				ImGui::EndPopup();
			}
		}


		// Glue for the Toolbar button
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			int toolBarMode = Toolbar::GToolbar->GetCurrentMode();

			switch (toolBarMode)
			{
			case eResizeImage:
				bOpenResizeModal = true;
				Toolbar::GToolbar->SetPreviousMode();
				break;
			case eRotate90Right:
				RotateRight();
				Toolbar::GToolbar->SetPreviousMode();
				break;
			case eRotate90Left:
				RotateLeft();
				Toolbar::GToolbar->SetPreviousMode();
				break;
			case eMirrorHorizontal:
				MirrorHorizontal();
				Toolbar::GToolbar->SetPreviousMode();
				break;
			case eMirrorVertical:
				MirrorVertical();
				Toolbar::GToolbar->SetPreviousMode();
				break;

			default:
				// nothing, probably pan or something
				break;
			}
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

		const float ScrollX = ImGui::GetScrollX();
		const float ScrollY = ImGui::GetScrollY();

	ImGui::EndChild();

	//if (eJrOBJAnalyze == Toolbar::GToolbar->GetCurrentMode())
	{
		RenderOBJShapes(ScrollX,ScrollY);
	}

	if (m_pSurfaces.size() > 1)
	{
		RenderTimeLine();
	}

	ImGui::End();

// Save File Dialog Stuff

	if (ImGuiFileDialog::Instance()->FileDialog("SaveC1Key" + m_uniqId))
	{
		if (ImGuiFileDialog::Instance()->IsOk == true)
		{
			SaveC1( ImGuiFileDialog::Instance()->GetFilepathName() );
		}

		ImGuiFileDialog::Instance()->CloseDialog("SaveC1Key" + m_uniqId);
	}

	if (ImGuiFileDialog::Instance()->FileDialog("SaveC2Key" + m_uniqId))
	{
		if (ImGuiFileDialog::Instance()->IsOk == true)
		{
			SaveC2( ImGuiFileDialog::Instance()->GetFilepathName() );
		}

		ImGuiFileDialog::Instance()->CloseDialog("SaveC2Key" + m_uniqId);
	}

	if (ImGuiFileDialog::Instance()->FileDialog("SaveJourneyKey" + m_uniqId))
	{
		if (ImGuiFileDialog::Instance()->IsOk == true)
		{
			//SaveGSLA( ImGuiFileDialog::Instance()->GetFilepathName() );
			GSDXFile gsdx( m_pTargetSurfaces[0] );

			std::string basename = ImGuiFileDialog::Instance()->GetFilepathName();

			std::string p1 = basename+"p1";
			std::string p2 = basename+"p2";
			std::string m1 = basename+"m1";
			std::string m2 = basename+"m2";

			gsdx.GenerateDeltaY( 1, p1.c_str() );
			gsdx.GenerateDeltaY( 2, p2.c_str() );

			gsdx.GenerateDeltaY( -1, m1.c_str() );
			gsdx.GenerateDeltaY( -2, m2.c_str() );
		}

		ImGuiFileDialog::Instance()->CloseDialog("SaveJourneyKey" + m_uniqId);
	}

	if (ImGuiFileDialog::Instance()->FileDialog("SaveGSLAKey" + m_uniqId))
	{
		if (ImGuiFileDialog::Instance()->IsOk == true)
		{
			SaveGSLA( ImGuiFileDialog::Instance()->GetFilepathName() );
		}

		ImGuiFileDialog::Instance()->CloseDialog("SaveGSLAKey" + m_uniqId);
	}

	if (ImGuiFileDialog::Instance()->FileDialog("SavePNGKey" + m_uniqId))
	{
		if (ImGuiFileDialog::Instance()->IsOk == true)
		{
			SavePNG( ImGuiFileDialog::Instance()->GetFilepathName() );
		}

		ImGuiFileDialog::Instance()->CloseDialog("SavePNGKey" + m_uniqId);
	}

	if (ImGuiFileDialog::Instance()->FileDialog("Save256Key" + m_uniqId))
	{
		if (ImGuiFileDialog::Instance()->IsOk == true)
		{
			Save256( ImGuiFileDialog::Instance()->GetFilepathName());
		}

		ImGuiFileDialog::Instance()->CloseDialog("Save256Key" + m_uniqId);
	}

	if (ImGuiFileDialog::Instance()->FileDialog("SaveFANKey" + m_uniqId))
	{
		if (ImGuiFileDialog::Instance()->IsOk == true)
		{
			SaveFAN( ImGuiFileDialog::Instance()->GetFilepathName(), false );
		}

		ImGuiFileDialog::Instance()->CloseDialog("SaveFANKey" + m_uniqId);
	}

	if (ImGuiFileDialog::Instance()->FileDialog("SaveFANTileKey" + m_uniqId))
	{
		if (ImGuiFileDialog::Instance()->IsOk == true)
		{
			SaveFAN( ImGuiFileDialog::Instance()->GetFilepathName(), true );
		}

		ImGuiFileDialog::Instance()->CloseDialog("SaveFANTileKey" + m_uniqId);
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

		if (!io.MouseDown[0])
		{
			eyeColor.x = (pixel & 0xFF) / 255.0f;
			eyeColor.y = ((pixel>>8)&0xFF) / 255.0f;
			eyeColor.z = ((pixel>>16)&0xFF) / 255.0f;
		}

		if (!m_bEyeDropDrag)
		{

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
	// Keep Track of Previous Zoom
	m_previousZoom = m_zoom;

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
bool ImageDocument::CheckGrid(int gx, int gy, std::vector<int>& grid, int grid_w, int grid_h, int obj_size)
{
	int num_tiles = 0;
	switch (obj_size)
	{
	case TILE_8x8:
		num_tiles = 1;
		break;
	case TILE_16x16:
		num_tiles = 2;
		break;
	case TILE_24x24:
		num_tiles = 3;
		break;
	case TILE_32x32:
		num_tiles = 4;
		break;
	default:
		num_tiles = 0;
		break;
	}

	if (!num_tiles)
	{
		return false;
	}

	for (int new_y = gy; new_y < (gy + num_tiles); ++new_y)
	{
		if (new_y > grid_h)
		{
			return false;
		}

		for (int new_x = gx; new_x < (gx + num_tiles); ++new_x)
		{
			if (new_x > grid_w)
			{
				return false;
			}

			if (TILE_8x8 != grid[(new_y * grid_w) + new_x])
			{
				return false;
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
void ImageDocument::SetGrid(int gx, int gy, std::vector<int>& grid, int grid_w, int grid_h, int obj_size)
{
	int num_tiles = 0;
	switch (obj_size)
	{
	case TILE_8x8:
		num_tiles = 1;
		break;
	case TILE_16x16:
		num_tiles = 2;
		break;
	case TILE_24x24:
		num_tiles = 3;
		break;
	case TILE_32x32:
		num_tiles = 4;
		break;
	default:
		num_tiles = 0;
		break;
	}

	if (!num_tiles)
	{
		return;
	}

	for (int new_y = gy; new_y < (gy + num_tiles); ++new_y)
	{
		if (new_y > grid_h)
		{
			continue;
		}

		for (int new_x = gx; new_x < (gx + num_tiles); ++new_x)
		{
			if (new_x > grid_w)
			{
				continue;
			}

			if ((gx == new_x) && (gy == new_y))
			{
				grid[(new_y * grid_w) + new_x] = obj_size;
			}
			else
			{
				grid[(new_y * grid_w) + new_x] = TILE_USED;
			}
		}
	}

}

//------------------------------------------------------------------------------
bool ImageDocument::CheckSurface8x8(SDL_Surface* pSurface, Uint32 bg_pixel, int x, int y)
{
	// Check the 8x8 tile to see if it has any pixels we care about
	for (int ly = 0; ly < 8; ++ly)
	{
		for (int lx = 0; lx < 8; ++lx)
		{
			Uint32 pixel = SDL_GetPixel(pSurface, x+lx, y+ly);

			if (pixel != bg_pixel)
			{
				return true;
			}
		}
	}

	return false;
}
//------------------------------------------------------------------------------
void ImageDocument::RenderOBJShapes(const float ScrollX, const float ScrollY)
{
	// By placing what we draw into a "window", it will appear on top
	// of our image

	ImVec2 parentPosition = ImGui::GetWindowPos();
	ImVec2 parentSize     = ImGui::GetWindowSize();

	ImVec2 windowPosition = parentPosition;
	ImVec2 windowSize = parentSize;

	// Position
	windowPosition.y += 82.0f;
	windowPosition.x += 8.0f;

	ImGui::SetNextWindowPos(windowPosition);

	ImGui::BeginChild("OBJ Shapes", windowSize,
						  false,
					      ImGuiWindowFlags_NoNav |
					      ImGuiWindowFlags_NoDecoration |
					      ImGuiWindowFlags_NoInputs |
						  ImGuiWindowFlags_NoMove |
						  ImGuiWindowFlags_NoScrollbar |
						  ImGuiWindowFlags_NoScrollWithMouse   |
						  ImGuiWindowFlags_AlwaysAutoResize );


	// Where we're plotting these boxes
	ImVec2 winPos = ImGui::GetWindowPos();
	winPos.x -= ScrollX;
	winPos.y -= ScrollY;

	winPos.x += (m_zoom * 0.5f);
	winPos.y += (m_zoom * 0.5f);
	winPos.x += 1.0f;   	  		/* Magic Numbers */
	winPos.y += 2.0f;

	// Get the Frame Surface
	SDL_Surface* pSurface = m_pSurfaces[m_iFrameNo];

	Uint32 bg_pixel = SDL_GetPixel(pSurface, (int)0, (int)0);

	// Draw a Square around the "sprite"
	Uint32 minx = pSurface->w;
	Uint32 maxx = 0;
	Uint32 miny = pSurface->h;
	Uint32 maxy = 0;

	for (int y = 0; y < pSurface->h; ++y)
	{
		for (int x = 0; x < pSurface->w; ++x)
		{
			Uint32 pixel = SDL_GetPixel(pSurface, x, y);

			if (pixel != bg_pixel)
			{
				if (x < minx) minx = x;
				if (x > maxx) maxx = x;
				if (y < miny) miny = y;
				if (y > maxy) maxy = y;
			}

		}
	}

	//minx*=m_zoom;
	//maxx*=m_zoom;
	//miny*=m_zoom;
	//maxy*=m_zoom;
#if 0
	// Bounding Rect just gets in the way
	ImGui::GetWindowDrawList()->AddRect(
		ImVec2(((float)minx*m_zoom)+winPos.x,((float)miny * m_zoom)+winPos.y),
		ImVec2(((float)maxx*m_zoom)+winPos.x,((float)maxy * m_zoom)+winPos.y),
		0x800000FF,  // Red
		0.0f,
		ImDrawCornerFlags_None,
		((float)m_zoom));
#endif

	// The Scan Area is now defined with the min/max
	int tile_w = ((pSurface->w + 7) / 8);
	int tile_h = ((pSurface->h + 7) / 8);

	int best_obj_count = tile_w * tile_h + 1;
	int best_offset_x = 0;
	int best_offset_y = 0;

	std::vector<int> tile_map(tile_w * tile_h);

	// Nested 64 iteration loop, to slide the tiles around
	// to find the set with the least number of OBJS/Sprites
	for (int scan_offset_y = 0; scan_offset_y < 8; ++scan_offset_y)
	{
		for (int scan_offset_x =0; scan_offset_x < 8; ++scan_offset_x)
		{
			memset(&tile_map[0], 0, sizeof(int) * tile_w * tile_h);
			for (int y = miny - scan_offset_y; y <= maxy; y+=8)
			{
				for (int x = minx - scan_offset_x; x <= maxx; x+=8)
				{
					tile_map[((y/8)*tile_w)+(x/8)] = CheckSurface8x8(pSurface, bg_pixel, x, y) ? TILE_8x8 : TILE_EMPTY;
				}
			}

			int offset_x = (minx-scan_offset_x) & 0x7;
			int offset_y = (miny-scan_offset_y) & 0x7;
			int obj_count = 0;

			for (int obj_size = TILE_32x32; obj_size >= TILE_8x8; --obj_size)
			{
				for (int y = 0; y < tile_h; ++y)
				{
					for (int x = 0; x < tile_w; ++x)
					{
						if (CheckGrid(x, y, tile_map, tile_w, tile_h, obj_size))
						{
							SetGrid(x, y, tile_map, tile_w, tile_h, obj_size);
							obj_count++;
						}
					}
				}
			}

			if (obj_count < best_obj_count)
			{
				best_obj_count = obj_count;
				best_offset_x = scan_offset_x;
				best_offset_y = scan_offset_y;
			}
		}
	}



//-----------------------------------------------------------------------------

	int scan_offset_x = best_offset_x;
	int scan_offset_y = best_offset_y;
	memset(&tile_map[0], 0, sizeof(int) * tile_w * tile_h);
	// Do the process again, on the best setting
	for (int y = miny - scan_offset_y; y <= maxy; y+=8)
	{
		for (int x = minx - scan_offset_x; x <= maxx; x+=8)
		{
			tile_map[((y/8)*tile_w)+(x/8)] = CheckSurface8x8(pSurface, bg_pixel, x, y) ? TILE_8x8 : TILE_EMPTY;
		}
	}

	int offset_x = (minx-scan_offset_x) & 0x7;
	int offset_y = (miny-scan_offset_y) & 0x7;
	int obj_count = 0;

	for (int obj_size = TILE_32x32; obj_size >= TILE_8x8; --obj_size)
	{
		for (int y = 0; y < tile_h; ++y)
		{
			for (int x = 0; x < tile_w; ++x)
			{
				if (CheckGrid(x, y, tile_map, tile_w, tile_h, obj_size))
				{
					SetGrid(x, y, tile_map, tile_w, tile_h, obj_size);
					obj_count++;
				}
			}
		}
	}

	for (int ty = 0; ty < tile_h; ++ty)
	{
		for (int tx = 0; tx < tile_w; ++tx)
		{
			int x = (tx * 8) + offset_x;
			int y = (ty * 8) + offset_y;

			switch (tile_map[(ty * tile_w) + tx])
			{
			case TILE_8x8:
				{
					ImGui::GetWindowDrawList()->AddRect(
						ImVec2((((float)x) * m_zoom) + winPos.x, (((float)y) * m_zoom) + winPos.y),
						ImVec2((((float)x + 7) * m_zoom) + winPos.x, (((float)y + 7) * m_zoom) + winPos.y),
						0x8000FF00,  // Green
						0.0f,
						ImDrawCornerFlags_None,
						((float)m_zoom));
				}
				break;
			case TILE_16x16:
				{
					ImGui::GetWindowDrawList()->AddRect(
						ImVec2((((float)x) * m_zoom) + winPos.x, (((float)y) * m_zoom) + winPos.y),
						ImVec2((((float)x + 15) * m_zoom) + winPos.x, (((float)y + 15) * m_zoom) + winPos.y),
						0x800000FF,  // Red
						0.0f,
						ImDrawCornerFlags_None,
						((float)m_zoom));
				}
				break;
			case TILE_24x24:
				{
					ImGui::GetWindowDrawList()->AddRect(
						ImVec2((((float)x) * m_zoom) + winPos.x, (((float)y) * m_zoom) + winPos.y),
						ImVec2((((float)x + 23) * m_zoom) + winPos.x, (((float)y + 23) * m_zoom) + winPos.y),
						0x80FF0000,  // Blue
						0.0f,
						ImDrawCornerFlags_None,
						((float)m_zoom));
				}
				break;
			case TILE_32x32:
				{
					ImGui::GetWindowDrawList()->AddRect(
						ImVec2((((float)x) * m_zoom) + winPos.x, (((float)y) * m_zoom) + winPos.y),
						ImVec2((((float)x + 31) * m_zoom) + winPos.x, (((float)y + 31) * m_zoom) + winPos.y),
						0x8000FFFF,  // Yello
						0.0f,
						ImDrawCornerFlags_None,
						((float)m_zoom));
				}
				break;
			default:
				break;
			}
		}
	}


	#if 0
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(winPos.x,winPos.y),
		ImVec2(winPos.x+m_zoom*m_pSurfaces[m_iFrameNo]->w, winPos.y),
		0xC0008000,
		m_zoom
		);

	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(winPos.x,winPos.y),
		ImVec2(winPos.x,winPos.y+m_zoom*m_pSurfaces[m_iFrameNo]->h),
		0xC0008000,
		m_zoom
		);
	#endif



	#if 0
	ImVec2 points[ 2 ];

	for (int x = 0; x < 100; ++x)
	{
		ImVec2 pos = ImVec2(winPos.x + (x * step), winPos.y);
		points[ 0 ] = pos;
		if (x % 10)
		{
			if (x % 5)
			{
				pos.y += 15.0f;
			}
			else
			{
				pos.y += 22.0f;
			}
		}
		else
		{
			pos.y += 30.0f;
		}
		points[ 1 ] = pos;

		ImGui::GetWindowDrawList()->AddLine(
			points[ 0 ],
			points[ 1 ],
			0xC0008000,
			m_zoom
			);

	}
	#endif

	ImGui::EndChild();

}
//------------------------------------------------------------------------------
void ImageDocument::RenderTimeLine()
{
	float timeSeconds = 0.0f;

	std::vector<float> tickTimes;

	// Total up the time in seconds
	// probably should cache this
	for (int idx = 0; idx < m_iDelayTimes.size(); ++idx)
	{
		tickTimes.push_back( timeSeconds );
		timeSeconds += (float) m_iDelayTimes[ idx ];
	}

	timeSeconds /= 100.0f;

	//--------------------------------------------------------------------------


	float fHZ = 100.0f;

	float DopeSheetHeight = ImGui::GetFrameHeightWithSpacing() * 2 + 30;

	ImVec2 parentPosition = ImGui::GetWindowPos();
	ImVec2 parentSize     = ImGui::GetWindowSize();

	ImVec2 windowPosition = parentPosition;
	ImVec2 windowSize = parentSize;
	
	// Size
	windowSize.y = 80.0f;


	// Position
	windowPosition.y = parentPosition.y + parentSize.y - windowSize.y - (DopeSheetHeight + 24.0f);
	ImGui::SetNextWindowPos(windowPosition);

	ImGui::BeginChild("TimeLine", windowSize,
						  false,
						  ImGuiWindowFlags_NoMove |
						  ImGuiWindowFlags_NoScrollbar |
						  ImGuiWindowFlags_NoScrollWithMouse   |
						  ImGuiWindowFlags_AlwaysAutoResize );

	// Some Buttons
	Toolbar* toolBar = Toolbar::GToolbar;

	if (toolBar)
	{
		// Start of the Bar
		float xPos = 8.0f;
		ImGui::SameLine(xPos);

		// First Frame ---------------------------------------------------------
		if (toolBar->ImageButton(4,13))
		{
			m_bPlaying = false;
			m_iFrameNo = 0;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
				ImGui::Text("First");
			ImGui::EndTooltip();
		}

		ImGui::SameLine(xPos+=40.0f);

		// Back 1 Frame --------------------------------------------------------
		if (toolBar->ImageButton(0,13))
		{
			m_bPlaying = false;
			m_iFrameNo--;
			if (m_iFrameNo < 0)
			{
				m_iFrameNo = (int)m_pSurfaces.size() - 1;
			}
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
				ImGui::Text("Previous");
			ImGui::EndTooltip();
		}

		ImGui::SameLine(xPos+=40.0f);


		// Reverse Play --------------------------------------------------------
		if(toolBar->ImageButton(4,11))
		{
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
				ImGui::Text("Play\nBackwards");
			ImGui::EndTooltip();
		}
		ImGui::SameLine(xPos+=40.0f);

		// Forward Play --------------------------------------------------------
		if(toolBar->ImageButton(0,11,m_bPlaying))
		{
			m_bPlaying = !m_bPlaying;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text(m_bPlaying ? "Stop" : "Play");
			ImGui::EndTooltip();
		}
		ImGui::SameLine(xPos+=40.0f);

		// Forward 1 Frame -----------------------------------------------------

		if (toolBar->ImageButton(0,12))
		{
			m_bPlaying = false;
			m_iFrameNo++;
			if (m_iFrameNo >= m_pSurfaces.size())
			{
				m_iFrameNo = 0;
			}
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Next");
			ImGui::EndTooltip();
		}
		ImGui::SameLine(xPos+=40.0f);

		// Last Frame ----------------------------------------------------------

		if (toolBar->ImageButton(4,12))
		{
			m_bPlaying = false;
			m_iFrameNo = (int)m_pSurfaces.size() - 1;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Last");
			ImGui::EndTooltip();
		}
		ImGui::SameLine(xPos+=40.0f);

		// Other Options

		static int hz = 0;
		ImGui::SetNextItemWidth(64);
		ImGui::Combo("##Snap", &hz, "50HZ\0" "59.94\0" "60HZ\0" "100HZ\0\0");

		// Mode Buttons
		ImGui::SameLine(xPos+=64.0f);
		// Reposition Frame  (no changes to time)
		toolBar->ImageButton(0,14);
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Move\nReorder");
			ImGui::EndTooltip();
		}
		ImGui::SameLine(xPos+=40.0f);
		// Adjust single Frame Time (allows timeline to grow / shrink)
		toolBar->ImageButton(2,14);
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Frame Time");
			ImGui::EndTooltip();
		}
		ImGui::SameLine(xPos+=40.0f);
		// Stretch Shrink on the left
		//toolBar->ImageButton(6,14);
		//if (ImGui::IsItemHovered())
		//{
		//	ImGui::BeginTooltip();
		//	ImGui::Text("Scale Time Left");
		//	ImGui::EndTooltip();
		//}
		//ImGui::SameLine(xPos+=40.0f);
		// Position with Stretch Shrink all frames in the timeline
		toolBar->ImageButton(4,14);
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Scale Time");
			ImGui::EndTooltip();
		}
		ImGui::SameLine(xPos+=40.0f);
		// Stretch Shrink on the right
		//toolBar->ImageButton(0,15);
		//if (ImGui::IsItemHovered())
		//{
		//	ImGui::BeginTooltip();
		//	ImGui::Text("Scale Time Right");
		//	ImGui::EndTooltip();
		//}
		//ImGui::SameLine(xPos+=40.0f);
		// Cut
		toolBar->ImageButton(4,6);
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Cut");
			ImGui::EndTooltip();
		}
		ImGui::SameLine(xPos+=40.0f);
		// Copy
		toolBar->ImageButton(6,0);
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Copy");
			ImGui::EndTooltip();
		}
		ImGui::SameLine(xPos+=40.0f);
		// Paste
		toolBar->ImageButton(4,7);
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Paste");
			ImGui::EndTooltip();
		}
		ImGui::SameLine(xPos+=40.0f);

		// Keep Timeline in View
		toolBar->ImageButton(0,16);
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Timeline Follow");
			ImGui::EndTooltip();
		}
		ImGui::SameLine(xPos+=40.0f);



		ImGui::NewLine();
	}

	// Some Sliders
	ImGui::SetNextItemWidth(windowSize.x);
	int frameNumber = m_iFrameNo+1;

	if (ImGui::SliderInt("##SliderFrame", &frameNumber, 1, (int)m_pSurfaces.size(), "Frame %d"))
	{
		m_bPlaying = false;
		m_iFrameNo = frameNumber-1;
	}

	ImGui::SetNextItemWidth(windowSize.x);
	float timePosition = tickTimes[ m_iFrameNo ] / 100.0f;
	if (ImGui::SliderFloat("##SliderTime", &timePosition, 0.0f, timeSeconds, "Time %.3f"))
	{
		m_bPlaying = false;

		for (int idx = (int)(tickTimes.size()-1); idx >= 0; --idx)
		{
			if ((timePosition*100.0f) >= tickTimes[ idx ])
			{
				m_iFrameNo = idx;
				break;
			}
		}
	}

	ImGui::EndChild();

	// Dope Sheet

	ImGuiIO& io = ImGui::GetIO();

	ImGui::BeginChild("scrolling", ImVec2(0, DopeSheetHeight), true,
					  ImGuiWindowFlags_NoScrollWithMouse|
					  ImGuiWindowFlags_HorizontalScrollbar);

	//ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

	float x_padding = 8.0f;

	float step = 10.0f;
	int frameCountByTime = ((int)(timeSeconds * fHZ)) + 1;

	// Numbers on the Dope Sheet (this is what determines the scroll size)
	// since ImGui doesn't pay any attention to primitives I render below
	// the numbers
	for (int x = 0; x < frameCountByTime; x+=10)
	{
		ImGui::SameLine(x_padding + (x * step));
		ImGui::Text("%d", x);
	}

	// A little extra to make sure the last bit of time can be scrolled into
	// view
	ImGui::SameLine(x_padding + (frameCountByTime * step));
	ImGui::Text("");

	// Ruler on the Dope Sheet
	ImVec2 winPos = ImGui::GetWindowPos();
	winPos.y += 30.0f;
	winPos.x -= ImGui::GetScrollX();
	winPos.x += (x_padding);


	ImVec2 points[ 2 ];

	for (int x = 0; x < frameCountByTime; ++x)
	{
		ImVec2 pos = ImVec2(winPos.x + (x * step), winPos.y);
		points[ 0 ] = pos;
		if (x % 10)
		{
			if (x % 5)
			{
				pos.y += 15.0f;
			}
			else
			{
				pos.y += 22.0f;
			}
		}
		else
		{
			pos.y += 30.0f;
		}
		points[ 1 ] = pos;

		ImGui::GetWindowDrawList()->AddLine(
			points[ 0 ],
			points[ 1 ],
			0x80808080,
			0.5f
			);

	}

	// Frame Markers on the Dope Sheet

	winPos.x += 0.5f; // for my visual sanity

	// Track which index is closest to the mouse
	bool bHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

	int closestIndex = 0;
	ImVec2 closestPos(winPos.x, winPos.y + 30.0f);
	ImVec2 delta = ImVec2(closestPos.x - io.MousePos.x, closestPos.y - io.MousePos.y);
	float closestDist = (delta.x*delta.x) + (delta.y*delta.y);

	for (int idx = 0; idx < tickTimes.size(); ++idx)
	{
		ImVec2 pos = ImVec2(winPos.x + (step * tickTimes[idx]), winPos.y + 30.0f);
		if (m_iFrameNo == idx)
		{
			ImGui::GetWindowDrawList()->AddCircleFilled(pos, 5.0f, 0xC0C0C0C0);
		}
		else
		{
			ImGui::GetWindowDrawList()->AddCircle(pos, 5.0f, 0x80808080);
		}

		if (bHovered)
		{
			delta = ImVec2(pos.x - io.MousePos.x, pos.y - io.MousePos.y);
			float dist = (delta.x*delta.x) + (delta.y*delta.y);

			// only get the closest point, that is to to the left of the mouse
			if (delta.x <= 0.0f)
			if (dist < closestDist)
			{
				closestIndex = idx;
				closestDist = dist;
				closestPos = pos;
			}
		}
	}

	if (bHovered)
	{
		// Render out a highlight for the closest point on the timeline
		// A Hollow Circle
		ImGui::GetWindowDrawList()->AddCircle(closestPos, 6.0f, 0xFF00C000);

		ImVec2 nextPos = closestPos;
		nextPos.x += ((m_iDelayTimes[ closestIndex ] * step));
		nextPos.y -= 20.0f;

		// Vertical Line, above the bubble
		ImGui::GetWindowDrawList()->AddLine(ImVec2(closestPos.x, closestPos.y - 5.0f),
											ImVec2(closestPos.x, closestPos.y - 30.0f),
											0xF000C000,
											3.0f);


		// A line to repesent the time used
		ImGui::GetWindowDrawList()->AddLine(ImVec2(closestPos.x, nextPos.y),
											nextPos,
											0xC000C000,
											10.0f);

		// An End marker for the Time Used
		ImGui::GetWindowDrawList()->AddLine(ImVec2(nextPos.x, nextPos.y - 10.0f),
											ImVec2(nextPos.x, nextPos.y + 10.0f),
											0xF000C000,
											3.0f);

	}


	#if 0
	int lines = 2;
	for (int line = 0; line < lines; line++)
	{
		// Display random stuff (for the sake of this trivial demo we are using basic Button+SameLine. If you want to create your own time line for a real application you may be better off
		// manipulating the cursor position yourself, aka using SetCursorPos/SetCursorScreenPos to position the widgets yourself. You may also want to use the lower-level ImDrawList API)
		int num_buttons = 10 + ((line & 1) ? line * 9 : line * 3);
		for (int n = 0; n < num_buttons; n++)
		{
			if (n > 0) ImGui::SameLine();
			ImGui::PushID(n + line * 1000);
			char num_buf[16];
			sprintf(num_buf, "%d", n);
			const char* label = (!(n%15)) ? "FizzBuzz" : (!(n%3)) ? "Fizz" : (!(n%5)) ? "Buzz" : num_buf;
			float hue = n*0.05f;
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(hue, 0.6f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(hue, 0.7f, 0.7f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(hue, 0.8f, 0.8f));
			ImGui::Button(label, ImVec2(40.0f + sinf((float)(line + n)) * 20.0f, 0.0f));
			ImGui::PopStyleColor(3);
			ImGui::PopID();
		}
	}
	#endif

	//ImGui::PopStyleVar();
	ImGui::EndChild();

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

		if (m_pTargetSurfaces.size())
		{
			for (int idx = 0; idx < pSurfaces.size(); ++idx)
			{
				// Do not delete targets that don't exist
				// Do not delete a target, that is now becoming a source

				if ( (m_pTargetSurfaces.size() < idx) &&
					 (pSurfaces[idx] != m_pTargetSurfaces[idx]) )
				{
					// Free the pixels, because I allocated them?
					if (m_pTargetSurfaces[idx]->flags & SDL_PREALLOC)
					{
						free(m_pTargetSurfaces[idx]->pixels);
						m_pTargetSurfaces[idx]->pixels = nullptr;
					}

					SDL_FreeSurface(m_pTargetSurfaces[ idx ]);
					m_pTargetSurfaces[ idx ] = nullptr;

				}
			}
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

			int BytesPerPixel = pSurface->format->BytesPerPixel;

			Uint8 * pixel = (Uint8*)pSurface->pixels;
			pixel += (y * pSurface->pitch) + (x * BytesPerPixel);

			//
			//color = *((Uint32*)pixel);

			color  =  (Uint32)pixel[0];
			color |= ((Uint32)pixel[1]) << 8;
			color |= ((Uint32)pixel[2]) << 16;

			if( SDL_MUSTLOCK(pSurface) )
				SDL_UnlockSurface(pSurface);
		}
	}

	color |= 0xFF000000;  // We don't support Alpha
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
//------------------------------------------------------------------------------

unsigned char* ImageDocument::CreateC1Data(int frameNo)
{
	unsigned char *c1data = new unsigned char[ 0x8000 ];

// Copy of the C1 memory
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
	SDL_Surface* pImage = m_pTargetSurfaces.size() ? m_pTargetSurfaces[frameNo] : m_pSurfaces[frameNo];

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

	return c1data;
}

//------------------------------------------------------------------------------
// Save As GS Lzb Animation Format
// 
// Proposed replacement for Paintworks Animations
//
void ImageDocument::SaveGSLA(std::string filenamepath)
{
	// First Collect a list of C1 images
	// $C1 is just the 32KB blob of data, as it sits in the IIgs video memory
	std::vector<unsigned char*> c1Images;

	for (int frameIndex = 0; frameIndex < m_pSurfaces.size(); ++frameIndex)
	{
		c1Images.push_back( CreateC1Data( frameIndex ) );
	}

	// Now the gsla support class can do the heavy lifting

	GSLAFile anim(320,200, 0x8000);

	anim.AddImages(c1Images);

	anim.SaveToFile(filenamepath.c_str());

	#if 1
	{
		// Verify the conversion is good
		// Load the file back in
		GSLAFile verify(filenamepath.c_str());

		const std::vector<unsigned char *> &frames = verify.GetPixelMaps();

		for (int idx = 0; idx < frames.size(); ++idx)
		{
			int result = memcmp(c1Images[idx % c1Images.size()], frames[idx], verify.GetFrameSize());
			LOG("Verify Frame %d - %s\n", idx, result ? "Failed" : "Good");
		}
	}
	#endif

	// Free the memory
	for (int idx = 0; idx < c1Images.size(); ++idx)
	{
		delete[] c1Images[idx];
		c1Images[idx] = nullptr;
	}


}

//------------------------------------------------------------------------------
// Save As Paintworks Preferred File Format
// which was has a very simple design
//
void ImageDocument::SaveC2(std::string filenamepath)
{
	// First Collect a list of C1 images
	// $C1 is just the 32KB blob of data, as it sits in the IIgs video memory
	std::vector<unsigned char*> c1Images;
	std::vector<unsigned char> c2Data;

	for (int frameIndex = 0; frameIndex < m_pSurfaces.size(); ++frameIndex)
	{
		c1Images.push_back( CreateC1Data( frameIndex ) );
	}

	// Then Serialize out a valid C2 animation into memory

	unsigned char* pPrevFrame = c1Images[0];

	// The first 32k of a C2 Animation, is just a regular uncompressed
	// $C1 image
	c2Data.resize( 0x8000 );
	memcpy( &c2Data[0], pPrevFrame, 0x8000);

	// Insert space for the length of the animation
	c2Data.resize( c2Data.size() + sizeof(unsigned int));

	// Tick Rate of this file (it will be 1)
	// ultimately to support variable rate, I will inject blank frames
	c2Data.push_back(1);
	c2Data.push_back(0);
	c2Data.push_back(0);
	c2Data.push_back(0);

	// This is some kind of length, that we just set to it's own size
	c2Data.push_back(4);
	c2Data.push_back(0);
	c2Data.push_back(0);
	c2Data.push_back(0);

	for (int frameIndex = 1; frameIndex < c1Images.size(); ++frameIndex)
	{
		unsigned char* pCurrentFrame = c1Images[ frameIndex ];

		std::vector<unsigned char> bytes = C2EncodeFrame(pPrevFrame, pCurrentFrame);

		pPrevFrame = pCurrentFrame;

		size_t offset = c2Data.size();

		// Make room for new frame
		c2Data.resize(c2Data.size() + bytes.size());

		// Insert the frame
		memcpy(&c2Data[ offset ], &bytes[0], bytes.size());
	}

	// If there are no frames, insert a blank frame
	if (c1Images.size() <= 1)
	{
		// Insert end of frame code
		c2Data.push_back(0x00);
		c2Data.push_back(0x00);
		c2Data.push_back(0xFF);
		c2Data.push_back(0xFF);
	}

	// update the length field
	int length = (int)c2Data.size();
	length -= 0x8008;

	c2Data[0x8000] = (unsigned char) ((length>> 0) & 0xFF);
	c2Data[0x8001] = (unsigned char) ((length>> 8) & 0xFF);
	c2Data[0x8002] = (unsigned char) ((length>>16) & 0xFF);
	c2Data[0x8003] = (unsigned char) ((length>>24) & 0xFF);

	// then finally write it out to a file

	{
		FILE* file = fopen(filenamepath.c_str(), "wb");

		fwrite(&c2Data[0], 1, c2Data.size(), file);

		fclose(file);
	}

	// We need to free the C1Data here since it's not a proper object
	while (c1Images.size())
	{
		delete[] c1Images[0];
		c1Images.erase(c1Images.begin());
	}
}

//------------------------------------------------------------------------------
std::vector<unsigned char> ImageDocument::C2EncodeFrame(unsigned char* pPrev, unsigned char* pNext)
{
	std::vector<unsigned char> bytes;

	// Constants
	const int PixelBytes    = 0x7DC8; // pixels + 200 SCBS
	const int PaletteOffset = 0x7E00; // palette offset
	const int PaletteBytes  = 0x200;  // number of bytes in the palette

	// Encode Pixels + SCBs
	for (int index = 0; index < PixelBytes; ++index)
	{
		if (pPrev[ index ] != pNext[ index ])
		{
			// Save out the delta cursor position in memory
			bytes.push_back( (unsigned char) (index & 0xFF ));
			bytes.push_back( (unsigned char) ((index>>8) & 0xFF));

	        // save out the data that change in the position
		    bytes.push_back( pNext[ index ] );
			index+=1;
			bytes.push_back( pNext[ index ] );
		}
	}

	// note, we are skipping the screen hole (between the SCBs and the Palette)

	// Encode the palettes separate, so that colors can be updated in lockstep
	// as best we can

	for (int index = PaletteOffset; index < (PaletteOffset + PaletteBytes); index+=2)
	{
		if ((pPrev[ index ] != pNext[ index ])|| (pPrev[index+1] != pNext[index+1]))
		{
			// We have a delta, save out the index
			bytes.push_back( (unsigned char) (index & 0xFF));
			bytes.push_back( (unsigned char) ((index>>8) & 0xFF));

			// Save out the, data
            bytes.push_back( (unsigned char) pNext[ index + 0 ]);
			bytes.push_back( (unsigned char) pNext[ index + 1 ]);
		}
	}

	// Insert end of frame code
	bytes.push_back(0x00);
	bytes.push_back(0x00);
	bytes.push_back(0xFF);
	bytes.push_back(0xFF);

	return bytes;
}

//------------------------------------------------------------------------------
void ImageDocument::SaveC1(std::string filenamepath)
{
// Copy of the C1 memory
	unsigned char* c1data;

	c1data = CreateC1Data(m_iFrameNo);
	
	// Serialize to disk
	if (c1data)
	{
		FILE* file = fopen(filenamepath.c_str(), "wb");

		fwrite(c1data, 1, 0x8000, file);

		fclose(file);

		delete[] c1data;
	}

}
//------------------------------------------------------------------------------

// For now, I'm just making this easy
// and using what SDL gave me

void ImageDocument::SavePNG(std::string filenamepath)
{
// Choose a surface to save
	SDL_Surface* pImage = m_pTargetSurfaces.size() ? m_pTargetSurfaces[0] : m_pSurfaces[0];

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

	bool bFixed = false;

	for (int rawIndex = 0; rawIndex < pRawPixels.size(); ++rawIndex)
	{
		//$$JGA Fixed Colors can be added to the input_image
		//$$JGA which is going to be sweet

		liq_image *input_image = liq_image_create_rgba(attr_handle, pRawPixels[ rawIndex ], width, height, 0);

		if (!bFixed)
		{
			bFixed = true;
			// Only add Fixed Colors to the first image
		
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

	#if 1
	std::vector<unsigned char> pixel_swaps;

	{
		// Probably shouldn't do this, but I'd like the libimage quant to just
		// remap stuff in the correct order to start, to monkey with order
		int max_blocks = (int)m_bLocks.size();
		if (max_blocks > (int)palette->count)
		{
			max_blocks = (int)palette->count;
		}

		int LockCount = 0;

		for (int idx = 0; idx < max_blocks; ++idx)
		{
			if (m_bLocks[ idx ])
			{
				LockCount++;
			}
		}

		int palette_offset = palette->count - LockCount;

		for (int LockIndex = 0; LockIndex < LockCount; ++LockIndex)
		{
			if (m_bLocks[ LockIndex ])
			{
				liq_color temp  = palette->entries[palette_offset];
				liq_color temp2 = palette->entries[LockIndex];

				memcpy((void*)&palette->entries[LockIndex],&temp, sizeof(liq_color));
				memcpy((void*)&palette->entries[palette_offset],&temp2, sizeof(liq_color));

				pixel_swaps.push_back((unsigned char) palette_offset);
				pixel_swaps.push_back((unsigned char) LockIndex);

				palette_offset++;
			}
		}

	}
	#endif


    size_t pixels_size = width * height;
	liq_set_dithering_level(quantization_result, m_iDither / 100.0f);  // 0.0->1.0
//	liq_set_output_gamma(quantization_result, 1.0);

	std::vector<SDL_Surface*> pResults;

	for (int idx = 0; idx < input_images.size(); ++idx)
	{
		unsigned char *raw_8bit_pixels = (unsigned char*)malloc(pixels_size);

		liq_write_remapped_image(quantization_result, input_images[ idx ], raw_8bit_pixels, pixels_size);

		// Fix up pixels
		for (int pixel_index = 0; pixel_index < pixels_size; ++pixel_index)
		{
			unsigned char pixel = raw_8bit_pixels[ pixel_index ];

			for (int swap_index = 0; swap_index < pixel_swaps.size(); swap_index+=2)
			{
				if (pixel == pixel_swaps[ swap_index ])
				{
					raw_8bit_pixels[ pixel_index ] = pixel_swaps[ swap_index + 1 ];
				}
				else if (pixel == pixel_swaps[ swap_index+1])
				{
					raw_8bit_pixels[ pixel_index ] = pixel_swaps[ swap_index ];
				}
			}
		}

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
		int max_blocks = (int)m_bLocks.size();

		if (max_blocks > m_iTargetColorCount)
		{
			max_blocks = m_iTargetColorCount;
		}

		for (int idx = 0; idx < max_blocks; ++idx)
		{
			liq_color color;

			color = palette->entries[idx];

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
    //m_pTargetSurface = m_pTargetSurfaces[0];

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
// Save as Foenix Animation
//
void ImageDocument::SaveFAN(std::string filenamepath, bool bTiled)
{
// Choose a surface to save
	SDL_IMG_SaveFAN(m_pTargetSurfaces, filenamepath.c_str(), bTiled);
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Save as Foenix Bitmap
//
void ImageDocument::Save256(std::string filenamepath)
{
// Choose a surface to save
	SDL_IMG_Save256(m_pTargetSurfaces, filenamepath.c_str());
}
//------------------------------------------------------------------------------


void ImageDocument::RotateRight()
{
	// Results
	std::vector<SDL_Surface*> pImages;

	for (int idx = 0; idx < m_pSurfaces.size(); ++idx)
	{
		Uint32* pSourcePixels = SDL_SurfaceToUint32Array(m_pSurfaces[idx]);
		Uint32 *pDestPixels = new Uint32[m_height * m_width];

		int SourceWidth = m_width;
		int SourceHeight = m_height;
		int DestWidth  = m_height;
		int DestHeight = m_width;

		// Do 90 degree clockwise copy
		for (int DestY = 0; DestY < DestHeight; ++DestY)
		{
			for (int DestX = 0; DestX < DestWidth; ++DestX)
			{
				int DestIndex   = (DestY * DestWidth) + DestX;

				int SourceX = DestY;
				int SourceY = (SourceHeight - 1) - DestX;

				int SourceIndex = (SourceY * SourceWidth) + SourceX;

				pDestPixels[ DestIndex ] = pSourcePixels[ SourceIndex ];
			}
		}

		delete[] pSourcePixels;
		pSourcePixels = nullptr;

		SDL_Surface* pSurface = SDL_SurfaceFromRawRGBA(pDestPixels, DestWidth, DestHeight);

		pImages.push_back(pSurface);

		delete[] pDestPixels;
		pDestPixels = nullptr;
	}

	SetDocumentSurface( pImages );
}

//------------------------------------------------------------------------------

void ImageDocument::RotateLeft()
{
	// Results
	std::vector<SDL_Surface*> pImages;

	for (int idx = 0; idx < m_pSurfaces.size(); ++idx)
	{
		Uint32* pSourcePixels = SDL_SurfaceToUint32Array(m_pSurfaces[idx]);
		Uint32 *pDestPixels = new Uint32[m_height * m_width];

		int SourceWidth = m_width;
		int SourceHeight = m_height; (void)SourceHeight;
		int DestWidth  = m_height;
		int DestHeight = m_width;

		// Do 90 degree clockwise copy
		for (int DestY = 0; DestY < DestHeight; ++DestY)
		{
			for (int DestX = 0; DestX < DestWidth; ++DestX)
			{
				int DestIndex   = (DestY * DestWidth) + DestX;

				int SourceX = (SourceWidth - 1) - DestY;
				int SourceY = DestX;

				int SourceIndex = (SourceY * SourceWidth) + SourceX;

				pDestPixels[ DestIndex ] = pSourcePixels[ SourceIndex ];
			}
		}

		delete[] pSourcePixels;
		pSourcePixels = nullptr;

		SDL_Surface* pSurface = SDL_SurfaceFromRawRGBA(pDestPixels, DestWidth, DestHeight);

		pImages.push_back(pSurface);

		delete[] pDestPixels;
		pDestPixels = nullptr;
	}

	SetDocumentSurface( pImages );
}

//------------------------------------------------------------------------------

void ImageDocument::MirrorHorizontal()
{
	// Results
	std::vector<SDL_Surface*> pImages;

	for (int idx = 0; idx < m_pSurfaces.size(); ++idx)
	{
		Uint32* pSourcePixels = SDL_SurfaceToUint32Array(m_pSurfaces[idx]);
		Uint32 *pDestPixels = new Uint32[m_width * m_height];

		int SourceWidth = m_width;
		int SourceHeight = m_height; (void)SourceHeight;
		int DestWidth  = m_width;
		int DestHeight = m_height;

		// Do 90 degree clockwise copy
		for (int DestY = 0; DestY < DestHeight; ++DestY)
		{
			for (int DestX = 0; DestX < DestWidth; ++DestX)
			{
				int DestIndex   = (DestY * DestWidth) + DestX;

				int SourceX = (SourceWidth-1) - DestX;
				int SourceY = DestY;

				int SourceIndex = (SourceY * SourceWidth) + SourceX;

				pDestPixels[ DestIndex ] = pSourcePixels[ SourceIndex ];
			}
		}

		delete[] pSourcePixels;
		pSourcePixels = nullptr;

		SDL_Surface* pSurface = SDL_SurfaceFromRawRGBA(pDestPixels, DestWidth, DestHeight);

		pImages.push_back(pSurface);

		delete[] pDestPixels;
		pDestPixels = nullptr;
	}

	SetDocumentSurface( pImages );
}

//------------------------------------------------------------------------------

void ImageDocument::MirrorVertical()
{
	// Results
	std::vector<SDL_Surface*> pImages;

	for (int idx = 0; idx < m_pSurfaces.size(); ++idx)
	{
		Uint32* pSourcePixels = SDL_SurfaceToUint32Array(m_pSurfaces[idx]);
		Uint32 *pDestPixels = new Uint32[m_width * m_height];

		int SourceWidth = m_width;
		int SourceHeight = m_height;
		int DestWidth  = m_width;
		int DestHeight = m_height;

		// Do 90 degree clockwise copy
		for (int DestY = 0; DestY < DestHeight; ++DestY)
		{
			for (int DestX = 0; DestX < DestWidth; ++DestX)
			{
				int DestIndex   = (DestY * DestWidth) + DestX;

				int SourceX = DestX;
				int SourceY = (SourceHeight-1) - DestY;

				int SourceIndex = (SourceY * SourceWidth) + SourceX;

				pDestPixels[ DestIndex ] = pSourcePixels[ SourceIndex ];
			}
		}

		delete[] pSourcePixels;
		pSourcePixels = nullptr;

		SDL_Surface* pSurface = SDL_SurfaceFromRawRGBA(pDestPixels, DestWidth, DestHeight);

		pImages.push_back(pSurface);

		delete[] pDestPixels;
		pDestPixels = nullptr;
	}

	SetDocumentSurface( pImages );
}

//------------------------------------------------------------------------------

