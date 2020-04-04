//
// Toolbar - My Dear ImGUI Toolbar Window
//
#include "toolbar.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include "log.h"


// This bit here is also dumb
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

// Again, this should be in some sort of helper, or util header
extern GLuint
SDL_GL_LoadTexture(SDL_Surface * surface, GLfloat * texcoord);

//------------------------------------------------------------------------------

Toolbar::Toolbar()
	: m_GLImage(0)
{
	// So I think this should not be in a file, it should be linked in, maybe
	// an XPM image like the eye dropper?

	// Get Icons loaded into an OpenGL Texture
	SDL_Surface* pImage = IMG_Load(".\\data\\buttons.png");

	if (pImage)
	{
		m_GLImage = SDL_GL_LoadTexture(pImage, m_UV);
		SDL_FreeSurface(pImage);
	}
	else
	{
		LOG("ERR IMG_Load: %s\n", IMG_GetError());
		LOG("Toolbar Class missing buttons.png, unable to instantiate\n");
	}

	if (pImage)
	{
		// Go ahead and add the buttons we support to the toolbar

	}
}

Toolbar::~Toolbar()
{

}

//------------------------------------------------------------------------------

void Toolbar::Render()
{
	// Don't bother, if we don't have the icons
	if (!m_GLImage) return;

	//ImGui::BeginChild("Toolbar", ImVec2(20*2*3, 12*2), true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
	//ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar |
	//								 ImGuiWindowFlags_NoScrollbar |
	//   							 ImGuiWindowFlags_NoCollapse |
	//								 ImGuiWindowFlags_AlwaysAutoResize);

	ImVec4 bg_color = ImVec4(0,0,0,0);
	ImVec4 tint_color = ImVec4(1,1,1,1);
	// Draw the buttons
	// These are 20x12 - weird but that's what DreamGrafix uses
	ImVec2 buttonSize = ImVec2(20*2,12*2);

	ImVec2 uv0 = ImVec2(0,0);
	ImVec2 uv1 = ImVec2(20.0f/256.0f,12.0f/256.0f);
	//ImVec4 tintColor = ImVec4(1,1,1,1);


	uv0.x = 1.0f*20.0f/256.0f;
	uv1.x = uv0.x + 20.0f/256.0f;

	uv0.y = 6.0f*12.0f/256.0f;
	uv1.y = uv0.y + 12.0f/256.0f;

	float xpos = 128.0f;
	ImGui::SameLine(xpos); xpos+=40.0f;
	ImGui::ImageButton((ImTextureID)m_GLImage, buttonSize, uv0, uv1, 0, bg_color, tint_color);
	
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
			ImGui::Text("Pan and Zoom");
		ImGui::EndTooltip();
	}


	uv0.x = 0.0f*20.0f/256.0f;
	uv1.x = uv0.x + 20.0f/256.0f;

	uv0.y = 8.0f*12.0f/256.0f;
	uv1.y = uv0.y + 12.0f/256.0f;
		
	ImGui::SameLine(xpos); xpos+=40.0f;
	ImGui::ImageButton((ImTextureID)m_GLImage, buttonSize, uv0, uv1, 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
			ImGui::Text("Eye Dropper");
		ImGui::EndTooltip();
	}

	uv0.x = 6.0f*20.0f/256.0f;
	uv1.x = uv0.x + 20.0f/256.0f;
	uv0.y = 9.0f*12.0f/256.0f;
	uv1.y = uv0.y + 12.0f/256.0f;
		
	ImGui::SameLine(xpos); xpos+=40.0f;
	ImGui::ImageButton((ImTextureID)m_GLImage, buttonSize, uv0, uv1, 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
			ImGui::Text("Resize Image");
		ImGui::EndTooltip();
	}

	//ImGui::End();
	//ImGui::EndChild();
}

//------------------------------------------------------------------------------

