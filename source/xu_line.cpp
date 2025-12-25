
#include "xu_line.h"

#include <math.h>

CRawCanvas::CRawCanvas(COBJFile* pOBJFile)
{
	int width,height;

	pOBJFile->GetWidthHeight(&width, &height);

	m_pOBJFile = pOBJFile;

	m_width  = width;
	m_height = height;
	m_color = 0xFFFFFFFF;

	m_pRawPixels = new u32[ width * height ];
	memset(m_pRawPixels, 0, width * height * sizeof(u32));

}

std::vector<SDL_Surface*> CRawCanvas::RenderFrames()
{
	// result
	std::vector<SDL_Surface*> frames;

	// Actually Render some stuff onto our m_pSurface

	const std::vector<OBJFILE::vec2>& points = m_pOBJFile->GetPoints();
	const std::vector<OBJFILE::int2>& lines  = m_pOBJFile->GetLines();
	const OBJFILE::vec2& center = m_pOBJFile->GetCenter();

	const OBJFILE::vec2& scale = m_pOBJFile->GetScale();

	int width = 640;
	int height = 400;

	m_pOBJFile->GetWidthHeight(&width, &height);

	float tx = (width / 2) - 1.0f;
	float ty = (height / 2) - 1.0f;

	const float PI_2 = (float) (M_PI * 2.0f);

	for (float angle = 0.0f; angle < 256.0f; angle+=1.0f)
	{
		float theta = angle * PI_2 / 256.0f;
		memset(m_pRawPixels, 0, sizeof(u32) * m_width * m_height);

		for (int line_idx = 0; line_idx < lines.size(); ++line_idx)
		{
			float x0,y0,x1,y1;

			x0 = points[ lines[ line_idx ].x - 1 ].x - center.x;
			y0 = points[ lines[ line_idx ].x - 1 ].y - center.y;

			x1 = points[ lines[ line_idx ].y - 1 ].x - center.x;
			y1 = points[ lines[ line_idx ].y - 1 ].y - center.y;
			// scale
			x0*=scale.x;
			x1*=scale.x;
			y0*=scale.y;
			y1*=scale.y;

			// rotate
			float rx = float(x0 * cos(theta) - y0 * sin(theta));
			float ry = float(x0 * sin(theta) + y0 * cos(theta));

			x0 = rx;
			y0 = ry;

			rx = float(x1 * cos(theta) - y1 * sin(theta));
			ry = float(x1 * sin(theta) + y1 * cos(theta));

			x1 = rx;
			y1 = ry;

			// translate

			x0+=tx;x1+=tx;
			y0+=ty;y1+=ty;

			//WULine(x0,y0,x1,y1);
			BLine((int)x0,(int)y0,(int)x1,(int)y1);
		}

		SDL_Surface* pSurface = SDL_SurfaceFromRawRGBA((Uint32*)m_pRawPixels, m_width, m_height);
		pSurface->userdata = (void*)1; // hacked animation time into here

		frames.push_back(pSurface);
	}

	return frames;
}



// Paint pixel white (floating point version, for reference only)
void inline CRawCanvas::plotf(i16 x, i16 y, float alpha)
{
	//m_pSurface[y*m_width + x] = 255 - ((255 - m_pSurface[y*m_width + x]) * (1.0f - alpha));

	int surfaceIndex = (y * m_width) + x;

	u32 surfaceColor = m_pRawPixels[surfaceIndex];

	// surface rgb
	u8 sr, sg, sb, sa;
	sr = (surfaceColor >> 0)  & 0xFF;
	sg = (surfaceColor >> 8)  & 0xFF;
	sb = (surfaceColor >> 16) & 0xFF;
	sa = (surfaceColor >> 24) & 0xFF;

	// paint rgb
	u8 pr, pg, pb, pa;
	pr = (m_color >> 0)  & 0xFF;
	pg = (m_color >> 8)  & 0xFF;
	pb = (m_color >> 16) & 0xFF;
	pa = (m_color >> 24) & 0xFF;

	// result pixel
	u8 r,g,b,a;
	r = static_cast<u8>(255 - (255-sr) * (1.0f - alpha));
	g = static_cast<u8>(255 - (255-sg) * (1.0f - alpha));
	b = static_cast<u8>(255 - (255-sb) * (1.0f - alpha));
	a = static_cast<u8>(255 - (255-sa) * (1.0f - alpha));

	u32 pixel = a; pixel<<=8;
	    pixel|= b; pixel<<=8;
	    pixel|= g; pixel<<=8;
		pixel|= r;

	m_pRawPixels[ surfaceIndex ] = pixel;



}

