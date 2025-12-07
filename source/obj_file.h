//
// C++ Decoder
// For simple WaveFront OBJ File
//    
// Currently only supports vertices, and lines
//
#ifndef COBJ_FILE_H
#define COBJ_FILE_H

#include <vector>


class COBJFile
{
public:
	// Load in a COBJ Image File
	COBJFile(const char *pFilePath);

	~COBJFile();

	// Retrieval
	void LoadFromFile(const char* pFilePath);

private:

	int m_numVerts;
	int m_numLines;
};


#endif // COBJ_FILE_H

