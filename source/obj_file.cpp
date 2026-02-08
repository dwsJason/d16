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
#include "compat.h"
#include <assert.h>

//------------------------------------------------------------------------------
// Load in a COBJFile constructor
//
COBJFile::COBJFile(const char *pFilePath)
	: m_width(0)
	, m_height(0)
{
	m_bFont = false;
	m_bAtlas = false;
	m_fontsize.x = 16.0f;
	m_fontsize.y = 16.0f;

	m_center.x = 0.0f;
	m_center.y = 0.0f;

	m_hotspot.x = 0.0f;
	m_hotspot.y = 0.0f;

	m_scale.x = m_scale.y = 1.0f;

	m_rotationframes = 1;
	m_scaleframes = 1;  // default - 1 scale

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

static std::string toLower(const std::string s)
{
	std::string result = s;

	for (int idx = 0; idx < result.size(); ++idx)
	{
		result[ idx ] = (char)tolower(result[idx]);
	}

	return result;
}

// Case Insensitive
static bool endsWith(const std::string& S, const std::string& SUFFIX)
{
	bool bResult = false;

	std::string s = toLower(S);
	std::string suffix = toLower(SUFFIX);

    bResult = s.rfind(suffix) == (s.size()-suffix.size());

	return bResult;
}

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

	if (!contains(s[s.length() - 1], separators))
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

void COBJFile::LoadFromSVG(std::vector<unsigned char>& bytes)
{
	if (bytes.size())
	{
		MemoryStream memStream(bytes.data(), bytes.size());

		OBJFILE::object global_object;
		global_object.m_name = "$global";

		m_objects.push_back(global_object);

		OBJFILE::object* pCurrentObject = &m_objects[0];

		bool path_is_open = false;

		OBJFILE::vec2 cursor = { 0.0f, 0.0f };

		while (memStream.NumBytesAvailable())
		{
			std::string lineData = memStream.ReadLine();

			std::vector<std::string> tokens = split(lineData, " \t<>\"");

			// technically # is the comment token
			// but all lines I don't understand, are "comments"

			char mode = 'l';

			if (tokens.size())
			{
				if (path_is_open)
				{
					if (tokens[0] == "d=")
					{
						cursor = { 0.0f, 0.0f }; // reset the cursor for this specific path
						mode = 'l'; 			 // reset mode for this path

						if (tokens[1] == "M" || tokens[1] == "m")
						{
							char move = tokens[1][0];
							if ('M' == move)
							{
								mode = 'L';
							}

							int current_point = (int)m_points.size()+1;
							int close_loop = false;

							for (int index = 2; index < tokens.size(); ++index)
							{
								if (tokens[index] == "z" || tokens[index] == "Z")
								{
									// mixing Mz or mZ is just a dump case, not supporting it
									close_loop = true;
								}
								else if (tokens[index] == "l" || tokens[index] == "L")
								{
									// not supporting Ml or mL
									mode = tokens[index][0];
								}
								else if (tokens[index] == "h" || tokens[index] == "H")
								{
									// not supporting Mh or mH
									// technically we're in horizontal line mode
									mode = tokens[index][0];
								}
								else if (tokens[index] == "v" || tokens[index] == "V")
								{
									// not supporting Mv or mV
									mode =tokens[index][0];
									// technically we're in vertical line mode
								}
								else if (tokens[index] == "c" || tokens[index] == "C")
								{
									assert(0); // curve to
								}
								else if (tokens[index] == "s" || tokens[index] == "S")
								{
									assert(0); // smooth curve to
								}
								else if (tokens[index] == "q" || tokens[index] == "Q")
								{
									assert(0); // quadradic Bezier curve to
								}
								else if (tokens[index] == "t" || tokens[index] == "T")
								{
									assert(0); // smooth quadradic Bezier curve to
								}
								else if (tokens[index] == "a" || tokens[index] == "A")
								{
									assert(0); // elliptical arc
								}
								else
								{
									OBJFILE::vec2 vec;

									switch (mode)
									{
									case 'l':
									case 'L':
										{
											std::vector<std::string> xy = split(tokens[index], ",");

											sscanf_s(xy[0].c_str(), "%f", &vec.x );
											sscanf_s(xy[1].c_str(), "%f", &vec.y );

											if (mode=='l')
											{
												vec.x+=cursor.x;
												vec.y+=cursor.y;
											}
										}
										break;
									case 'v':
									case 'V':
										{
											sscanf_s(tokens[index].c_str(), "%f", &vec.y );
											vec.x = m_points[ m_points.size() -1 ].x;

											if (mode=='v')
											{
												vec.y+=cursor.y;
											}
										}
										break;
									case 'h':
									case 'H':
										{
											sscanf_s(tokens[index].c_str(), "%f", &vec.x );
											vec.y = m_points[ m_points.size() -1 ].y;

											if (mode=='h')
											{
												vec.x+=cursor.x;
											}
										}
										break;
									default:
										assert(0);
									}


									cursor.x = vec.x;
									cursor.y = vec.y;


									m_points.push_back(vec);
								}
							}

							for (int index = current_point; index < m_points.size(); index++)
							{
								OBJFILE::int2 ivec;

								ivec.x = index;
								ivec.y = index+1;

								m_lines.push_back(ivec);

								// track which lines belong to this object
								pCurrentObject->m_lines.push_back( (int)m_lines.size() );
							}

							if (close_loop)
							{
								OBJFILE::int2 ivec;
								ivec.x = current_point;
								ivec.y = (int)m_points.size();

								cursor.x = m_points[ current_point-1 ].x;
								cursor.y = m_points[ current_point-1 ].y;

								// if (ivec.x != ivec.y) add this to skip plotting 0 length lines
								{
									m_lines.push_back(ivec);
									// track which lines belong to this object
									pCurrentObject->m_lines.push_back( (int)m_lines.size() );
								}
							}
						}

						// we have a new absolute path
						path_is_open = false;
					}
				}
				else
				{
					if (tokens[tokens.size()-1] == "path")
					{
						path_is_open = true;
					}
				}
			}
		}

		m_scale.x = 5.0f;
		m_scale.y = 5.0f;

		m_center.x = 16.0;
		m_center.y = 16.0;

		m_hotspot.x = 320.0f;
		m_hotspot.y = 200.0f;

		m_rotationframes = 1;


		// auto width + height
		if (0==m_width || 0==m_height)
		{
			//$$TODO -- analyze the data to make a good width + height
			m_width  = 640;
			m_height = 400;

			if (m_points.size() > 0)
			{
				// I really need this, at the moment for xml files
				OBJFILE::vec2 bounds_min;
				OBJFILE::vec2 bounds_max;

				bounds_min.x = bounds_max.x = m_points[0].x;
				bounds_min.y = bounds_max.y = m_points[0].y;

				for (int pointIndex = 1; pointIndex < m_points.size(); ++pointIndex)
				{
					const OBJFILE::vec2& point = m_points[ pointIndex ];

					if (point.x < bounds_min.x) bounds_min.x = point.x;
					if (point.y < bounds_min.y) bounds_min.y = point.y;
					if (point.x > bounds_max.x) bounds_max.x = point.x;
					if (point.y > bounds_max.y) bounds_max.y = point.y;
				}

				m_center.x = (bounds_min.x + bounds_max.x) / 2.0f;
				m_center.y = (bounds_min.y + bounds_max.y) / 2.0f;

				float width  = bounds_max.x - bounds_min.x;
				float height = bounds_max.y - bounds_min.y;

				if ((width > 0.0f) && (height > 0.0f))
				{
					float scale = 1.0f;

					if (width > height)
					{
						scale = 56.0f / width;
					}
					else
					{
						scale = 56.0f / height;
					}

					m_scale.x = m_scale.y = scale;
				}
			}
		}
	}
}

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

	if (endsWith(pFilePath, ".svg"))
	{
		LoadFromSVG(bytes);
	}
	else if (bytes.size())
	{
		MemoryStream memStream(bytes.data(), bytes.size());

		OBJFILE::object global_object;
		global_object.m_name = "$global";

		m_objects.push_back(global_object);

		OBJFILE::object* pCurrentObject = &m_objects[0];


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
							pCurrentObject->m_lines.push_back( (int)m_lines.size() );
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
						pCurrentObject = &m_objects[m_objects.size() - 1];

						pCurrentObject->m_name = tokens[1];
					}
				}
				else if (tokens[0] == "atlas" && tokens.size() >= 2)
				{
					int bIsAtlas = 0;
					sscanf_s(tokens[1].c_str(), "%d", &bIsAtlas );

					m_bAtlas = bIsAtlas > 0;
				}
				else if (tokens[0] == "anim_rotate_frames" && tokens.size() >= 2)
				{
					sscanf_s(tokens[1].c_str(), "%d", &m_rotationframes );
				}
				else if (tokens[0] == "anim_scale_frames" && tokens.size() >= 2)
				{
					sscanf_s(tokens[1].c_str(), "%d", &m_scaleframes );
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

