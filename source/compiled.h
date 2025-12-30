//
// C++ CompiledData class
//

#ifndef COMPILED_DATA_H
#define COMPILED_DATA_H

#include <map>
#include <vector>
#include <string>

#include "bctypes.h"

class CCompiledData
{
public:

	CCompiledData(u8* pDest, u8* pSource, int size_bytes=0x8000);
	~CCompiledData();


	void SetName(std::string name);

	int ExportBlit(std::vector<u8>& output);

private:

	int AddLine(std::vector<u8>& output, char* pLabel, char* pInst, char* pExp, int val, int clocks);

	std::string m_name;

	// map constants, to list of memory addresses
	std::map<u8, std::vector<int>>  m_ByteMap;
	std::map<u16, std::vector<int>> m_ShortMap;

	// if we have less than 256 unique bytes total, we could use direct page
	// loads -- which would allow us to reuse code for erase, and perhaps
	// reuse code for remapping colors (just all a memory thing)


};


#endif // COMPILE_DATA_H


