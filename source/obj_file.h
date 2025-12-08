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

	// Retrieval
	void LoadFromFile(const char* pFilePath);

private:

	// center of rotation
	OBJFILE::vec2 m_center; 

	std::vector<OBJFILE::vec2> m_points;
	std::vector<OBJFILE::int2> m_lines;

};


#endif // COBJ_FILE_H

