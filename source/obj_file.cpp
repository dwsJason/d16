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
	: m_width(0)
	, m_height(0)
{
	m_bFont = false;
	m_fontsize.x = 16.0f;
	m_fontsize.y = 16.0f;

	m_center.x = 0.0f;
	m_center.y = 0.0f;

	m_hotspot.x = 0.0f;
	m_hotspot.y = 0.0f;

	m_scale.x = m_scale.y = 1.0f;

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

		OBJFILE::object global_object;
		global_object.m_name = "$global";

		m_objects.push_back(global_object);

		OBJFILE::object& current_object = m_objects[0];


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

						for (int pointsIndex = 1; pointsIndex < (tokens.size() - 1); ++pointsIndex)
						{
							OBJFILE::int2 ivec;

							sscanf_s(tokens[pointsIndex+0].c_str(), "%d", &ivec.x );
							sscanf_s(tokens[pointsIndex+1].c_str(), "%d", &ivec.y );

							m_lines.push_back(ivec);

							// track which lines belong to this object
							current_object.m_lines.push_back( (int)m_lines.size() );
						}
					}
				}
				else if (tokens[0] == "scale")
				{
					if (tokens.size() == 2)
					{
						float scale = 1.0f;
						sscanf_s(tokens[1].c_str(), "%f", &scale );
						m_scale.x = scale;
						m_scale.y = scale;
					}
					else if (tokens.size() >= 3)
					{
						sscanf_s(tokens[1].c_str(), "%f", &m_scale.x );
						sscanf_s(tokens[2].c_str(), "%f", &m_scale.y );
					}
				}
				else if (tokens[0] == "font")
				{
					m_bFont = true;

					if (tokens.size() == 2)
					{
						float fontsize = 16.0f;
						sscanf_s(tokens[1].c_str(), "%f", &fontsize );
						m_fontsize.x = fontsize;
						m_fontsize.y = fontsize;
					}
					else if (tokens.size() >= 3)
					{
						sscanf_s(tokens[1].c_str(), "%f", &m_fontsize.x );
						sscanf_s(tokens[2].c_str(), "%f", &m_fontsize.y );
					}
				}
				else if (tokens[0] == "canvas")
				{
					// set the render canvas size
					if (tokens.size() >= 3)
					{
						sscanf_s(tokens[1].c_str(), "%d", &m_width );
						sscanf_s(tokens[2].c_str(), "%d", &m_height );

						if ((0.0f == m_hotspot.x) && (0.0f == m_hotspot.y))
						{
							m_hotspot.x = (float)(m_width / 2.0f) - 1.0f;
							m_hotspot.y = (float)(m_height / 2.0f) - 1.0f;
						}
					}
				}
				else if (tokens[0] == "hotspot")
				{
					if (tokens.size() >= 3)
					{
						sscanf_s(tokens[1].c_str(), "%f", &m_hotspot.x );
						sscanf_s(tokens[2].c_str(), "%f", &m_hotspot.y );
					}
				}
				else if (tokens[0] == "o")
				{
					if (tokens.size() >= 2)
					{
						// object!
						OBJFILE::object new_object;
						m_objects.push_back(new_object);

						// current object, reference object in the list
						current_object = m_objects[m_objects.size() - 1];

						current_object.m_name = tokens[1];
					}
				}

			}
		}

		// auto width + height
		if (0==m_width || 0==m_height)
		{
			//$$TODO -- analyze the data to make a good width + height
			m_width  = 640;
			m_height = 400;
		}
	}
}

//------------------------------------------------------------------------------

void COBJFile::GetWidthHeight(int* pWidth, int* pHeight)
{
	pWidth[0] = m_width;
	pHeight[0] = m_height;
}

//------------------------------------------------------------------------------

