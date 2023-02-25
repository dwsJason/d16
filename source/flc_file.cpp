//
// C++ Decoder
// For FLC/FLI, Autodesk File Format
// 
#include "flc_file.h"
#include <stdio.h>
#include <assert.h>


//------------------------------------------------------------------------------
// Load in a FanFile constructor
//
FlcFile::FlcFile(const char *pFilePath)
	: m_widthPixels(0)
	, m_heightPixels(0)
	, m_numColors( 0 )
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

		std::vector<unsigned char> bytes(header.width, header.height);
		flic::Frame frame;

		frame.pixels = &bytes[0];
		frame.rowstride = header.width;

		m_widthPixels  = header.width;
		m_heightPixels = header.height;

		for (int i=0; i<header.frames; ++i) {
			if (!decoder.readFrame(frame))
				return 3;
		}

		fseek(pFile, 0, SEEK_END);
		size_t length = ftell(pFile);	// get file size
		fseek(pFile, 0, SEEK_SET);

		bytes.resize( length );			// make sure buffer is large enough

		// Read in the file
		fread(&bytes[0], sizeof(unsigned char), bytes.size(), pFile);
		fclose(pFile);
	}

	if (bytes.size())
	{
		//size_t file_offset = 0;	// File Cursor

		m_widthPixels = 320;
		m_heightPixels = 200;
		m_numColors = 256;

		ANM_Header* pHeader = (ANM_Header*)&bytes[0];

		if (pHeader->IsValid())
		{
			//unsigned char* FirstLPage = &bytes[ sizeof(ANM_Header) ];
			// Looks Valid

			// I don't support palette animation, just just grab
			// the palette from the header
			m_pal.iNumColors = 256;
			m_pal.pColors = new ANM_Color[ 256 ];
			for (int idx = 0; idx < 256; ++idx)
			{
				u32 srcColor = pHeader->palette[ idx ];

				ANM_Color color;
				color.r = (u8)(srcColor & 0xFF);
				color.g = (u8)((srcColor >> 8) & 0xFF);
				color.b = (u8)((srcColor >> 16) & 0xFF);
				color.a = 255;

				m_pal.pColors[ idx ] = color;
			}

			// This is another format that applies delta changes
			// to a reference buffer
			unsigned char* pCanvas = new unsigned char[ 320 * 200 ];
			memset(pCanvas, 0, 320*200);

			// Now grab first frame of animation
			unsigned char* pData = FindRecord(pHeader, 0);
			DecodeFrame(pCanvas, pData);

			unsigned char* pFrame = new unsigned char[320 * 200];
			memcpy(pFrame, pCanvas, 320 * 200 );
			m_pPixelMaps.push_back(pFrame);



			// loop through and grab the rest of the frames
			for (int frameNo = 1; frameNo < (int)pHeader->nFrames; ++frameNo)
			{
				pData = FindRecord(pHeader, frameNo);

				// Now we're pointed at compressed data, probably
				DecodeFrame(pCanvas, pData);

				pFrame = new unsigned char[320 * 200];
				memcpy(pFrame, pCanvas, 320 * 200 );
				m_pPixelMaps.push_back(pFrame);
			}
		}
	}
}

//------------------------------------------------------------------------------

void FlcFile::SaveToFile(const char* pFilenamePath)
{
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

