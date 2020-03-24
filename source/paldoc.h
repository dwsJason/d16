
// W3C FFFFFF+FFFF00+FF00FF+FF0000+C0C0C0+808080+808000+800080+800000+00FFFF+00FF00+008080+008000+0000FF+000080+000000

// PaletteDocument Class header
#ifndef _PALETTE_DOCUMENT_
#define _PALETTE_DOCUMENT_

#include <string>

class PaletteDocument
{
public:
	PaletteDocument(std::string filename);
	~PaletteDocument();

	void Render();

private:

};



#endif // _PALETTE_DOCUMENT_

