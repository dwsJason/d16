// PaletteDocument Class Implementation

#include "paldoc.h"

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

//------------------------------------------------------------------------------
// Statics
std::vector<PaletteDocument*> PaletteDocument::GDocuments;
// Private Statics
PaletteDocument* PaletteDocument::g_pDoc;  // Document to do a command upon
int PaletteDocument::g_iCommand;		     // enum, which command
//------------------------------------------------------------------------------

PaletteDocument::PaletteDocument(std::string filename, std::string pathname)
	: m_filename(filename)
	, m_pathname(pathname)
{
	FILE* pFile = fopen(pathname.c_str(), "rb");

	unsigned int colors[16] = {0};

	for (int idx = 0; idx < 16; ++idx)
	{
		fread(&colors[idx], 3, 1, pFile);
	}

	//size_t numread = fread(colors, sizeof(unsigned int), 16, pFile);
	size_t numread = 16;

	fclose(pFile);

	//LOG("Read in %d Colors\n", (int)numread);

	for (int idx = 0; idx < numread; ++idx)
	{
		m_colors.push_back( colors[ idx ] | 0xFF000000 );

		unsigned int icolor = m_colors[idx];
		ImVec4 temp_color;
		temp_color.x = (icolor>>0  & 0xFF) / 255.0f;
		temp_color.y = (icolor>>8  & 0xFF) / 255.0f;
		temp_color.z = (icolor>>16 & 0xFF) / 255.0f;
		temp_color.w = (icolor>>24 & 0xFF) / 255.0f;

		m_floatColors.push_back(temp_color);
	}
}

PaletteDocument::~PaletteDocument()
{
}

//------------------------------------------------------------------------------

void PaletteDocument::Render()
{
	ImGui::BeginChild(m_filename.c_str(), ImVec2(340, 56), true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);


		if (ImGui::BeginPopupContextWindow(nullptr))
		{
			ImGui::Text(m_filename.c_str());
			ImGui::Separator();
			ImGui::Separator();

			ImGui::TextDisabled("Duplicate");
			#if 0
			if (ImGui::MenuItem("Duplicate"))
			{
				g_pDoc = this;
				g_iCommand = CMD_DUPLICATE;
			}
			#endif

			ImGui::Separator();
			ImGui::Separator();

			if (ImGui::MenuItem("Move Up"))
			{
				g_pDoc = this;
				g_iCommand = CMD_MOVE_UP;
			}
			if (ImGui::MenuItem("Move Down"))
			{
				g_pDoc = this;
				g_iCommand = CMD_MOVE_DOWN;
			}
			if (ImGui::MenuItem("Move Top"))
			{
				g_pDoc = this;
				g_iCommand = CMD_MOVE_TOP;
			}
			if (ImGui::MenuItem("Move Bottom"))
			{
				g_pDoc = this;
				g_iCommand = CMD_MOVE_BOTTOM;
			}

			ImGui::Separator();
			ImGui::Separator();
			#if 1
			ImGui::TextDisabled("Rename");
			ImGui::TextDisabled("Save as...");
			#else

			if (ImGui::MenuItem("Rename"))
			{
				g_pDoc = this;
				g_iCommand = CMD_RENAME;
			}
			if (ImGui::MenuItem("Save as..."))
			{
				g_pDoc = this;
				g_iCommand = CMD_SAVE_AS;
			}
			#endif

			ImGui::Separator();
			ImGui::Separator();

			if (ImGui::MenuItem("Close"))
			{
				g_pDoc = this;
				g_iCommand = CMD_CLOSE;
			}
			ImGui::EndPopup();
		}



		if (ImGui::BeginDragDropSource())
		{
			if (ImGui::SetDragDropPayload("xPalette16", &m_floatColors[0], 4*4*16, ImGuiCond_Once))
			{
				// I don't really understand what I'm supposed to do here
			}

			// Drag Preview Content
			ImGui::Text(m_filename.c_str());
			ImGui::NewLine();

			ImVec2 buttonSize = ImVec2(20,20);

			for (int idx = 0; idx < m_colors.size(); ++idx)
			{
				ImGui::SameLine(10.0f + (idx * buttonSize.x));

				std::string colorId = m_filename + "##" + std::to_string(idx);
				ImGui::ColorButton(colorId.c_str(), m_floatColors[idx],
								   ImGuiColorEditFlags_NoLabel |
								   ImGuiColorEditFlags_NoAlpha |
								   ImGuiColorEditFlags_NoBorder, buttonSize );

			}

			ImGui::EndDragDropSource();                                                                    // only call EndDragDropSource() if BeginDragDropSource() returns true!
		}

		ImGui::Text(m_filename.c_str());
		ImGui::NewLine();

		ImVec2 buttonSize = ImVec2(20,20);

		for (int idx = 0; idx < m_colors.size(); ++idx)
		{
			ImGui::SameLine(10.0f + (idx * buttonSize.x));

			std::string colorId = m_filename + "##" + std::to_string(idx);
			ImGui::ColorButton(colorId.c_str(), m_floatColors[idx],
							   ImGuiColorEditFlags_NoLabel |
							   ImGuiColorEditFlags_NoAlpha |
							   ImGuiColorEditFlags_NoBorder, buttonSize );

			// Allow user to drop colors into each palette entry
			// (Note that ColorButton is already a drag source by default, unless using ImGuiColorEditFlags_NoDragDrop)
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
					memcpy((float*)&m_floatColors[idx], payload->Data, sizeof(float) * 3);
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
					memcpy((float*)&m_floatColors[idx], payload->Data, sizeof(float) * 4);
				ImGui::EndDragDropTarget();
			}

		}
		ImGui::NewLine();

	ImGui::EndChild();
}

