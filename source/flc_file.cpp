//
// C++ Decoder
// For FLC/FLI, Autodesk File Format
// 
#include "flc_file.h"
#include <stdio.h>
#include <assert.h>


//------------------------------------------------------------------------------
// Load in a FlcFile constructor
//
FlcFile::FlcFile(const char *pFilePath)
	: m_widthPixels(0)
	, m_heightPixels(0)
	, m_numColors( 0 )
	, m_speed(50)   // 50ms or 20FPS
{

//	m_pal.iNumColors = 0;
//	m_pal.pColors = nullptr;

	LoadFromFile(pFilePath);
}
//------------------------------------------------------------------------------

FlcFile::~FlcFile()
{
//	if (m_pal.pColors)
//	{
//		delete[] m_pal.pColors;
//		m_pal.pColors = nullptr;
//	}
	// Free Up the memory
	for (int idx = 0; idx < m_pPixelMaps.size(); ++idx)
	{
		delete[] m_pPixelMaps[idx];
		m_pPixelMaps[ idx ] = nullptr;
	}
}

//------------------------------------------------------------------------------

void FlcFile::LoadFromFile(const char* pFilePath)
{
	// Free any existing memory
//	if (m_pal.pColors)
//	{
//		delete[] m_pal.pColors;
//		m_pal.pColors = nullptr;
//	}
	// Free Up the memory
	for (int idx = 0; idx < m_pPixelMaps.size(); ++idx)
	{
		delete[] m_pPixelMaps[idx];
		m_pPixelMaps[ idx ] = nullptr;
	}
	m_pPixelMaps.clear();

	//--------------------------------------------------------------------------

	// Read the file into memory
	FILE* pFile = nullptr;
	errno_t err = fopen_s(&pFile, pFilePath, "rb");

	if (0==err)
	{
		flic::StdioFileInterface file(pFile);
		flic::Decoder decoder(&file);
		flic::Header header;

		if (!decoder.readHeader(header))
		{
			return;
		}

		std::vector<unsigned char> bytes(header.width * header.height);

		frame.pixels = &bytes[0];
		frame.rowstride = header.width;

		m_widthPixels  = header.width;
		m_heightPixels = header.height;

		for (int idx=0; idx<header.frames; ++idx)
		{
			if (!decoder.readFrame(frame))
				return;


			unsigned char* pFrame = new unsigned char[ header.width * header.height ];
			memcpy(pFrame, &bytes[0], header.width * header.height);
			m_pPixelMaps.push_back(pFrame);
		}
	}

}

//------------------------------------------------------------------------------

void FlcFile::SaveToFile(const char* pFilenamePath)
{
	FILE* f = std::fopen(pFilenamePath, "wb");

	if (f)
	{
		flic::StdioFileInterface file(f);
		flic::Encoder encoder(&file);
		flic::Header header;
		header.frames = GetFrameCount();
		header.width = GetWidth();
		header.height = GetHeight();
		header.speed = m_speed;
		encoder.writeHeader(header);

		for (int frame_no = 0; frame_no < GetFrameCount(); ++frame_no)
		{
			frame.pixels = (uint8_t *)m_pPixelMaps[ frame_no ];
			frame.rowstride = m_widthPixels;
			encoder.writeFrame(frame);
		}

		// Render first frame and call writeRingFrame()
		frame.pixels = (uint8_t*)m_pPixelMaps[ 0 ];
		encoder.writeRingFrame(frame);
	}

#if 0
	void render_frame(const flic::Header& header, flic::Frame& frame,
                  const int frameNumber)
{
  // Palettes can change frame by frame
  for (int c=0; c<256; ++c)
    frame.colormap[c] = flic::Color(c, c, 255*frameNumber/header.frames);

  // Frame image
  for (int y=0; y<header.height; ++y)
    for (int x=0; x<header.width; ++x)
      frame.pixels[y*frame.rowstride + x] = ((x*y*frameNumber) % 256);
}

int main(int argc, char* argv[])
{
  if (argc < 2)
    return 1;

  FILE* f = std::fopen(argv[1], "wb");
  flic::StdioFileInterface file(f);
  flic::Encoder encoder(&file);
  flic::Header header;
  header.frames = 10;
  header.width = 32;
  header.height = 32;
  header.speed = 50;              // Speed is in milliseconds per frame
  encoder.writeHeader(header);

  std::vector<uint8_t> buffer(header.width * header.height);
  flic::Frame frame;
  frame.pixels = &buffer[0];
  frame.rowstride = header.width;

  for (int i=0; i<header.frames; ++i) {
    render_frame(header, frame, i);
    encoder.writeFrame(frame);
  }

  // Render first frame and call writeRingFrame()
  render_frame(header, frame, 0);
  encoder.writeRingFrame(frame);

  return 0;
}
#endif
}

//------------------------------------------------------------------------------

