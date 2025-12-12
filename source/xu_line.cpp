
#include "xu_line.h"

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
