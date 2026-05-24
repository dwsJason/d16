    // dear imgui: standalone example application for SDL2 + OpenGL
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)
// (GL3W is a helper library to access OpenGL functions since there is no standard header to access modern OpenGL functions easily. Alternatives are GLEW, Glad, etc.)

#include "imgui.h"
#include "imgui_internal.h" // required for dock builder
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include "log.h"
#include <nfd.hpp>  // needed for the native file dialog
#include "imagedoc.h"
#include "paldoc.h"
#ifdef _WIN32
#include "dirent.h"
#else
#include <dirent.h>
#endif
#include "toolbar.h"
#include "obj_file.h"
#include "xu_line.h"

#include "d16.h"

// My SDL Helper Functions file
#include "sdl_helpers.h"

void SetWindowIcon( SDL_Window* pIcon );
void ShowLog();
void DockSpaceUI();
void ToolBarUI();
void MainMenuBarUI();

//------------------------------------------------------------------------------
// Local helper functions

static std::string toLower(const std::string s)
{
	std::string result = s;

	for (int idx = 0; idx < result.size(); ++idx)
	{
		result[ idx ] = (char)tolower(result[idx]);
	}

	return result;
}

// Case Insensitive
static bool endsWith(const std::string& S, const std::string& SUFFIX)
{
	bool bResult = false;

	std::string s = toLower(S);
	std::string suffix = toLower(SUFFIX);

    bResult = s.rfind(suffix) == (s.size()-suffix.size());

	return bResult;
}
//------------------------------------------------------------------------------
static int alphaSort(const struct dirent **a, const struct dirent **b)
{
	return strcoll((*a)->d_name, (*b)->d_name);
}
//------------------------------------------------------------------------------
std::vector<ImageDocument*>   imageDocuments;

Toolbar* pToolbar = nullptr; // need to create after stuff intialize

bool bAppDone = false; // Set true to quit App

	bool show_log_window = true;
	bool show_palette_window = true;

//------------------------------------------------------------------------------

// Main code
int main(int, char**)
{
    // initialize NFD
    NFD::Guard nfdGuard;

    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        SYSERROR("Error: %s\n", SDL_GetError());
        return -1;
    }

	// load support for the JPG and PNG image formats
	int flags=IMG_INIT_JPG|IMG_INIT_PNG|IMG_INIT_TIF|IMG_INIT_WEBP;
	int initted=IMG_Init(flags);
	if((initted&flags) != flags)
	{
		SYSERROR("IMG_Init: Failed to init required jpg and png support!\n"
		         "IMG_Init: %s\n", IMG_GetError());
		// handle error
	}

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags sdl_window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dream16", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, sdl_window_flags);

	// Get that Awesome Icon in there
	SetWindowIcon( window );

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();

	//io.Fonts->AddFontFromFileTTF("./data/Inconsolata.otf", 15.0f);

	// Sanity-check the font file before handing it to imgui.  The 1.91.3
	// error-tooltip path crashes if AddFontFromFileTTF can't read the file,
	// so we'd rather catch that ourselves and fall back to the default font.
	const char* shaston_path = "./data/ShastonHi640.ttf";
	FILE* shaston_file = fopen(shaston_path, "rb");
	if (shaston_file)
	{
		fclose(shaston_file);
		io.Fonts->AddFontFromFileTTF(shaston_path, 16.0f);
	}
	else
	{
		fprintf(stderr,
			"d16: could not open '%s' (cwd should be the project root).\n"
			"     Falling back to the imgui default font.\n",
			shaston_path);
		io.Fonts->AddFontDefault();
	}

	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
#ifdef _DEBUG
    bool show_demo_window = false;
    bool show_another_window = false;
