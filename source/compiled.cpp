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
//

int CCompiledData::ExportBlit(std::vector<u8>& output)
{
	int bytes_total = 0;
	int clocks = 0;

	for (auto const& [key, address] : m_ShortMap)
	{
		u16 pixel = key;

		clocks += AddLine(output,"","LDA","#$%04X",pixel,3);
		bytes_total += 3;

		for (int idx = 0; idx < address.size(); ++idx)
		{
			char *pFormat = "|$%04X,X";
			int addy = address[idx];
			bytes_total += 3;

			if (addy & 0x8000)
			{
				addy &= 0x7FFF;
				addy |= 0xE00000;
				pFormat = ">$%06X,X";
				bytes_total += 1;
			}
			clocks += AddLine(output,"", "STA", pFormat, addy, 6);
		}
	}

	clocks += AddLine(output,"","SEP","#$20 ;%d cycles",clocks+3,3);
	bytes_total += 2;

	for (auto const& [key, address] : m_ByteMap)
	{
		u8 pixel = key;

		clocks += AddLine(output,"","LDA","#$%02X",pixel,2);
		bytes_total += 2;

		for (int idx = 0; idx < address.size(); ++idx)
		{
			char *pFormat = "|$%04X,X";
			int addy = address[idx];
			bytes_total += 3;

			if (addy & 0x8000)
			{
				addy &= 0x7FFF;
				addy |= 0xE00000;
				pFormat = ">$%06X,X";
				bytes_total += 1;
			}
			clocks += AddLine(output,"", "STA", pFormat, addy, 5);
		}
	}

	clocks += AddLine(output,"","REP","#$30 ;%d cycles",clocks+3,3);
	bytes_total += 2;

	clocks += AddLine(output,"","RTL"," ;%d cycles",clocks+6,6);
	bytes_total += 1;

	return bytes_total;

}

//-----------------------------------------------------------------------------

int CCompiledData::AddLine(std::vector<u8>& output,char*pLabel,char*pInst,char*pExp,int val,int clocks)
{
	char temp[256];
	char pArg[256];

	memset(pArg,0,256);
	sprintf(pArg,pExp,val);

	sprintf(temp, "%8s %3s %s\n", pLabel,pInst,pArg);

	for (int idx = 0; idx < 256; ++idx)
	{
		if (temp[idx])
		{
			output.push_back(temp[idx]);
		}
		else
		{
			break;
		}
	}

	return clocks;
}

//-----------------------------------------------------------------------------