void CRawCanvas::WULine(float x0, float y0, float x1, float y1)
{
	bool steep = fabs(y1 - y0) > fabs(x1 - x0);

	if(steep)   { float z = x0; x0 = y0; y0 = z; z = x1; x1 = y1; y1 = z; }
	if(x0 > x1) { float z = x0; x0 = x1; x1 = z; z = y0; y0 = y1; y1 = z; }

	float dx = x1 - x0, dy = y1 - y0;
	float gradient = (dx == 0.0f) ? 1.0f : dy / dx;

	// handle first endpoint
	uint16_t xend = (uint16_t)round(x0);
	float yend = y0 + gradient * ((float)xend - x0);
	float xgap = (float)(1.0f - (x0 + 0.5f - floor(x0 + 0.5f)));
	int16_t xpxl1 = xend; // this will be used in the main loop
	int16_t ypxl1 = (int16_t)floor(yend);
	if(steep) {
		plotf(ypxl1,    xpxl1, (1.0f - (yend - (float)floor(yend))) * xgap);
		plotf(ypxl1 + 1, xpxl1,        (yend - (float)floor(yend))  * xgap);
	} else {
		plotf(xpxl1, ypxl1,    (1.0f - (yend - (float)floor(yend))) * xgap);
		plotf(xpxl1, ypxl1 + 1,        (yend - (float)floor(yend))  * xgap);
	}
	float intery = yend + gradient; // first y-intersection for the main loop

	// handle second endpoint
	xend = (uint16_t)round(x1);
	yend = y1 + gradient * ((float)xend - x1);
	xgap = x1 + 0.5f - (float)floor(x1 + 0.5f);
	int16_t xpxl2 = xend; //this will be used in the main loop
	int16_t ypxl2 = (int16_t)floor(yend);
	if(steep) {
		plotf(ypxl2,     xpxl2, (1.0f - (yend - (float)floor(yend))) * xgap);
		plotf(ypxl2 + 1, xpxl2,         (yend - (float)floor(yend))  * xgap);
	} else {
		plotf(xpxl2, ypxl2,    (1.0f - (yend - (float)floor(yend))) * xgap);
		plotf(xpxl2, ypxl2 + 1,        (yend - (float)floor(yend))  * xgap);
	}

	// main loop
	if(steep) {
		for(uint16_t x = xpxl1 + 1; x < xpxl2; x++) {
			plotf((i16)floor(intery),     x, (1.0f - (intery - (float)floor(intery))));
			plotf((i16)floor(intery) + 1, x,         (intery - (float)floor(intery) ));
			intery += gradient;
		}
	} else {
		for(uint16_t x = xpxl1 + 1; x < xpxl2; x++) {
			plotf(x, (i16)floor(intery),     (1.0f - (intery - (float)floor(intery))));
			plotf(x, (i16)floor(intery) + 1,         (intery - (float)floor(intery) ));
			intery += gradient;
		}
	}

}

void CRawCanvas::setPixel(i16 x, i16 y)
{
	int surfaceIndex = (y*m_width) + x;
	m_pRawPixels[surfaceIndex] = m_color;
}

void CRawCanvas::BLine(int x0, int y0, int x1, int y1)
{

  int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
  int dy = abs(y1-y0), sy = y0<y1 ? 1 : -1; 
  int err = (dx>dy ? dx : -dy)/2, e2;

  for(;;){
    setPixel((i16)x0,(i16)y0);
    if (x0==x1 && y0==y1) break;
    e2 = err;
    if (e2 >-dx) { err -= dy; x0 += sx; }
    if (e2 < dy) { err += dx; y0 += sy; }
  }
}


#if 0
// fast fixed point
// Something to draw on
static uint8_t canvas[240][240];

// Paint pixel white
static void inline plot(int16_t x, int16_t y, uint16_t alpha) {
	canvas[y][x] = 255 - (((255 - canvas[y][x]) * (alpha & 0x1FF)) >> 8);
}

