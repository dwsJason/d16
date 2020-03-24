// ImageDocument Class header
#ifndef _IMAGE_DOCUMENT_
#define _IMAGE_DOCUMENT_

#include <string>

#include "SDL_Surface.h"

#ifndef GLuint
typedef unsigned int	GLuint;		/* 4-byte unsigned */
typedef float		GLfloat;	/* single precision float */
#endif


class ImageDocument
{
public:
	ImageDocument(std::string filename, std::string pathname, SDL_Surface* pImage);
	~ImageDocument();

	bool IsClosed() { return !m_bOpen; }

	void Render();

private:

	int CountUniqueColors();
	void Quant();

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

	bool m_bOpen;

static int s_uniqueId;

};

#endif // _IMAGE_DOCUMENT_

