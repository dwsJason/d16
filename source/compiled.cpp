//
// Sprite Compiler for the GS
//


#include "compiled.h"

//-----------------------------------------------------------------------------

CCompiledData::CCompiledData(u8* pDest, u8* pSource, int size_bytes)
{
	for (int index = 0; index < size_bytes; ++index)
	{
		// Skip shit that didn't change
		if (pSource[ index ] == pDest[ index ])
		{
			continue;
		}

		if (index < (size_bytes-1) && (pSource[ index + 1 ] == pDest[ index + 1 ]))
		{
			// we have a single byte!

			u8 token = pDest[ index ];

			auto it = m_ByteMap.find(token);

			if (it != m_ByteMap.end())
			{
				it->second.push_back(index);
			}
			else
			{
				std::vector<int> offsets;
				offsets.push_back(index);
				m_ByteMap[ token ] = offsets;
			}
		}
		else
		{
			// We have 2 bytes that changed
			u16 token = pDest[ index + 1 ];
			token<<=8;
			token |= pDest[ index ];

			auto it = m_ShortMap.find(token);

			if (it != m_ShortMap.end())
			{
				it->second.push_back(index);
			}
			else
			{
				std::vector<int> offsets;
				offsets.push_back(index);
				m_ShortMap[ token ] = offsets;
			}


			// we need to skip
			++index;
		}
	}
}

CCompiledData::~CCompiledData()
{
}

//-----------------------------------------------------------------------------

void CCompiledData::SetName(std::string name)
{
	m_name = name;
}

//-----------------------------------------------------------------------------

void CCompiledData::CalcBlitClocks()
{
}

//-----------------------------------------------------------------------------

void ExportBlit()
{
}

//-----------------------------------------------------------------------------

