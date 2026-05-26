// ImageDocument Class header
#ifndef _IMAGE_DOCUMENT_
#define _IMAGE_DOCUMENT_

#include <string>
#include <vector>

#include "imgui.h"
#include "SDL_surface.h"

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

enum QuantAlgorithm
{
	eQuantLibimagequant,
	eQuantWu,
	eQuantOklab
};

extern QuantAlgorithm g_eQuantAlgorithm;

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
enum ScaleFilter
{
	ePointSample,
	eBilinearSample,
	eLanczos,
	eAVIR
};
//-------------------------------

class SpriteObjectDef
{
public:
	int m_size;
	int m_x;
	int m_y;
};

class SpriteChopSettings
{
public:
	bool m_bUse8x8;
	bool m_bUse16x16;
	bool m_bUse24x24;
	bool m_bUse32x32;
	bool m_bFavorMemory;  // if true optimize for memory, above all else
	                      // if false, favor OBJ count over all else
};


class SpriteObjectDocument
{
public:
	SpriteChopSettings m_settings;  // the settings used when we shopped this frame
	int m_minX;
	int m_maxX;
	int m_minY;
	int m_maxY;
	std::vector<SpriteObjectDef> m_objs;
};

//-------------------------------


class ImageDocument
{
public:
	ImageDocument(std::string filename, std::string pathname, SDL_Surface* pImage);
	ImageDocument(std::string filename, std::string pathname, const std::vector<SDL_Surface*> Images);
	~ImageDocument();

	bool IsClosed() { return !m_bOpen; }

	void Render();

	bool IsNew() { return m_bIsFirstRender; }
	const char* WindowName() { return m_windowName.c_str(); }

private:

	int CountUniqueColors();

	//$$JGA TODO - move this crap into a separate file/ class
	unsigned char* CreateC1Data(int frameNo);
	unsigned char* CreateC1DataInterlaced(int frameNo, int interlaceNo);
	std::vector<unsigned char> C2EncodeFrame(unsigned char* pPrev, unsigned char* pNext);

	void CropImage(int iNewWidth, int iNewHeight, int iJustify);
	void Quant16();
	void Quant256();
	void Quant135();

	void PointSampleResize(int iNewWidth, int iNewHeight);
	void LinearSampleResize(int iNewWidth, int iNewHeight);
	void LanczosResize(int iNewWidth, int iNewHeight);
	void AvirSampleResize(int iNewWidth, int iNewHeight, bool bDither);

	void RotateRight();
	void RotateLeft();
	void MirrorHorizontal();
	void MirrorVertical();
	void PlasmaFilter();
	void HalfToneGenerator();

	void RenderEyeDropper();
	void RenderPanAndZoom(int iButtonIndex=0);
	void RenderResizeDialog();
	void RenderSpriteChopDialog();
	void RenderTimeLine();

	void RenderSpriteDoc(const float ScrollX, const float ScrollY);
	void RenderOBJShapes(const float ScrollX, const float ScrollY);

	bool CheckSurface8x8(SDL_Surface* pSurface, Uint32 bg_pixel, int x, int y);

	bool CheckGrid(int gx, int gy, std::vector<int>& grid, int grid_w, int grid_h, int obj_size);
	void SetGrid(int gx, int gy, std::vector<int>& grid, int grid_w, int grid_h, int obj_size);

	void SaveC1(std::string filenamepath);
	void SaveC2(std::string filenamepath);
	void SavePNG(std::string filenamepath);
	void Save256(std::string filenamepath);
	void Save16(std::string filenamepath);
	void SaveFAN(std::string filenamepath, bool bTiled = false);
	void SaveGSLA(std::string filenamepath);

	void SetDocumentSurface(std::vector<SDL_Surface*> pSurfaces);
	void SetDocumentSurface(SDL_Surface* pSurface, int iFrameNo);

	SDL_Surface* SDL_SurfaceToRGBA(SDL_Surface* pSurface);
	SDL_Surface* SDL_SurfaceFromRawRGBA(Uint32* pPixels, int iWidth, int iHeight);
	Uint32* SDL_SurfaceToUint32Array(SDL_Surface* pSurface);
	Uint32 SDL_GetPixel(SDL_Surface* pSurface, int x, int y);

	std::string m_uniqId;
	std::string m_windowName;
	std::string m_filename;
	std::string m_pathname;

	bool m_bIsFirstRender;

	// Source Image Things
	GLfloat m_image_uv[4];    // uv coordinates

	// Turns out we now support Animation, weird
	std::vector<GLuint> m_images; // GL Images
	std::vector<SDL_Surface*> m_pSurfaces;
	std::vector<SpriteObjectDocument*> m_spriteDocuments;

	//
	// For editing, and playing having a present time, that is a real time
	// makes more sense than a list of delay times, this way each frame
	// gets to live in a "global" spot on the timeline
	//
	std::vector<int> m_iDelayTimes;

	bool m_bPlaying; 	// Is Animation Playing
	float m_fDelayTime;	// Delay Time
	int  m_iFrameNo;	// Currently Displayed Frame Number

	int m_numSourceColors;
	std::vector<int> m_numUniqueColors;

	int m_width;
	int m_height;

	// if we keep previousZoom, we can give IMGUI
	// some hints to keep the window from scrolling
	// all weird during the zoom
	int m_previousZoom;
	int m_zoom;

	// Destination Image Things
	std::vector<GLuint> m_targetImages; // GL Images
	std::vector<SDL_Surface*> m_pTargetSurfaces;

	int m_numTargetColors;

	int m_iDither;
	int m_iPosterize;
	int m_iTargetColorCount;

	std::vector<int>   m_bLocks;
	std::vector<ImVec4> m_targetColors;

//-- UI State
	bool m_bOpen;      				// document open
	bool m_bPanActive;  			// panning
	bool m_bShowResizeUI;   		// resize image modal
	bool m_bShowSpriteChopUI;   	// sprite chop modal
	bool m_bEyeDropDrag;

static int s_uniqueId;

};

enum 
{
	TILE_EMPTY,
	TILE_USED,

	TILE_8x8,
	TILE_16x16,
	TILE_24x24,
	TILE_32x32,

	TILE_SIZES_COUNT
};


#endif // _IMAGE_DOCUMENT_

