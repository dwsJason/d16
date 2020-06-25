//
//  Toolbar - My Dear ImGUI toolbar window
//  Right now, my thinking is you dock this, maybe I can force it to be docked
//
#ifndef TOOLBAR_H_
#define TOOLBAR_H_

#include "imgui.h"
#include <string>

enum ToolBarMode
{
	ePanZoom,
	eEyeDropper,
	eResizeImage,

	eRotate90Right,
	eRotate90Left,
	eMirrorHorizontal,
	eMirrorVertical,

	eToolBarMode_COUNT
};


class Toolbar
{
public:
	Toolbar();
	~Toolbar();

	void Render();

	void SetCurrentMode(int toolBarMode);
	int  GetCurrentMode()  { return m_currentMode; }
	int  GetPreviousMode() { return m_previousMode; }
	void SetPreviousMode() { SetCurrentMode(m_previousMode); }

	void SetFocusWindow(const char* pFocusID);

	// Provide access to our bitmap buttons, so anyone who wants them
	bool ImageButton(int col, int row, bool pressed = false);

	static Toolbar* GToolbar;

private:

	std::string m_focusWindow;

	void SetButtonImage(int x, int y);

	unsigned int m_GLImage;
	float m_UV[4];

	int m_currentMode;
	int m_previousMode;

	// Some button stuff
	ImVec2 m_uv0;
	ImVec2 m_uv1;

};

#endif

