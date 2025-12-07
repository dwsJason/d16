//
// C++ Decoder
// For simple WaveFront OBJ File
//
// Currently only supports vertices, and lines
// 
#include "obj_file.h"

#include <stdio.h>

//------------------------------------------------------------------------------
// Load in a COBJFile constructor
//
COBJFile::COBJFile(const char *pFilePath)
	: m_numVerts( 0 )
	, m_numLines( 0 )
{
	LoadFromFile(pFilePath);
}
//------------------------------------------------------------------------------

COBJFile::~COBJFile()
{
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

void COBJFile::LoadFromFile(const char* pFilePath)
{
	//--------------------------------------------------------------------------

	std::vector<unsigned char> bytes;

	//--------------------------------------------------------------------------
	// Read the file into memory
	FILE* pFile = nullptr;
	errno_t err = fopen_s(&pFile, pFilePath, "rb");

	if (0==err)
	{
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
	}
}

//------------------------------------------------------------------------------
