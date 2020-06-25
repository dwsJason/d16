// dear imgui: standalone example application for SDL2 + OpenGL
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)
// (GL3W is a helper library to access OpenGL functions since there is no standard header to access modern OpenGL functions easily. Alternatives are GLEW, Glad, etc.)

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include "log.h"
#include "ImGuiFileDialog.h"
#include "imagedoc.h"
#include "paldoc.h"
#include "dirent.h"
#include "toolbar.h"

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

    // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#else
    bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
    if (err)
    {
        SYSERROR("Failed to initialize OpenGL loader!\nOpenGL 3 or above required");
        return 1;
    }

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
	io.Fonts->AddFontFromFileTTF("./data/ShastonHi640.ttf", 16.0f);

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

	//SDL_Delay(1000);

	// Set File Type Filter Colors, because that won't look weird
	ImGuiFileDialog::Instance()->SetFilterColor(".png", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".tif", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".tga", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".gif", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".fan", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".jpg", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".jfif", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".lbm", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".bmp", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".webp", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".pal", ImVec4(0,1,0,1));
	// keep the uppercase around, until we aren't using a stupid file dialog 
	ImGuiFileDialog::Instance()->SetFilterColor(".PNG", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".TIF", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".TGA", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".GIF", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".FAN", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".JPG", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".JFIF", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".LBM", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".BMP", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".WEBP", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".PAL", ImVec4(0,1,0,1));

	{
		// Scan preset palette directory
		LOG("Load Preset Palettes\n");

		std::string vPath = ".\\data\\palettes";
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
					PaletteDocument::GDocuments.push_back(new PaletteDocument(filename, vPath+"\\"+filename ));
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
        ImGui_ImplSDL2_NewFrame(window);
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

void DockSpaceUI()
{
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
	ImGuiWindowFlags window_flags = /*ImGuiWindowFlags_MenuBar |*/ ImGuiWindowFlags_NoDocking;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImVec2 WorkPos = viewport->GetWorkPos();
	WorkPos.y += toolbarSize;
	ImVec2 WorkSize = viewport->GetWorkSize();
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
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

//---
	ImGui::End();

}

void ToolBarUI()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetWorkPos());
	ImVec2 WorkSize = viewport->GetWorkSize();
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
				ImGuiFileDialog::Instance()->OpenDialog("OpenImageDlgKey", "Open Image", "\0", ".", "", 0);
			}

			if (ImGui::MenuItem("Open Palette"))
			{
				ImGuiFileDialog::Instance()->OpenDialog("OpenPaletteDlgKey", "Open Palette", "\0", ".", "", 0);
			}

			ImGui::Separator();
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

	// display open file dialog
	if (ImGuiFileDialog::Instance()->FileDialog("OpenImageDlgKey")) 
	{
	  // action if OK
	  if (ImGuiFileDialog::Instance()->IsOk == true)
	  {
		  //std::string filePathName = ImGuiFileDialog::Instance()->GetFilepathName();
		  //std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
		  //std::string filter = ImGuiFileDialog::Instance()->GetCurrentFilter();
		  // here convert from string because a string was passed as a userDatas, but it can be what you want
		  //auto userDatas = std::string((const char*)ImGuiFileDialog::Instance()->GetUserDatas()); 
		  //auto selection = ImGuiFileDialog::Instance()->GetSelection(); // multiselection

		  std::map<std::string, std::string> selection = ImGuiFileDialog::Instance()->GetSelection(); // multiselection

		  // action
		  for (std::map<std::string, std::string>::iterator it = selection.begin(); it != selection.end(); it++)
		  {
			  //LOG("%s - %s\n", it->first.c_str(), it->second.c_str());
			  std::string pathName = it->second;

			  SDL_Surface *image = nullptr;

			  if (endsWith(pathName, ".fan"))
			  {
				  // Foenix Animation
				  std::vector<SDL_Surface*> frames = SDL_FAN_Load(pathName.c_str());

				  LOG("FAN_Load %d Frames\n", frames.size());
				  if (frames.size())
				  {
					  LOG("Loaded %s\n", it->second.c_str());
					  imageDocuments.push_back(new ImageDocument(it->first, it->second, frames));
				  }


			  }
			  else if (endsWith(pathName, ".gif"))
			  {
				  // Use GIF Library
				  std::vector<SDL_Surface*> frames = SDL_GIF_Load(pathName.c_str());

				  LOG("GIF_Load %d Frames\n", frames.size());
				  if (frames.size())
				  {
					  LOG("Loaded %s\n", it->second.c_str());
					  imageDocuments.push_back(new ImageDocument(it->first, it->second, frames));
				  }
				  else
					  image=IMG_Load(pathName.c_str());
			  }
			  else
			  {
				  image=IMG_Load(pathName.c_str());
			  }

			  if (image)
			  {
				  LOG("Loaded %s\n", it->second.c_str());

				  imageDocuments.push_back(new ImageDocument(it->first, it->second, image));
			  }
			  else
			  {
				  LOG("Failed %s\n", it->second.c_str());
			  }
		  }
	  }
	  // close
	  ImGuiFileDialog::Instance()->CloseDialog("OpenImageDlgKey");
	}


	// display open file dialog
	if (ImGuiFileDialog::Instance()->FileDialog("OpenPaletteDlgKey")) 
	{
	  // action if OK
	  if (ImGuiFileDialog::Instance()->IsOk == true)
	  {
		  //std::string filePathName = ImGuiFileDialog::Instance()->GetFilepathName();
		  //std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
		  //std::string filter = ImGuiFileDialog::Instance()->GetCurrentFilter();
		  // here convert from string because a string was passed as a userDatas, but it can be what you want
		  //auto userDatas = std::string((const char*)ImGuiFileDialog::Instance()->GetUserDatas()); 
		  std::map<std::string, std::string> selection = ImGuiFileDialog::Instance()->GetSelection(); // multiselection

		  // action
		  for (std::map<std::string, std::string>::iterator it = selection.begin(); it != selection.end(); it++)
		  {
			  LOG("Open PAL: %s, %s\n", it->first.c_str(), it->second.c_str());

			  // Eventually, support opening any type of image, and extracting
			  // the palette, for now, lets just open the file, if it has a
			  // .pal extension
			  std::string filename = it->first;
			  std::string& fullpath = it->second;

			  std::string extension = ".pal";

			  if (fullpath.length() > extension.length())
			  {
				  size_t fullpath_offset = fullpath.length() - extension.length();

				  for (int idx = 0; idx < extension.length(); ++idx)
				  {
					  if (tolower(fullpath[ fullpath_offset + idx ]) != extension[ idx])
					  {
						  LOG("FAILED %s\n", filename.c_str());
					  }
				  }

				  PaletteDocument::GDocuments.push_back(new PaletteDocument(filename, fullpath));
			  }
			  else
			  {
				  LOG("FAILED %s\n", filename.c_str());
			  }

		  }
	  }
	  // close
	  ImGuiFileDialog::Instance()->CloseDialog("OpenPaletteDlgKey");
	}
}
