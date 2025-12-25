//
// C++ Decoder
// For simple WaveFront OBJ File
//    
// Currently only supports vertices, and lines
//
#ifndef COBJ_FILE_H
#define COBJ_FILE_H

#include <vector>


namespace OBJFILE
{

struct vec2
{
	float x,y;
};

struct int2
{
	int x,y;
};

};


class COBJFile
{
public:
	// Load in a COBJ Image File
	COBJFile(const char *pFilePath);

	~COBJFile();

	// Information
	void GetWidthHeight(int* pWidth, int* pHeight);
	const OBJFILE::vec2& GetScale() { return m_scale; }

	// Retrieval
	void LoadFromFile(const char* pFilePath);

	const std::vector<OBJFILE::vec2>& GetPoints() { return m_points; }
	const std::vector<OBJFILE::int2>& GetLines()  { return m_lines; }
	const OBJFILE::vec2& GetCenter() { return m_center; }

private:

	int m_width;
	int m_height;

	// center of rotation
	OBJFILE::vec2 m_center; 
	OBJFILE::vec2 m_scale;

	std::vector<OBJFILE::vec2> m_points;
	std::vector<OBJFILE::int2> m_lines;

};


#endif // COBJ_FILE_H