// Xiaolin Wu's line algorithm
// Coordinates are Q16 fixed point, ie 0x10000 == 1
void wuline(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
	bool steep = ((y1 > y0) ? (y1 - y0) : (y0 - y1)) > ((x1 > x0) ? (x1 - x0) : (x0 - x1));

	if(steep)   { int32_t z = x0; x0 = y0; y0 = z; z = x1; x1 = y1; y1 = z; }
	if(x0 > x1) { int32_t z = x0; x0 = x1; x1 = z; z = y0; y0 = y1; y1 = z; }

	int32_t dx = x1 - x0, dy = y1 - y0;
	int32_t gradient = ((dx >> 8) == 0) ? 0x10000 : (dy << 8) / (dx >> 8);

	// handle first endpoint
	int32_t xend = (x0 + 0x8000) & 0xFFFF0000;
	int32_t yend = y0 + ((gradient * (xend - x0)) >> 16);
	int32_t xgap = 0x10000 - ((x0 + 0x8000) & 0xFFFF);
	int16_t xpxl1 = xend >> 16; // this will be used in the main loop
	int16_t ypxl1 = yend >> 16;
	if(steep) {
		plot(ypxl1,     xpxl1,     0x100 - (((0x100 - ((yend >> 8) & 0xFF)) * xgap) >> 16));
		plot(ypxl1 + 1, xpxl1,     0x100 - ((         ((yend >> 8) & 0xFF)  * xgap) >> 16));
	} else {
		plot(xpxl1,     ypxl1,     0x100 - (((0x100 - ((yend >> 8) & 0xFF)) * xgap) >> 16));
		plot(xpxl1,     ypxl1 + 1, 0x100 - ((         ((yend >> 8) & 0xFF)  * xgap) >> 16));
	}
	int32_t intery = yend + gradient; // first y-intersection for the main loop

	// handle second endpoint
	xend = (x1 + 0x8000) & 0xFFFF0000;
	yend = y1 + ((gradient * (xend - x1)) >> 16);
	xgap = (x1 + 0x8000) & 0xFFFF;
	int16_t xpxl2 = xend >> 16; //this will be used in the main loop
	int16_t ypxl2 = yend >> 16;
	if(steep) {
		plot(ypxl2,     xpxl2,     0x100 - (((0x100 - ((yend >> 8) & 0xFF)) * xgap) >> 16));
		plot(ypxl2 + 1, xpxl2,     0x100 - ((         ((yend >> 8) & 0xFF)  * xgap) >> 16));
	} else {                       
		plot(xpxl2,     ypxl2,     0x100 - (((0x100 - ((yend >> 8) & 0xFF)) * xgap) >> 16));
		plot(xpxl2,     ypxl2 + 1, 0x100 - ((         ((yend >> 8) & 0xFF)  * xgap) >> 16));
	}

	// main loop
	if(steep) {
		for(int32_t x = xpxl1 + 1; x < xpxl2; x++) {
			plot((intery >> 16)    , x,          (intery >> 8) & 0xFF );
			plot((intery >> 16) + 1, x, 0x100 - ((intery >> 8) & 0xFF));
			intery += gradient;
		}
	} else {
		for(int32_t x = xpxl1 + 1; x < xpxl2; x++) {
			plot(x, (intery >> 16),              (intery >> 8) & 0xFF );
			plot(x, (intery >> 16) + 1, 0x100 - ((intery >> 8) & 0xFF));
			intery += gradient;
		}
	}
}
#endif

#if 0
// Paint pixel white (floating point version, for reference only)
static void inline plotf(int16_t x, int16_t y, float alpha) {
	canvas[y][x] = 255 - ((255 - canvas[y][x]) * (1.0 - alpha));
}

