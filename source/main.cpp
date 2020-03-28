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

void SetWindowIcon( SDL_Window* pIcon );
void ShowLog();

//------------------------------------------------------------------------------
/* Quick utility function for texture creation */
static int
power_of_two(int input)
{
    int value = 1;

    while (value < input) {
        value <<= 1;
    }
    return value;
}

GLuint
SDL_GL_LoadTexture(SDL_Surface * surface, GLfloat * texcoord)
{
    GLuint texture;
    int w, h;
    SDL_Surface *image;
    SDL_Rect area;
    SDL_BlendMode saved_mode;

    /* Use the surface width and height expanded to powers of 2 */
    w = power_of_two(surface->w);
    h = power_of_two(surface->h);
    texcoord[0] = 0.0f;         /* Min X */
    texcoord[1] = 0.0f;         /* Min Y */
    texcoord[2] = (GLfloat) surface->w / w;     /* Max X */
    texcoord[3] = (GLfloat) surface->h / h;     /* Max Y */

    image = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN     /* OpenGL RGBA masks */
                                 0x000000FF,
                                 0x0000FF00, 0x00FF0000, 0xFF000000
#else
                                 0xFF000000,
                                 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
        );
    if (image == NULL) {
        return 0;
    }

    /* Save the alpha blending attributes */
    SDL_GetSurfaceBlendMode(surface, &saved_mode);
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);

    /* Copy the surface into the GL texture image */
    area.x = 0;
    area.y = 0;
    area.w = surface->w;
    area.h = surface->h;
    SDL_BlitSurface(surface, &area, image, &area);

    /* Restore the alpha blending attributes */
    SDL_SetSurfaceBlendMode(surface, saved_mode);

    /* Create an OpenGL texture for the image */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    SDL_FreeSurface(image);     /* No longer needed */

    return texture;
}
//------------------------------------------------------------------------------
static int alphaSort(const struct dirent **a, const struct dirent **b)
{
	return strcoll((*a)->d_name, (*b)->d_name);
}
//------------------------------------------------------------------------------
std::vector<ImageDocument*>   imageDocuments;
std::vector<PaletteDocument*> paletteDocuments;

//------------------------------------------------------------------------------

