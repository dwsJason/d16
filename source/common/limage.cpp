//
// LinearImage Scale
//
// Written by Jason Andersen
//

#include "limage.h"


LinearImage::LinearImage(unsigned int* pPixels, int width, int height)
	: m_width(width)
	, m_height(height)
{
	m_pPixels = new FloatPixel[ width * height ];
   
	int numPixels = width * height;
	
	for (int idx = 0; idx < numPixels; ++idx)
	{
		m_pPixels[ idx ] = FloatPixel(pPixels[idx]);
	}

}

LinearImage::LinearImage(int width, int height)
	: m_width(width)
	, m_height(height)
{
	m_pPixels = new FloatPixel[ width * height ];  
}

LinearImage::~LinearImage()
{
 	delete[] m_pPixels;
}


LinearImage* LinearImage::Scale(int width, int height)
{
	LinearImage* result = new LinearImage(width, height);

	float xRatio = (float)m_width  / (float)width;
	float yRatio = (float)m_height / (float)height;

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			float sourceX = x * xRatio;
			float sourceY = y * yRatio;

			FloatPixel pixel = SuperSample(sourceX,sourceY,xRatio,yRatio);

			result->SetPixel(x,y, pixel);
		}
	}

	return result;
}

//
// Sample an Area
//
FloatPixel LinearImage::SuperSample(float x, float y, float xRatio, float yRatio)
{
	FloatPixel result;

	int stepsX = xRatio < 1.0f ? 1 : ((int)(xRatio * 2)+1);
	int stepsY = yRatio < 1.0f ? 1 : ((int)(yRatio * 2)+1);

	float sx = stepsX > 1 ? x - (xRatio*0.5f) : x;
	float sy = stepsY > 1 ? y - (yRatio*0.5f) : y;

	float dx = stepsX > 1 ? xRatio / (stepsX-1) : 0;
	float dy = stepsY > 1 ? yRatio / (stepsY-1) : 0;
	
	for (int coordinateY = 0; coordinateY < stepsY; ++coordinateY)
	{
		for (int coordinateX = 0; coordinateX < stepsX; ++coordinateX)
		{
			FloatPixel ss = SubSample( sx + (coordinateX * dx),
									   sy + (coordinateY * dy) );

			result.r += ss.r;
			result.g += ss.g;
			result.b += ss.b;
			result.a += ss.a;
		}
	}

	float sampleCount = (float)stepsX * stepsY;

	result.r /= sampleCount;
	result.g /= sampleCount;
	result.b /= sampleCount;
	result.a /= sampleCount;

	if (result.r > 255.0f) result.r = 255.0f;
	if (result.g > 255.0f) result.g = 255.0f;
	if (result.b > 255.0f) result.b = 255.0f;
	if (result.a > 255.0f) result.a = 255.0f;

	return result;
}

//
// Fetch a pixel
//
FloatPixel LinearImage::GetPixel(int x, int y)
{
	// Clamp against bounds
	if (x < 0) x = 0;
	if (x >= m_width) x = m_width-1;
	if (y < 0) y = 0;
	if (y >= m_height) y = m_height-1;

	return m_pPixels[ (y*m_width + x) ];
}

//
// Set Pixel
//
void LinearImage::SetPixel(int x, int y, FloatPixel& pixel)
{
	m_pPixels[ (y*m_width) + x ] = pixel;
}

//
// Linear Sample from this image
//
FloatPixel LinearImage::SubSample(float x, float y)
{
	int ix = (int) x;
	int iy = (int) y;

	FloatPixel upperLeft  = GetPixel(ix+0, iy+0);
	FloatPixel upperRight = GetPixel(ix+1, iy+0);
	FloatPixel lowerLeft  = GetPixel(ix+0, iy+1);
	FloatPixel lowerRight = GetPixel(ix+1, iy+1);

	float xlerp = (x - ix);
	float ylerp = (y - iy);

	upperLeft = Lerp(upperLeft, upperRight, xlerp);
	lowerLeft = Lerp(lowerLeft, lowerRight, xlerp);

	return Lerp(upperLeft, lowerLeft, ylerp);
}

//
// Linear Interpolate between 2 pixels
//
FloatPixel LinearImage::Lerp(FloatPixel& left, FloatPixel& right, float lerp)
{
	FloatPixel pixel;

	pixel.r = ((right.r - left.r) * lerp) + left.r;
	pixel.g = ((right.g - left.g) * lerp) + left.g;
	pixel.b = ((right.b - left.b) * lerp) + left.b;
	pixel.a = ((right.a - left.a) * lerp) + left.a;

	return pixel;
}