// Xiaolin Wu's line algorithm (floating point version, for reference only)
void wulinef(float x0, float y0, float x1, float y1) {
	bool steep = fabs(y1 - y0) > fabs(x1 - x0);

	if(steep)   { float z = x0; x0 = y0; y0 = z; z = x1; x1 = y1; y1 = z; }
	if(x0 > x1) { float z = x0; x0 = x1; x1 = z; z = y0; y0 = y1; y1 = z; }

	float dx = x1 - x0, dy = y1 - y0;
	float gradient = (dx == 0.0) ? 1.0 : dy / dx;

	// handle first endpoint
	uint16_t xend = round(x0);
	float yend = y0 + gradient * ((float)xend - x0);
	float xgap = 1.0 - (x0 + 0.5 - floor(x0 + 0.5));
	int16_t xpxl1 = xend; // this will be used in the main loop
	int16_t ypxl1 = floor(yend);
	if(steep) {
		plotf(ypxl1,       xpxl1, (1.0 - (yend - floor(yend))) * xgap);
		plotf(ypxl1 + 1.0, xpxl1,        (yend - floor(yend))  * xgap);
	} else {
		plotf(xpxl1, ypxl1,       (1.0 - (yend - floor(yend))) * xgap);
		plotf(xpxl1, ypxl1 + 1.0,        (yend - floor(yend))  * xgap);
	}
	float intery = yend + gradient; // first y-intersection for the main loop

	// handle second endpoint
	xend = round(x1);
	yend = y1 + gradient * ((float)xend - x1);
	xgap = x1 + 0.5 - floor(x1 + 0.5);
	int16_t xpxl2 = xend; //this will be used in the main loop
	int16_t ypxl2 = floor(yend);
	if(steep) {
		plotf(ypxl2,       xpxl2, (1.0 - (yend - floor(yend))) * xgap);
		plotf(ypxl2 + 1.0, xpxl2,        (yend - floor(yend))  * xgap);
	} else {
		plotf(xpxl2, ypxl2,       (1.0 - (yend - floor(yend))) * xgap);
		plotf(xpxl2, ypxl2 + 1.0,        (yend - floor(yend))  * xgap);
	}

	// main loop
	if(steep) {
		for(uint16_t x = xpxl1 + 1; x < xpxl2; x++) {
			plotf(floor(intery),     x, (1.0 - (intery - floor(intery))));
			plotf(floor(intery) + 1, x,        (intery - floor(intery) ));
			intery += gradient;
		}
	} else {
		for(uint16_t x = xpxl1 + 1; x < xpxl2; x++) {
			plotf(x, floor(intery),     (1.0 - (intery - floor(intery))));
			plotf(x, floor(intery) + 1,        (intery - floor(intery) ));
			intery += gradient;
		}
	}
}

void wudemo() {

	// Clear the canvas
	memset(canvas, 0, sizeof(canvas));

	// Of course it doesn't make sense to use slow floating point trig. functions here
	// This is just for demo purposes
	static float wudemo_v;
	wudemo_v += 0.005;
	float x = sinf(wudemo_v) * 50;
	float y = cosf(wudemo_v) * 50;

	// Draw using fast fixed-point version
	wuline ((x + 125) * (1 << 16), (y + 125) * (1 << 16), (-x + 125) * (1 << 16), (-y + 125) * (1 << 16));

	// Draw using reference version for comparison
	wulinef(  x + 115,                y + 115,               -x + 115,                -y + 115             );

	// -- insert display code here --
	showme(canvas);

}
#endif

SDL_Surface* CRawCanvas::SDL_SurfaceFromRawRGBA(Uint32 *pPixels, int iWidth, int iHeight)
{
	SDL_Surface *pImage = SDL_CreateRGBSurface(SDL_SWSURFACE, iWidth, iHeight,
											   32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN     /* OpenGL RGBA masks */
								 0x000000FF,
								 0x0000FF00, 0x00FF0000, 0xFF000000
#else
								 0xFF000000,
								 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
								 );

	if (nullptr == pImage)
		return nullptr;

	if( SDL_MUSTLOCK(pImage) )
		SDL_LockSurface(pImage);


	Uint32 *pRGBA = pPixels;  // Start with the first pixel

	for (int y = 0; y < iHeight; ++y)
	{
		for (int x = 0; x < iWidth; ++x)
		{
			Uint8* pPixel = (Uint8*)pImage->pixels;
			pPixel += (y*pImage->pitch) + (x * sizeof(Uint32));

			*((Uint32*)pPixel) = *pRGBA++;
		}
	}

	if( SDL_MUSTLOCK(pImage) )
		SDL_UnlockSurface(pImage);

	return pImage;
}