#endif

    ImVec4 clear_color = ImVec4(0.18f, 0.208f, 0.38f, 1.00f);

	//--------------------------------------------------------------------------
	//--------------------------------------------------------------------------

	LOG("Dream16 Compiled %s %s\n", __DATE__, __TIME__);

	{
		// Scan preset palette directory
		LOG("Load Preset Palettes\n");

		std::string vPath = "./data/palettes";
		struct dirent **files = nullptr;
		int fileCount = scandir(vPath.c_str(), &files, nullptr, alphaSort);

		for (int idx = 0; idx < fileCount; ++idx)
		{
			struct dirent *ent = files[idx];

			if (DT_REG == ent->d_type)
			{
				std::string filename = ent->d_name;

				if (endsWith(filename, ".pal"))
				{
					LOG("%s\n", filename.c_str());
					PaletteDocument::GDocuments.push_back(new PaletteDocument(filename, vPath+"/"+filename ));
				}
			}
		}
	}

	// Get that toolbar created

	pToolbar = new Toolbar();

    // Main loop
    bAppDone = false;
    while (!bAppDone)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                bAppDone = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                bAppDone = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

		MainMenuBarUI();
		ToolBarUI();
		// Put everything in a DockSpace, because it's just cool
		DockSpaceUI();

		// Render the imageDocuments

		for (int idx = 0; idx < imageDocuments.size(); ++idx)
		{
			imageDocuments[ idx ]->Render();

			// Delete the document if the user closes it
			if (imageDocuments[ idx ]->IsClosed())
			{
				delete imageDocuments[ idx ];
				imageDocuments.erase(imageDocuments.begin() + idx);
				idx--; // Compensate index for the window being removed
			}
		}

		// Render the Palette Window

		if (show_palette_window)
		{
			ImGui::Begin("Palettes", &show_palette_window);

			PaletteDocument::GRender();

			ImGui::End();
		}

		if (show_log_window)
		{
			ShowLog();
		}

#ifdef _DEBUG
        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }
#endif

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
	IMG_Quit();
    SDL_Quit();

    return 0;
}

//------------------------------------------------------------------------------
void ShowLog()
{
    // For the demo: add a debug button _BEFORE_ the normal log window contents
    // We take advantage of a rarely used feature: multiple calls to Begin()/End() are appending to the _same_ window.
    // Most of the contents of the window will be added by the log.Draw() call.
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    // Actually call in the regular Log helper (which will Begin() into the same window as we just did)
    Log::GLog.Draw("Dream16 Log");
}
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
const float toolbarSize = 32;

// Signal to rebuild the dock
bool G_RebuildDock = false;

