

#include "log.h"

#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <stdio.h>
#include <stdarg.h>

//------------------------------------------------------------------------------
Log Log::GLog;
//------------------------------------------------------------------------------

#if WIN32
#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include "winuser.h"
#endif

void Log::NativeError(const char* format, ... )
{
#if WIN32
	char messagebuffer[1024] = { "Default Message" };

	va_list argptr;
	va_start(argptr, format);
	sprintf(messagebuffer, format, argptr);
	va_end(argptr);

    int msgboxID = MessageBox(
        NULL,
        messagebuffer,
        "FATAL ERROR",
        MB_ICONERROR | MB_OK
    );

    if (msgboxID == IDYES)
    {
        // TODO: add code
    }
#else
	va_list argptr;
	va_start(argptr, format);
	vfprintf(stderr, format, argptr);
	va_end(argptr);
#endif
}

