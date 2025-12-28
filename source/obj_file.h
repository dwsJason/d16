//
// C++ Decoder
// For simple WaveFront OBJ File
//    
// Currently only supports vertices, and lines
//
#ifndef COBJ_FILE_H
#define COBJ_FILE_H

#include <vector>
#include <string>


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

struct object
{
	std::string m_name;
	std::vector<int> m_lines;
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
	const OBJFILE::vec2& GetHotSpot() { return m_hotspot; }

	// Retrieval
	void LoadFromFile(const char* pFilePath);

	const std::vector<OBJFILE::vec2>& GetPoints() { return m_points; }
	const std::vector<OBJFILE::int2>& GetLines()  { return m_lines; }
	const OBJFILE::vec2& GetCenter() { return m_center; }

	const bool IsFont() { return m_bFont; }
	const OBJFILE::vec2& GetFontSize() { return m_fontsize; }

	const std::vector<OBJFILE::object>& GetObjects() { return m_objects; }

private:

	bool m_bFont;
	OBJFILE::vec2 m_fontsize;

	int m_width;
	int m_height;

	// hot spot
	OBJFILE::vec2 m_hotspot; // location where to render the vector image in the canvas

	// center of rotation
	OBJFILE::vec2 m_center; 
	OBJFILE::vec2 m_scale;

	std::vector<OBJFILE::vec2> m_points;
	std::vector<OBJFILE::int2> m_lines;

	std::vector<OBJFILE::object> m_objects;

};


#endif // COBJ_FILE_H

