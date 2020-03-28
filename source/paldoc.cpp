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
	ImGui::BeginChild(m_filename.c_str(), ImVec2(500, 56), true, ImGuiWindowFlags_NoMove);

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
		ImGui::NewLine();

	ImGui::EndChild();
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