void DockSpaceUI()
{
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
	ImGuiWindowFlags window_flags = /*ImGuiWindowFlags_MenuBar |*/ ImGuiWindowFlags_NoDocking;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImVec2 WorkPos = viewport->WorkPos;
	WorkPos.y += toolbarSize;
	ImVec2 WorkSize = viewport->WorkSize;
	WorkSize.y -= toolbarSize;

	ImGui::SetNextWindowPos(WorkPos);
	ImGui::SetNextWindowSize(WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace", nullptr, window_flags);
	ImGui::PopStyleVar();

	ImGui::PopStyleVar(2);

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

	static bool bFirstTime = true;

	if (bFirstTime)
	{
		bFirstTime = false;

		//$$TODO - Add code to preserve the current UI state, so when it rebuilds
		// it matches what we have right now, instead of doing a full reset

		// Clear out existing layout
		ImGui::DockBuilderRemoveNode(dockspace_id);
		// Add empty node
		ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
		// Main node should cover entire window
		ImGui::DockBuilderSetNodeSize(dockspace_id, WorkSize);
		// get id of main dock space area
		ImGuiID dockspace_main_id = dockspace_id;
		// Create a dock node for the right docked window
		ImGuiID right  = ImGui::DockBuilderSplitNode(dockspace_main_id, ImGuiDir_Right, 0.27f, nullptr, &dockspace_main_id);
		ImGuiID bottom = ImGui::DockBuilderSplitNode(dockspace_main_id, ImGuiDir_Down, 0.25f, nullptr, &dockspace_main_id);

		ImGui::DockBuilderDockWindow("Palettes", right);
		ImGui::DockBuilderDockWindow("Dream16 Log", bottom);

		// Dock all the elements
		for (int idx = 0; idx < imageDocuments.size(); ++idx)
		{
			ImageDocument* pImageDoc = imageDocuments[idx];
			ImGui::DockBuilderDockWindow(pImageDoc->WindowName(), dockspace_main_id);
		}

		ImGui::DockBuilderFinish(dockspace_id);

	}

	//
	// The ImageDocument Windows must exist, before we dock them
	// so we need to wait 1 frame from when a Rebuild is Requested
	// stinky IMGUI
	//
	if (G_RebuildDock)
	{
		bFirstTime = true;
		G_RebuildDock = false;
	}

	#if 0
	// Check to see if we need to add any documents to the dock
	for (int idx = 0; idx < imageDocuments.size(); ++idx)
	{
		ImageDocument* pImageDoc = imageDocuments[idx];

		if (pImageDoc->IsNew())
		{
			ImGui::DockBuilderDockWindow(pImageDoc->WindowName(), G_DockSpaceMainID);
		}

		ImGui::DockBuilderFinish(ImGui::GetID("MyDockSpace"));
	}
	#endif

	// Render DockSpace
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

//---
	ImGui::End();



}

void ToolBarUI()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImVec2 WorkSize = viewport->WorkSize;
	WorkSize.y = toolbarSize;
	ImGui::SetNextWindowSize(WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags window_flags = 0
		| ImGuiWindowFlags_NoDocking 
		| ImGuiWindowFlags_NoTitleBar 
		| ImGuiWindowFlags_NoResize 
		| ImGuiWindowFlags_NoMove 
		| ImGuiWindowFlags_NoScrollbar 
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::Begin("TOOLBAR", NULL, window_flags);
	ImGui::PopStyleVar(2);
  
	// Put the buttons on there
	pToolbar->Render();

	ImGui::End();
}

void MainMenuBarUI()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			// Disabling fullscreen would allow the window to be moved to the front of other windows,
			// which we can't undo at the moment without finer window depth/z control.
			//ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen_persistant);

			if (ImGui::MenuItem("Open Image"))
			{
				// Open File
				NFD::UniquePathSet outPaths;

				nfdu8filteritem_t filterItem[1] = { {"Images", "png,tif,tga,gif,flc,fli,jpg,jpeg,jfif,lbm,bmp,webp,anm,pal,c1,c2,256,#C10000,#C20000,gsla"} };


				nfdresult_t result = NFD::OpenDialogMultiple(outPaths,
															 filterItem,  // filterList
															 1,           // filterCount
															 nullptr );   // defaultPath $$JGA FIXME

				if (result == NFD_OKAY)
				{
					// action
					nfdpathsetsize_t NumPaths;
					NFD::PathSet::Count(outPaths, NumPaths);
					for (unsigned int index = 0; index < NumPaths; ++index)
					{
						bool error = false;

						NFD::UniquePathSetPathU8 loadPath;
						NFD::PathSet::GetPath(outPaths, index, loadPath);

						std::string pathName = loadPath.get();
						size_t offset = pathName.find_last_of("\\/");
						std::string fileName = &pathName.c_str()[offset+1];

						SDL_Surface *image = nullptr;
						if (endsWith(pathName, ".anm"))
						{
							// Deluxe Paint Animation File
							// I'm only implementing the importing of these files, so that
							// I can view clay-fighter pencil sketches, without dosbox
							// Paintworks Animation
							std::vector<SDL_Surface*> frames = SDL_ANM_Load(pathName.c_str());

							LOG("ANM_Load %d Frames\n", frames.size());
							if (frames.size())
							{
								LOG("Loaded %s\n", pathName.c_str());
								imageDocuments.push_back(new ImageDocument(fileName, pathName, frames));
							}

						}
						else if (endsWith(pathName, ".c1") || endsWith(pathName, "#c10000"))
						{
							image = SDL_C1_Load(pathName.c_str());
						}
						else if (endsWith(pathName, ".c2") || endsWith(pathName, "#c20000"))
						{
							// Paintworks Animation
							std::vector<SDL_Surface*> frames = SDL_C2_Load(pathName.c_str());

							LOG("C2_Load %d Frames\n", frames.size());
							if (frames.size())
							{
								LOG("Loaded %s\n", pathName.c_str());
								imageDocuments.push_back(new ImageDocument(fileName, pathName, frames));
							}
						}
						else if (endsWith(pathName, ".gsla"))
						{
							//GS Lzb Animation File (DG Animation File)
							std::vector<SDL_Surface*> frames = SDL_GSLA_Load(pathName.c_str());

							LOG("GSLA_Load %d Frames\n", frames.size());
							if (frames.size())
							{
								LOG("Loaded %s\n", pathName.c_str());
								imageDocuments.push_back(new ImageDocument(fileName, pathName, frames));
							}
						}
						else if (endsWith(pathName, ".fan"))
						{
							// Foenix Animation
							std::vector<SDL_Surface*> frames = SDL_FAN_Load(pathName.c_str());

							LOG("FAN_Load %d Frames\n", frames.size());
							if (frames.size())
							{
								LOG("Loaded %s\n", pathName.c_str());
								imageDocuments.push_back(new ImageDocument(fileName, pathName, frames));
							}
						}
						else if (endsWith(pathName, ".256"))
						{
							// Foenix Bitmap Image
							std::vector<SDL_Surface*> frames = SDL_256_Load(pathName.c_str());
							LOG("256_Load %d Frames\n", frames.size());
							if (frames.size())
							{
								LOG("Loaded %s\n", pathName.c_str());
								imageDocuments.push_back(new ImageDocument(fileName, pathName, frames));
							}


						}
						else if (endsWith(pathName, ".gif"))
						{
							// Use GIF Library
							std::vector<SDL_Surface*> frames = SDL_GIF_Load(pathName.c_str());

							LOG("GIF_Load %d Frames\n", frames.size());
							if (frames.size())
							{
								LOG("Loaded %s\n", pathName.c_str());
								imageDocuments.push_back(new ImageDocument(fileName, pathName, frames));
							}
							else
							{
								image=IMG_Load(pathName.c_str());
							}
						}
						else if (endsWith(pathName, ".fli") || (endsWith(pathName, ".flc")))
						{
							// Use FLC/FLI Library
							std::vector<SDL_Surface*> frames = SDL_FLC_Load(pathName.c_str());

							LOG("FLC_Load %d Frames\n", frames.size());
							if (frames.size())
							{
								LOG("Loaded %s\n", pathName.c_str());
								imageDocuments.push_back(new ImageDocument(fileName, pathName, frames));
							}
						}
						else
						{
							image=IMG_Load(pathName.c_str());
						}

						if (image)
						{
							LOG("Loaded %s\n", pathName.c_str());

							imageDocuments.push_back(new ImageDocument(fileName, pathName, image));
						}

						if (true == error)
						{
							LOG("Failed %s\n", pathName.c_str());
						}
					}

					G_RebuildDock = true;
				}
			}

			if (ImGui::MenuItem("Open Palette"))
			{
				// Open File
				NFD::UniquePathSet outPaths;

				nfdu8filteritem_t filterItem[1] = { {"Palettes", "pal"} };


				nfdresult_t result = NFD::OpenDialogMultiple(outPaths,
															 filterItem,  // filterList
															 1,           // filterCount
															 nullptr );   // defaultPath $$JGA FIXME

				if (result == NFD_OKAY)
				{
					nfdpathsetsize_t NumPaths;
					NFD::PathSet::Count(outPaths, NumPaths);
					for (unsigned int index = 0; index < NumPaths; ++index)
					{
						NFD::UniquePathSetPathU8 loadPath;
						NFD::PathSet::GetPath(outPaths, index, loadPath);

						std::string pathName = loadPath.get();
						size_t offset = pathName.find_last_of("\\/");
						std::string filename = &pathName.c_str()[offset+1];

						LOG("Open PAL: %s, %s\n", filename.c_str(), pathName.c_str());

						// Eventually, support opening any type of image, and extracting
						// the palette, for now, lets just open the file, if it has a
						// .pal extension
						std::string& fullpath = pathName;

						if (endsWith(pathName, ".pal"))
						{
							PaletteDocument::GDocuments.push_back(new PaletteDocument(filename, fullpath));
						}
						else
						{
							LOG("FAILED %s\n", filename.c_str());
						}
					}
				}
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Import Vector Image"))
			{
				// Open File
				NFD::UniquePathSet outPaths;

				nfdu8filteritem_t filterItem[1] = { {"Wavefront OBJ", "obj,svg"} };


				nfdresult_t result = NFD::OpenDialogMultiple(outPaths,
															 filterItem,  // filterList
															 1,           // filterCount
															 nullptr );   // defaultPath $$JGA FIXME

				if (result == NFD_OKAY)
				{
					nfdpathsetsize_t NumPaths;
					NFD::PathSet::Count(outPaths, NumPaths);
					for (unsigned int index = 0; index < NumPaths; ++index)
					{
						NFD::UniquePathSetPathU8 loadPath;
						NFD::PathSet::GetPath(outPaths, index, loadPath);

						std::string pathName = loadPath.get();
						size_t offset = pathName.find_last_of("\\/");
						std::string filename = &pathName.c_str()[offset+1];

						LOG("Import vector OBJ: %s, %s\n", filename.c_str(), pathName.c_str());

						// Eventually, support opening any type of image, and extracting
						// the palette, for now, lets just open the file, if it has a
						// .pal extension
						std::string& fullpath = pathName;

						if (endsWith(pathName, ".obj") || endsWith(pathName, ".svg"))
						{
							COBJFile* pImportVector = new COBJFile(fullpath.c_str());
							CRawCanvas* pRawCanvas = new CRawCanvas(pImportVector);

							std::vector<SDL_Surface*> frames = pRawCanvas->RenderFrames();

							imageDocuments.push_back(new ImageDocument(filename, pathName, frames));

							delete pImportVector; // this also frees the pRawCanvas

						}
						else
						{
							LOG("FAILED %s\n", filename.c_str());
						}
					}

					G_RebuildDock = true;
				}

			}

			ImGui::Separator();
			#if 0 // Show them disabled, until I implement them
			if (ImGui::MenuItem("Save"))
			{
				// Save File
			}
			if (ImGui::MenuItem("Save as..."))
			{
				// Save As
			}
			#else
			ImGui::TextDisabled("Save");
			ImGui::TextDisabled("Save as...");
			#endif
			ImGui::Separator();
			ImGui::Separator();
			if (ImGui::MenuItem("Quit", "Alt+F4"))
			{
				// Quit the Application
				bAppDone = true;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Quantize"))
		{
			ImGui::TextDisabled("Algorithm");
			ImGui::Separator();

			if (ImGui::MenuItem("libimagequant", nullptr, g_eQuantAlgorithm == eQuantLibimagequant))
			{
				g_eQuantAlgorithm = eQuantLibimagequant;
			}
			if (ImGui::MenuItem("Wu", nullptr, g_eQuantAlgorithm == eQuantWu))
			{
				g_eQuantAlgorithm = eQuantWu;
			}
			if (ImGui::MenuItem("Oklab k-means", nullptr, g_eQuantAlgorithm == eQuantOklab))
			{
				g_eQuantAlgorithm = eQuantOklab;
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Windows"))
		{
			ImGui::TextDisabled("About");
			//if (ImGui::MenuItem("About"))
			//{
			//}

			ImGui::Separator();
			ImGui::Separator();

			if (ImGui::MenuItem("Palettes", nullptr, show_palette_window))
			{
				show_palette_window = !show_palette_window;
			}

			if (ImGui::MenuItem("Log", nullptr, show_log_window))
			{
				show_log_window = !show_log_window;
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}
