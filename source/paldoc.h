//
// PaletteDocument Class header
//
#ifndef _PALETTE_DOCUMENT_
#define _PALETTE_DOCUMENT_

#include "imgui.h"
#include <string>
#include <vector>

class PaletteDocument
{
public:
	PaletteDocument(std::string filename, std::string pathname);
	~PaletteDocument();

static void GRender();  // Render the Palette Documents windows

static std::vector<PaletteDocument*> GDocuments;

private:

	void Render();

	std::string m_filename;
	std::string m_pathname;

	std::vector<unsigned int>  m_colors;
	std::vector<ImVec4> m_floatColors;

//------ Stuff for doc manipulation
static int IndexOf(PaletteDocument* pDoc);

enum
{
	CMD_DUPLICATE,
	CMD_MOVE_UP,
	CMD_MOVE_DOWN,
	CMD_MOVE_TOP,
	CMD_MOVE_BOTTOM,
	CMD_RENAME,
	CMD_SAVE_AS,
	CMD_CLOSE
};
static PaletteDocument* g_pDoc;  // Document to do a command upon
static int g_iCommand;		     // enum, which command

};



#endif // _PALETTE_DOCUMENT_

