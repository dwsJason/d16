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
{
	m_center.x = 0.0f;
	m_center.y = 0.0f;

	m_scale = 1.0f;

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
#if 0
static void tolower(std::string& s)
{
	for (int index = 0; index < s.length(); ++index)
	{
		s[index] = tolower( s[index] );
	}
}
#endif

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
					if (tokens.size() >= 3)
					{
						sscanf_s(tokens[1].c_str(), "%f", &m_center.x);
						sscanf_s(tokens[2].c_str(), "%f", &m_center.y);
					}
				}
				else if (tokens[0] == "v")
				{
					// add a vertex
					if (tokens.size() >= 3)
					{
						// there's at least 2 numbers here, in theory
						// which is all I want (at least for now)
						OBJFILE::vec2 vec;

						sscanf_s(tokens[1].c_str(), "%f", &vec.x );
						sscanf_s(tokens[2].c_str(), "%f", &vec.y );

						m_points.push_back(vec);
					}

				}
				else if (tokens[0] == "l")
				{
					// add a line segment
					if (tokens.size() >= 3)
					{
						// the line has at least 2 points (it could have more)
						// $$TODO support more than 2 points
						OBJFILE::int2 ivec;

						sscanf_s(tokens[1].c_str(), "%d", &ivec.x );
						sscanf_s(tokens[2].c_str(), "%d", &ivec.y );

						m_lines.push_back(ivec);
					}
				}
				else if (tokens[0] == "scale")
				{
					if (tokens.size() >= 2)
					{
						sscanf_s(tokens[1].c_str(), "%f", &m_scale );
					}
				}

			}

		}
	}
}

//------------------------------------------------------------------------------

void COBJFile::GetWidthHeight(int* pWidth, int* pHeight)
{
	pWidth[0] = 640;
	pHeight[0] = 400;
}

//------------------------------------------------------------------------------