// Main code
int main(int, char**)
{
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

	// load support for the JPG and PNG image formats
	int flags=IMG_INIT_JPG|IMG_INIT_PNG|IMG_INIT_TIF|IMG_INIT_WEBP;
	int initted=IMG_Init(flags);
	if((initted&flags) != flags)
	{
		printf("IMG_Init: Failed to init required jpg and png support!\n");
		printf("IMG_Init: %s\n", IMG_GetError());
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
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
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

	bool show_log_window = true;
	bool show_palette_window = true;

    ImVec4 clear_color = ImVec4(0.18f, 0.208f, 0.38f, 1.00f);

	//--------------------------------------------------------------------------
	// load sample.png into image
	#if 0
	GLuint about_image=0;
	GLfloat about_image_uv[4];

	SDL_Surface *image;
	image=IMG_Load("..\\data\\about.256.png");
	if(!image) {
		printf("IMG_Load: %s\n", IMG_GetError());
		// handle error
	}
	else
	{
		about_image = SDL_GL_LoadTexture(image, about_image_uv);
	}

	GLuint chomp_image=0;
	GLfloat chomp_image_uv[4];
	image=IMG_Load("..\\data\\CHOMPFIS.LBM");
	if(!image) {
		printf("IMG_Load: %s\n", IMG_GetError());
		// handle error
	}
	else
	{
		chomp_image = SDL_GL_LoadTexture(image, chomp_image_uv);
	}
	#endif
	//--------------------------------------------------------------------------

	LOG("Dream16 Compiled %s %s\n", __DATE__, __TIME__);

	//SDL_Delay(1000);

	// Set File Type Filter Colors, because that won't look weird
	ImGuiFileDialog::Instance()->SetFilterColor(".png", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".tif", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".tga", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".gif", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".jpg", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".lbm", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".bmp", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".webp", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".pal", ImVec4(0,1,0,1));
	// keep the uppercase around, until we aren't using a stupid file dialog 
	ImGuiFileDialog::Instance()->SetFilterColor(".PNG", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".TIF", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".TGA", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".GIF", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".JPG", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".LBM", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".BMP", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".WEBP", ImVec4(0,1,0,1));
	ImGuiFileDialog::Instance()->SetFilterColor(".PAL", ImVec4(0,1,0,1));

	{
		// Scan preset palette directory
		std::string vPath = ".\\data\\palettes";
		struct dirent **files = nullptr;
		int fileCount = scandir(vPath.c_str(), &files, nullptr, alphaSort);

		for (int idx = 0; idx < fileCount; ++idx)
		{
			struct dirent *ent = files[idx];

			if (DT_REG == ent->d_type)
			{
				std::string filename = ent->d_name;

				int len = filename.size();

				if (len > 4)
				{
					int extensionOffset = len - 4;

					if ('.' == filename[extensionOffset++])
					{
						if ('p' == tolower(filename[extensionOffset++]))
						{
							if ('a' == tolower(filename[extensionOffset++]))
							{
								if ('l' == tolower(filename[extensionOffset++]))
								{
									LOG("%s\n", filename.c_str());

									paletteDocuments.push_back(new PaletteDocument(filename, vPath+"\\"+filename ));
								}
							}
						}
					}
				}
			}
		}
	}

    // Main loop
    bool done = false;
    while (!done)
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
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

		// Put everything in a DockSpace, because it's just cool
		{
			static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
			ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->GetWorkPos());
			ImGui::SetNextWindowSize(viewport->GetWorkSize());
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

			if (ImGui::BeginMenuBar())
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
					if (ImGui::MenuItem("Save"))
					{
						// Save File
					}
					if (ImGui::MenuItem("Save As"))
					{
						// Save As
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Quit", "Alt+F4"))
					{
						// Quit the Application
						done = true;
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Windows"))
				{
					if (ImGui::MenuItem("About"))
					{
					}

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

				ImGui::Button("Pan/Zoom");
				ImGui::Button("Eye Dropper");

				ImGui::EndMenuBar();
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

					  SDL_Surface *image;
					  image=IMG_Load(it->second.c_str());

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

						  paletteDocuments.push_back(new PaletteDocument(filename, fullpath));
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


			ImGui::End();
		}

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

			for (int idx = 0; idx < paletteDocuments.size(); ++idx)
				paletteDocuments[ idx ]->Render();

			ImGui::End();
		}

		// Render out the Resize Window

		{
			int iOriginalWidth  = 1024;
			int iOriginalHeight = 768;
			static int iNewWidth = 320;
			static int iNewHeight = 200;
			static bool bMaintainAspectRatio = false;
			static float fAspectRatio = 1.0f;

			static bool show_resize_image = true;
			ImGui::Begin("Resize Image", &show_resize_image,
						 ImGuiWindowFlags_AlwaysAutoResize
						 | ImGuiWindowFlags_NoCollapse
						 | ImGuiWindowFlags_NoDocking
						 );

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
				iNewHeight = (int)((((float)iNewWidth) / fAspectRatio) + 0.5f);
				if (iNewHeight < 1) iNewHeight = 1;
			}

			ImGui::Text("New Height:");  ImGui::SameLine(100); ImGui::SetNextItemWidth(128);
			if (ImGui::InputInt("Pixels##Height", &iNewHeight))
			{
				if (iNewHeight < 1) iNewHeight = 1;
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

			const char* items[] = { "Point Sample", "Bilinear Sample", "AVIR" };
            static int item_current = 2;
			ImGui::SetNextItemWidth(148);
            ImGui::Combo("##SampleCombo", &item_current, items, IM_ARRAYSIZE(items));

			static bool bDither = false;
			ImGui::NewLine();
			ImGui::SameLine(128);
			ImGui::Checkbox("Dither", &bDither);

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
			ImGui::Button("Ok", okSize);
			ImGui::SameLine();
			ImGui::Button("Cancel", okSize);

			ImGui::End();
		}


		//$$JGA Some picture windows, just for fun
#if 0
		{
			if (about_image)
			{
				ImTextureID tex_id = (ImTextureID)((size_t) about_image ); 
				ImVec2 uv0 = ImVec2(about_image_uv[0],about_image_uv[1]);
				ImVec2 uv1 = ImVec2(about_image_uv[2],about_image_uv[3]);
				ImGui::Begin("About DreamGrafix PNG",nullptr,ImGuiWindowFlags_NoResize);
				ImGui::Image(tex_id, ImVec2(320*2, 200*2), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
				ImGui::End();
			}
			if (chomp_image)
			{
				ImTextureID tex_id = (ImTextureID)((size_t) chomp_image );
				ImVec2 uv0 = ImVec2(chomp_image_uv[0],chomp_image_uv[1]);
				ImVec2 uv1 = ImVec2(chomp_image_uv[2],chomp_image_uv[3]);
				ImGui::Begin("Chomp LBM",nullptr,ImGuiWindowFlags_NoResize);
				ImGui::Image(tex_id, ImVec2(320*2, 200*2), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
				ImGui::End();
			}
		}
#endif

//		ShowShipEditor();

//		ShowVoronoiTest();

//		ShowClipper();

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


#if 0
//------------------------------------------------------------------------------
void ShowShipEditor()
{
	static C_ShipShape shipShape;
	static int hoverIndex = -1;	// For bolding a line that is being hovered on

	ImGui::Begin("Ship Editor", nullptr, ImGuiWindowFlags_NoResize |
				                         ImGuiWindowFlags_NoScrollbar |
										 ImGuiWindowFlags_AlwaysAutoResize );

	// Editor Window
	{
		const float pixels = 512.0f;
		const int grids = C_ShipShape::GRID_COUNT; // 15
		const float step = pixels / (grids+1.0f);

		ImGui::BeginChild("Ship", ImVec2(512, 512), true, ImGuiWindowFlags_NoMove);

		ImGuiIO& io = ImGui::GetIO();

		bool mouseUp = io.MouseReleased[0];  // So I know when to collect a new line

		if (io.MouseReleased[1] && (hoverIndex >= 0))
		{
			// Right Clicked, while hovering over a segment
			// Let's Delete it
			shipShape.RemoveLine( hoverIndex );
			hoverIndex = -1;
		}

		ImVec2 closestPoint(pixels*2, pixels*2);
		ImVec2 closestAnchorPoint = closestPoint;
		int anchorIndex = -1;

		float distSqr = pixels*pixels*pixels;
		float distAnchorSqr = distSqr;
		int mouseIndex = -1;

		int index = 0;
		ImVec2 points[grids*grids];

		for (int ypos = 0; ypos < grids; ++ypos)
		{
			for (int xpos = 0; xpos < grids; ++xpos)
			{
				ImVec2 winPos = ImGui::GetWindowPos();

				ImVec2 pos = ImVec2(winPos.x +((xpos+1)*step),
									winPos.y +((ypos+1)*step));

				// Initialize the Array of points, used in the editor
				// since the window can move, so can the points
				points[ index ] = pos; //Point2(pos.x, pos.y);

				// Current Mouse Position
				ImVec2 delta = ImVec2(pos.x - io.MousePos.x, pos.y - io.MousePos.y);

				float dist = (delta.x*delta.x) + (delta.y*delta.y);

				if (dist < distSqr)
				{
					distSqr = dist;
					closestPoint = pos;
					mouseIndex = index;
				}
				// Anchor Mouse Position
				delta = ImVec2(pos.x - io.MouseClickedPos[0].x, pos.y - io.MouseClickedPos[0].y);
				dist = (delta.x*delta.x) + (delta.y*delta.y);

				if (dist < distAnchorSqr)
				{
					distAnchorSqr = dist;
					closestAnchorPoint = pos;
					anchorIndex = index;
				}


				ImGui::GetWindowDrawList()->AddCircleFilled(
					    pos,
						3.0f,
						//ImGui::GetColorU32(ImGuiCol_Text)
						0x80808080
						);

				index++;
			}
		}

		{
			// Render the Ship
			for (int lineIndex = 0; lineIndex < shipShape.GetNumLines(); ++lineIndex)
			{
				C_ShipShape::s_Line& line = shipShape.GetLine( lineIndex );

				if (lineIndex == hoverIndex)
				{
					// Make it bright and bigger if it's selected, with a hover
					ImGui::GetWindowDrawList()->AddLine(
						points[ line.startIndex ],
						points[ line.endIndex ],
						0xF0F0F0F0,
						5.0f
						);
				}
				else
					ImGui::GetWindowDrawList()->AddLine(
						points[ line.startIndex ],
						points[ line.endIndex ],
						0xC0C0C0C0,
						4.0f
						);
				{
				}
			}

		}

		if (ImGui::IsWindowHovered(ImGuiFocusedFlags_ChildWindows))
		{
			ImGui::GetWindowDrawList()->AddCircle(
					closestPoint,
					4.0f,
					ImGui::GetColorU32(ImGuiCol_Text)
					);

			if (ImGui::IsMouseDragging(0))
			{
				ImGui::GetWindowDrawList()->AddLine(
					closestAnchorPoint,
					closestPoint,
					ImGui::GetColorU32(ImGuiCol_Text),
					4.0f
					);
			}

			if (mouseUp && (mouseIndex != anchorIndex)
				&& (mouseIndex >= 0)
				&& (anchorIndex >= 0))
			{
				//$$TODO Don't Add a Line if it's a duplicate of another Line
				C_ShipShape::s_Line newLine;
				newLine.startIndex = (unsigned char)anchorIndex;
				newLine.endIndex = (unsigned char)mouseIndex;

				// For now limit to 63 lines, keep size to 128 bytes
				if (shipShape.GetNumLines() < 63)
				{
					shipShape.AddLine(newLine);
				}

			}
		}

		ImGui::EndChild();

	}

	ImGui::SameLine();

	// Preview Window
	{
		ImGui::BeginChild("SideCar", ImVec2(128, 512), false,
						  ImGuiWindowFlags_NoResize |
						  ImGuiWindowFlags_NoScrollbar |
						  ImGuiWindowFlags_AlwaysAutoResize );

			    {
					ImGui::BeginChild("Preview", ImVec2(128, 128), true);
					float seconds_per_spin = 4.0f;
					float scale     = 16.0f;  // $$TODO Hook to a slider
					float spin_rate = (2.0f * (float)M_PI) / (60.0f * seconds_per_spin); // (about 2 seconds)
					static float angle = 0.0f;
					angle += spin_rate;

					Point2* pPoints = shipShape.GetVerts();

					Vector2 winpos= Vector2(ImGui::GetWindowPos());
					Vector2 center = winpos + Vector2(64,64);
					Transform3 rot = Transform3::rotationZ(angle);

					// Render the Ship
					for (int lineIndex = 0; lineIndex < shipShape.GetNumLines(); ++lineIndex)
					{
						C_ShipShape::s_Line& line = shipShape.GetLine( lineIndex );

						Vector2 start = Vector2(pPoints[ line.startIndex ]);
						Vector2 end   = Vector2(pPoints[ line.endIndex ]);

						// rotate start
						Vector3 temp = Vector3(start.getX(), start.getY(), 0.0f);
						temp = rot * temp;
						start = Vector2(temp.getX(), temp.getY());
						// rotate end
						temp = Vector3(end.getX(), end.getY(), 0.0f);
						temp = rot * temp;
						end = Vector2(temp.getX(), temp.getY());

						start *= scale;
						end   *= scale;
						start += center;
						end   += center;

						ImGui::GetWindowDrawList()->AddLine(
							ImVec2(start),
							ImVec2(end),
							0xF0F0F0F0,
							1.0f
							);
					}

					ImGui::EndChild();
		        }

				ImGui::BeginChild("Ship Info", ImVec2(128, 384), true);
				ImGui::Text("Segments: %d", shipShape.GetNumLines());

				std::vector<C_ShipShape::s_Line> &lines = shipShape.GetLines();

				hoverIndex = -1;

				for (int idx = 0; idx < (int)lines.size(); ++idx)
				{
					char buf[32];
					sprintf(buf,"%d: %d,%d", idx,
										lines[idx].startIndex,
										lines[idx].endIndex);
					ImGui::Selectable(buf, false);
					if (ImGui::IsItemHovered())
					{
						hoverIndex = idx;
					}

					//ImGui::Text("%d: %d,%d", idx,
					//			lines[idx].startIndex,
					//			lines[idx].endIndex);
				}

			ImGui::EndChild();

		ImGui::EndChild();
	}

	ImGui::End();
}
//------------------------------------------------------------------------------

// Distance from Point, to Line
float minimum_distance(Point2 v, Point2 w, Point2 p) {
  // Return minimum distance between line segment vw and point p
  const float l2 = distSqr(v,w);  // i.e. |w-v|^2 -  avoid a sqrt
  if (l2 == 0.0f) return dist(p, v);   // v == w case
  // Consider the line extending the segment, parameterized as v + t (w - v).
  // We find projection of point p onto the line. 
  // It falls where t = [(p-v) . (w-v)] / |w-v|^2
  // We clamp t from [0,1] to handle points outside the segment vw.
  const float t = max(0, min(1, dot(p - v, w - v) / l2));
  const Point2 projection = v + t * (w - v);  // Projection falls on the segment
  return dist(p, projection);
}

//------------------------------------------------------------------------------

// This is meant to be in it's own module, but for now I put it here, for testing
// and validation that I want to use this library

#define JC_VORONOI_IMPLEMENTATION
#include "jc_voronoi.h"

#define JC_VORONOI_CLIP_IMPLEMENTATION
#include "jc_voronoi_clip.h"

void ShowVoronoiTest()
{
static int rand_seed = 0;
	int num_sites = 101;  // number of flags for capture

	float map_width  = 640.0f; // map dimensions
	float map_height = 640.0f;

	float half_width  = (float)map_width / 2.0f;
	float half_height = (float)map_height / 2.0f;

	std::vector<jcv_point> points;
	jcv_rect rect;

	rect.min.x = -half_width;
	rect.min.y = -half_height;
	rect.max.x =  half_width;
	rect.max.y =  half_height;

	// Generate Those points
	srand(rand_seed);   	// Force random seed

	float scale = 0.95f; // Shrink it, to give it a border

	// Should generate the same sheet, as long as random seed is initialized
	for (int point_count = 0; point_count < num_sites; ++point_count)
	{
		float rand_x = -half_width  + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(map_width)));
		float rand_y = -half_height + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(map_height)));

		jcv_point point = { rand_x * scale, rand_y * scale};
		
		points.push_back(point); 
	}

	// Setup Crap for Clipper
	jcv_clipper* clipper = nullptr;   // do I really need this?
	#if 0
	jcv_clipping_polygon polygon;

	jcv_point clip_pts[4];
	clip_pts[0] = { rect.min.x, rect.min.y };
	clip_pts[1] = { rect.max.x, rect.min.y };
	clip_pts[2] = { rect.max.x, rect.max.y };
	clip_pts[3] = { rect.min.x, rect.max.y };
	polygon.num_points = 4;
	polygon.points = clip_pts;

	jcv_clipper polygonclipper;
	polygonclipper.test_fn = jcv_clip_polygon_test_point;
	polygonclipper.clip_fn = jcv_clip_polygon_clip_edge;
	polygonclipper.fill_fn = jcv_clip_polygon_fill_gaps;
	polygonclipper.ctx = &polygon;

	clipper = &polygonclipper;
	#endif
	//--------------------------------------------------------------------------


	jcv_diagram diagram;
	memset(&diagram, 0, sizeof(jcv_diagram));
	jcv_diagram_generate( num_sites,
						  (const jcv_point*) &points[0],
						  &rect,
						  clipper,
						  &diagram );
	{
		// Draw the things
		ImGui::Begin("Voronoi", nullptr, ImGuiWindowFlags_NoResize    |
										 ImGuiWindowFlags_NoScrollbar |
										 ImGuiWindowFlags_AlwaysAutoResize );

		ImGui::BeginChild("Map", ImVec2(map_width, map_height), true, ImGuiWindowFlags_NoMove);

		if (ImGui::IsWindowHovered(ImGuiFocusedFlags_ChildWindows))
		{
			ImGuiIO& io = ImGui::GetIO();

			if (io.MouseClicked[1])
			{
				rand_seed++;
			}
		}


		ImVec2 winPos = ImGui::GetWindowPos();

        const jcv_site* sites = jcv_diagram_get_sites( &diagram );
        for( int site_idx = 0; site_idx < diagram.numsites; ++site_idx )
		{
            const jcv_site* site = &sites[ site_idx ];

			ImVec2 pos = ImVec2(winPos.x + (site->p.x) + half_width,
								winPos.y + (site->p.y) + half_height );


			// Draw the Border!
            const jcv_graphedge* e = site->edges;
            while( e )
            {
                jcv_point p0 = e->pos[0];
                jcv_point p1 = e->pos[1];

				ImVec2 pos0 = ImVec2(winPos.x + p0.x + half_width,
									 winPos.y + p0.y + half_height );

				ImVec2 pos1 = ImVec2(winPos.x + p1.x + half_width,
									 winPos.y + p1.y + half_height );

				ImGui::GetWindowDrawList()->AddLine(
					pos0,
					pos1,
					0xF0F0F0F0,
					1.0f
					);

				{
					// Each Edge should have a neighbor
					const jcv_site* neighbor = e->neighbor;

					if (neighbor)
					{

						ImVec2 pos0 = ImVec2(winPos.x + site->p.x + half_width,
											 winPos.y + site->p.y + half_height );

						ImVec2 pos1 = ImVec2(winPos.x + neighbor->p.x + half_width,
											 winPos.y + neighbor->p.y + half_height );

						ImGui::GetWindowDrawList()->AddLine(
							pos0,
							pos1,
							0x800000A0,
							1.0f
							);

					}

				}

                e = e->next;
            }

			// Draw the Flag
			// $$TODO Average the Edge Points, to move the flag toward
			// $$TODO the center of the area
			ImGui::GetWindowDrawList()->AddCircleFilled(
					pos,
					3.0f,
					0xFF20C020
					);


		}

		ImGui::EndChild();
		ImGui::End();
	}

	jcv_diagram_free( &diagram );

}

//------------------------------------------------------------------------------

#include <clipper.hpp>
using namespace ClipperLib;

#include <earcut.hpp>

// Template crap to allow earcut to take clipper compatible inputs

namespace mapbox {
namespace util {

template <>
struct nth<0, IntPoint> {
    inline static auto get(const IntPoint &t) {
        return t.X;
    };
};
template <>
struct nth<1, IntPoint> {
    inline static auto get(const IntPoint &t) {
        return t.Y;
    };
};

} // namespace util
} // namespace mapbox

// The index type. Defaults to uint32_t, but you can also pass uint16_t if you know that your
// data won't have more than 65536 vertices.
using N = uint32_t;
//using Coord = double;

// Create array
//using Point = std::array<Coord, 2>;
using Point = IntPoint;

void Tesselate(const PolyNode& node, std::vector<N>& totalIndices, Path& verts)
{
	// Do Children Islands First
	for (int idx = 0; idx < node.ChildCount(); ++idx)
	{
		// Holes
		const PolyNode& child = *node.Childs[ idx ];

		for (int islandIndex = 0; islandIndex < child.ChildCount(); ++islandIndex)
		{
			Tesselate( *child.Childs[ islandIndex ], totalIndices, verts );
		}
	}


	std::vector<Path> terrainPolygon;
	int baseIndex = verts.size(); // For Remapping the result

	// Contour for the filled area
	terrainPolygon.push_back(node.Contour);

	// Collect Verts for rendering
	verts.insert(verts.end(),
				 node.Contour.begin(),
				 node.Contour.end()
							  );

	for (int idx = 0; idx < node.ChildCount(); ++idx)
	{
		// Holes
		const PolyNode& child = *node.Childs[ idx ];
		terrainPolygon.push_back(child.Contour);
		// collect more verts for rendering
		verts.insert(verts.end(),
					 child.Contour.begin(),
					 child.Contour.end()
								  );
	}

	std::vector<N> indices = mapbox::earcut<N>(terrainPolygon);

	// Result indices used for rendering
	// offset index, appending into a single list
	for (int idx = 0; idx < indices.size(); ++idx)
	{
		totalIndices.push_back( indices[idx] + baseIndex );
	}
}

//
// Demonstrate Dynamically Wrecking a polygon
// use the clipper to clip out a chunk
// then use the earcut, so generate a new polygon list
//
void ShowClipper()
{
	float map_width = 512.0f;
	float map_height = 512.0f;

static bool bInit = false;
static Path Cursor;
static Path Terrain;
static Paths Terrains;
static Path TerrainRenderVerts;
static std::vector<N> TerrainIndices;
static std::vector<N> CursorIndices;
static double CLIPPER_SCALE = 65536.0f; //65536.0*16384.0;

// Menu Options
static bool bResetCursor = true;
static bool bShowPolys = false;
static float cursor_angle = 0.0f;
static float cursor_scale = 8.0f;
static float cursor_edges = 7.0f;

	if (!bInit)
	{
		bResetCursor = true;

		Terrain.clear();
		Terrains.clear();
		TerrainRenderVerts.clear();
		TerrainIndices.clear();

		bInit = true;
		// Initialize a few things

		// Generate a terrain clipper path
		Terrain.push_back(
			IntPoint( (cInt)(0.0f*CLIPPER_SCALE), 
				      (cInt)(0.0f*CLIPPER_SCALE)));
		Terrain.push_back(
			IntPoint( (cInt)(map_width*CLIPPER_SCALE), 
					  (cInt)(0.0f*CLIPPER_SCALE)));
		Terrain.push_back(
			IntPoint( (cInt)(map_width*CLIPPER_SCALE), 
			  		  (cInt)(map_height*CLIPPER_SCALE)));
		Terrain.push_back(
			IntPoint( (cInt)(0.0f*CLIPPER_SCALE), 
					  (cInt)(map_height*CLIPPER_SCALE)));

		Terrains.push_back(Terrain);
		TerrainRenderVerts = Terrain;

		std::vector<Path> terrainPolygon;
		terrainPolygon.push_back(Terrain);
		TerrainIndices = mapbox::earcut<N>(terrainPolygon);
	}

	if (bResetCursor)
	{
		bResetCursor = false;
		Cursor.clear();
		CursorIndices.clear();

		// Generate a cursor, define a clipper path
		float steps = cursor_edges; //7.0f;
		float scale = cursor_scale; //25.0f;

		float ro = cursor_angle * M_PI / 180.0f;

		for (float r = 0.0f; r < (M_PI*2.0f); r+= (M_PI*2.0f/steps))
		{
			float x,y;
			IntPoint clipPoint;

			x = cos(r + ro) * scale;
			y = sin(r + ro) * scale;

			clipPoint.X = (cInt)(x*CLIPPER_SCALE);
			clipPoint.Y = (cInt)(y*CLIPPER_SCALE);

			Cursor.push_back(clipPoint);
		}

		// use the earcut, to convert the cursor path into triangles
		// that I can use to render the cursor
		// The number type to use for tessellation
		std::vector<Path> polygon;

		// Fill polygon structure with actual data. Any winding order works.
		// The first polyline defines the main polygon.
		polygon.push_back(Cursor);

		//polygon.push_back({{100, 0}, {100, 100}, {0, 100}, {0, 0}});
		// Following polylines define holes.
		//polygon.push_back({{75, 25}, {75, 75}, {25, 75}, {25, 25}});

		// Run tessellation
		// Returns array of indices that refer to the vertices of the input polygon.
		// e.g: the index 6 would refer to {25, 75} in this example.
		// Three subsequent indices form a triangle. Output triangles are clockwise.
		CursorIndices = mapbox::earcut<N>(polygon);
	}

	// Draw the things
	ImGui::Begin("Polygon Clipper", nullptr, ImGuiWindowFlags_NoResize    |
									 ImGuiWindowFlags_NoScrollbar |
									 ImGuiWindowFlags_AlwaysAutoResize );

	ImGui::BeginChild("Clipper", ImVec2(map_width, map_height), true, ImGuiWindowFlags_NoMove);

	ImVec2 winPos = ImGui::GetWindowPos();
	ImGuiIO& io = ImGui::GetIO();
	bool mouseDown = io.MouseDown[0];

	{
		{
			Path &verts = TerrainRenderVerts;
			// Draw the solid playfield
			for (int idx = 0; idx < TerrainIndices.size(); idx+=3)
			{
				ImVec2 p1 = ImVec2(verts[ TerrainIndices[idx+0] ].X, verts[ TerrainIndices[idx+0] ].Y);
				ImVec2 p2 = ImVec2(verts[ TerrainIndices[idx+1] ].X, verts[ TerrainIndices[idx+1] ].Y);
				ImVec2 p3 = ImVec2(verts[ TerrainIndices[idx+2] ].X, verts[ TerrainIndices[idx+2] ].Y);
				p1.x /= CLIPPER_SCALE;
				p1.y /= CLIPPER_SCALE;
				p2.x /= CLIPPER_SCALE;
				p2.y /= CLIPPER_SCALE;
				p3.x /= CLIPPER_SCALE;
				p3.y /= CLIPPER_SCALE;

				p1.x += winPos.x; p1.y += winPos.y;
				p2.x += winPos.x; p2.y += winPos.y;
				p3.x += winPos.x; p3.y += winPos.y;

				ImGui::GetWindowDrawList()->AddTriangleFilled(p3,
														p2,
														p1,
														0xFF202040);

				if (bShowPolys)
				{
					ImGui::GetWindowDrawList()->AddLine(p1, p2, 0xF08080F0, 0.5f);
					ImGui::GetWindowDrawList()->AddLine(p2, p3, 0xF08080F0, 0.5f);
					ImGui::GetWindowDrawList()->AddLine(p3, p1, 0xF08080F0, 0.5f);
				}
			}
		}

		if (ImGui::IsWindowHovered(ImGuiFocusedFlags_ChildWindows))
		{
			// Draw the Cut Shape, based on the Mouse Input

			for (int idx = 0; idx < CursorIndices.size(); idx+=3)
			{
				ImVec2 p1 = ImVec2(Cursor[ CursorIndices[idx+0] ].X, Cursor[ CursorIndices[idx+0] ].Y);
				ImVec2 p2 = ImVec2(Cursor[ CursorIndices[idx+1] ].X, Cursor[ CursorIndices[idx+1] ].Y);
				ImVec2 p3 = ImVec2(Cursor[ CursorIndices[idx+2] ].X, Cursor[ CursorIndices[idx+2] ].Y);
				p1.x /= CLIPPER_SCALE;
				p1.y /= CLIPPER_SCALE;
				p2.x /= CLIPPER_SCALE;
				p2.y /= CLIPPER_SCALE;
				p3.x /= CLIPPER_SCALE;
				p3.y /= CLIPPER_SCALE;

				p1.x += io.MousePos.x; p1.y += io.MousePos.y;
				p2.x += io.MousePos.x; p2.y += io.MousePos.y;
				p3.x += io.MousePos.x; p3.y += io.MousePos.y;

				ImGui::GetWindowDrawList()->AddTriangleFilled(p3,
															  p2,
															  p1,
															  0xFF808080);
			}

			if (mouseDown)
			{
				IntPoint xlate = IntPoint((cInt)((io.MousePos.x - winPos.x)*CLIPPER_SCALE),
										  (cInt)((io.MousePos.y - winPos.y)*CLIPPER_SCALE)); 

				Clipper clipper;

				SimplifyPolygons(Terrains);
				CleanPolygons(Terrains);

				// Put the terrain into the clipper
				clipper.AddPaths(Terrains, ptSubject, true);

				Paths clip;
				Path cursor;

				// Cursor needs translated, into Terrain Space
				for (int idx = 0; idx < Cursor.size(); ++idx)
				{
					IntPoint p = IntPoint( Cursor[idx].X + xlate.X,
										   Cursor[idx].Y + xlate.Y );
					cursor.push_back(p);
				}

				clip.push_back(cursor);

				clipper.AddPaths(clip, ptClip, true);

				PolyTree solution;
				clipper.Execute(ctDifference, solution, pftNonZero, pftNonZero);
				
				// Parse the Tree, Tesselating each "Outer" area
				// individually
				TerrainRenderVerts.clear();
				Terrains.clear();

				TerrainIndices.clear();

				for (int idx = 0; idx <solution.ChildCount(); ++idx)
				{
					PolyNode& child = *solution.Childs[ idx ];
					Tesselate(child, TerrainIndices, TerrainRenderVerts);
				}

				// Generate Terrains from Solution
				// using the Clipper Provided Hoodoo
				PolyTreeToPaths(solution, Terrains);

			}

			// Right Mouse Button Click
			if (io.MouseClicked[1])
			{
				bInit = false;
			}

		}
	}

	ImGui::EndChild();

	ImGui::SameLine();
	{
		ImGui::BeginChild("Controls", ImVec2(384, 512), false,
						  /*ImGuiWindowFlags_NoResize |*/
						  ImGuiWindowFlags_NoScrollbar |
						  ImGuiWindowFlags_AlwaysAutoResize );

        ImGui::Checkbox("Show Polys", &bShowPolys);

		ImGui::Separator();                                                    // separator, generally horizontal. inside a menu bar or in horizontal layout mode, this becomes a vertical separator.

		ImGui::Text("Cursor");

		ImGui::Separator();                                                    // separator, generally horizontal. inside a menu bar or in horizontal layout mode, this becomes a vertical separator.

		if (ImGui::DragFloat("angle", &cursor_angle, 1.0f))
				bResetCursor = true;

		if (cursor_angle < 0.0f) cursor_angle = 0.0f;
		if (cursor_angle > 360.0f) cursor_angle = 360.0f;

		//if (ImGui::InputFloat("", &cursor_angle, 0.0f, 360.0f, "%.0f"))
		//	bResetCursor = true;

		//if (ImGui::VSliderFloat("", ImVec2(100,128),&cursor_angle, 0.0f, 360.0f, "rotation %.0f"))
		//	bResetCursor = true;

		//ImGui::SameLine();
		ImGui::Separator();                                                    // separator, generally horizontal. inside a menu bar or in horizontal layout mode, this becomes a vertical separator.

		if (ImGui::DragFloat("scale", &cursor_scale, 1.0f))
				bResetCursor = true;

		if (cursor_scale < 8.0f) cursor_scale = 8.0f;
		if (cursor_scale > 50.0f) cursor_scale = 50.0f;

		//if (ImGui::VSliderFloat("", ImVec2(100,128),&cursor_scale, 8.0f, 50.0f, "scale %.0f"))
		//	bResetCursor = true;

		//ImGui::SameLine();
		ImGui::Separator(); 

		if (ImGui::DragFloat("edges", &cursor_edges, 1.0f))
				bResetCursor = true;

		if (cursor_edges < 3.0f) cursor_edges = 3.0f;
		if (cursor_edges > 25.0f) cursor_edges = 25.0f;

		//if (ImGui::VSliderFloat("", ImVec2(100,128),&cursor_edges, 3.0f, 25.0f, "edges %.0f"))
		//	bResetCursor = true;

		ImGui::Separator();                                                    // separator, generally horizontal. inside a menu bar or in horizontal layout mode, this becomes a vertical separator.

		ImGui::EndChild();
	}

	ImGui::End();
}
#endif
//------------------------------------------------------------------------------

