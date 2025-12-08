//
// C++ Decoder
// For simple WaveFront OBJ File
//
// Currently only supports vertices, and lines
// 
#include "obj_file.h"
#include "bctypes.h"
#include "memstream.h"

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
// Static Helpers $$TODO just add these to the memStream class
//                       we always need these

static bool contains(char x, const char* pSeparators)
{
	while (*pSeparators)
	{
		if (x == *pSeparators)
		{
			return true;
		}
		pSeparators++;
	}

	return false;
}

static std::vector<std::string> split(const std::string& s, const char* separators)
{
	std::vector<std::string> output;
	std::string::size_type prev_pos = 0, pos = 0;

	for (int index = 0; index < s.length(); ++index)
	{
		pos = index;

		// if the index is a separator
		if (contains(s[index], separators))
		{
			// if we've skipped a token, collect it
			if (prev_pos != index)
			{
				output.push_back(s.substr(prev_pos, index-prev_pos));

				// skip white space here
				while (index < s.length())
				{
					if (contains(s[index], separators))
					{
						++index;
					}
					else
					{
						prev_pos = index;
						pos = index;
						break;
					}
				}
			}
			else
			{
				prev_pos++;
			}
		}
	}

    output.push_back(s.substr(prev_pos, pos-prev_pos+1)); // Last word

    return output;
}

// make this string lowercase
static void tolower(std::string& s)
{
	for (int index = 0; index < s.length(); ++index)
	{
		s[index] = tolower( s[index] );
	}
}

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
		MemoryStream memStream(bytes.data(), bytes.size());

		while (memStream.NumBytesAvailable())
		{
			std::string lineData = memStream.ReadLine();

			std::vector<std::string> tokens = split(lineData, " \t");

			// technically # is the comment token
			// but all lines I don't understand, are "comments"

			if (tokens.size())
			{
				if (tokens[0] == "center")
				{
					// grab the center vertex
				}
				else if (tokens[0] == "v")
				{
					// add a vertex
					if (tokens.size() >= 3)
					{
						// there's at least 2 numbers here, in theory
						// which is all I want (at least for now)

					}

				}
				else if (tokens[0] == "l")
				{
					// add a line segment
				}

			}

		}
	}
}

//------------------------------------------------------------------------------
