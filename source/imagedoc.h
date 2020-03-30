// ImageDocument Class header
#ifndef _IMAGE_DOCUMENT_
#define _IMAGE_DOCUMENT_

#include <string>
#include <vector>

#include "imgui.h"
#include "SDL_Surface.h"

#ifndef GLuint
typedef unsigned int	GLuint;		/* 4-byte unsigned */
typedef float		GLfloat;	/* single precision float */
#endif

enum PosterizeTargets
{
	ePosterize444,
	ePosterize555,
	ePosterize888
};

//-------------------------------
//  0 1 2
//  3 4 5
//  6 7 8
//-------------------------------
enum Justify
{
	eUpperLeft,
	eUpperCenter,
	eUpperRight,

	eCenterLeft,
	eCenterCenter,
	eCenterRight,

	eLowerLeft,
	eLowerCenter,
	eLowerRight
};
//-------------------------------

class ImageDocument
{
public:
	ImageDocument(std::string filename, std::string pathname, SDL_Surface* pImage);
	~ImageDocument();

	bool IsClosed() { return !m_bOpen; }

	void Render();

private:

	int CountUniqueColors();
	void CropImage(int iNewWidth, int iNewHeight, int iJustify);
	void Quant();

	void RenderPanAndZoom();
	void RenderResizeDialog();

	std::string m_windowName;
	std::string m_filename;
	std::string m_pathname;

	// Source Image Things
	GLuint m_image;           // GL Image Number
	GLfloat m_image_uv[4];    // uv coordinates
	SDL_Surface* m_pSurface;

	int m_numSourceColors;

	int m_width;
	int m_height;
	int m_zoom;

	// Destination Image Things
	GLuint m_targetImage; // GL Image Number
	SDL_Surface* m_pTargetSurface;
	int m_numTargetColors;

	int m_iDither;
	int m_iPosterize;

	std::vector<int>   m_bLocks;
	std::vector<ImVec4> m_targetColors;

//-- UI State
	bool m_bOpen;
	bool m_bPanActive;
	bool m_bShowResizeUI;

static int s_uniqueId;

};

#endif // _IMAGE_DOCUMENT_

