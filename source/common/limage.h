//
// LinearImage Scale
//
// Written by Jason Andersen
//


class FloatPixel
{
public:
	FloatPixel()
		: r(0)
		, g(0)
		, b(0)
		, a(0)
	{
	}

	FloatPixel(unsigned int rgba)
	{
		r = (float)((rgba >> 0 ) & 0xFF);
		g = (float)((rgba >> 8 ) & 0xFF);
		b = (float)((rgba >> 16) & 0xFF);
		a = (float)((rgba >> 24) & 0xFF);
	}

	FloatPixel(float red, float green, float blue, float alpha)
		: r(red)
		, g(green)
		, b(blue)
		, a(alpha)
	{ }

	float r;
	float g;
	float b;
	float a;

};


class LinearImage
{
public:

	LinearImage(unsigned int* pPixels, int width, int height);
	LinearImage(int width, int height);
	~LinearImage();

	LinearImage* Scale(int width, int height);
	FloatPixel GetPixel(int x, int y);
	void SetPixel(int x, int y, FloatPixel& pixel);
	FloatPixel SubSample(float x, float y);
	FloatPixel SuperSample(float x, float y, float xRatio, float yRatio);


private:

	FloatPixel Lerp(FloatPixel& left, FloatPixel& right, float lerp);

	int m_width;
	int m_height;

	FloatPixel* m_pPixels;

};