//------------------------------------------------------------------------------
//
//  I want this in here, so that I can hide all the list manipulation
//  over here, instead of in the main.cpp
//
/*static*/ void PaletteDocument::GRender()
{
	for (int idx = 0; idx < PaletteDocument::GDocuments.size(); ++idx)
		GDocuments[ idx ]->Render();

	// Support for the Context menu commands
	if (g_pDoc)
	{
		PaletteDocument* pDoc = g_pDoc;
		g_pDoc = nullptr;

		int docIndex = IndexOf( pDoc );

		// Basically, do we have a valid document
		if (docIndex >= 0)
		{
			switch (g_iCommand)
			{
			case CMD_DUPLICATE:
				break;

			case CMD_MOVE_UP:
				if (docIndex > 0)
				{
					GDocuments[ docIndex ] = GDocuments[ docIndex - 1 ];
					GDocuments[ docIndex - 1 ] = pDoc;
				}
				break;

			case CMD_MOVE_DOWN:
				if (docIndex < (GDocuments.size() - 1))
				{
					GDocuments[ docIndex ] = GDocuments[ docIndex + 1 ];
					GDocuments[ docIndex + 1 ] = pDoc;
				}
				break;

			case CMD_MOVE_TOP:
				if (docIndex > 0)
				{
					GDocuments.erase(GDocuments.begin() + docIndex);
					GDocuments.insert(GDocuments.begin(), pDoc);
				}
				break;
			case CMD_MOVE_BOTTOM:
				if (docIndex < (GDocuments.size() - 1))
				{
					GDocuments.erase(GDocuments.begin() + docIndex);
					GDocuments.insert(GDocuments.end(), pDoc);
				}
				break;

			case CMD_RENAME:
			case CMD_SAVE_AS:
				break;

			case CMD_CLOSE:
				{
					GDocuments.erase(GDocuments.begin() + docIndex);
					delete pDoc;
					pDoc = nullptr;
				}
				break;

			default:
				break;
			}
		}
	}
}

//------------------------------------------------------------------------------

/*static*/ int PaletteDocument::IndexOf(PaletteDocument* pDoc)
{
	for (int idx = 0; idx < GDocuments.size(); ++idx)
	{
		if (GDocuments[ idx ] == pDoc)
		{
			return idx;
		}
	}

	return -1;
}

//------------------------------------------------------------------------------

