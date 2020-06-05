//
// Jason's Memory Stream Helper thing
// Serialize a byte stream into real types
// 
// Currently only support Little Endian
//

#ifndef MEMSTREAM_H_
#define MEMSTREAM_H_

#include "bctypes.h"
#include <string>

// Prototypes
void *memcpy(void *dest, const void *src, size_t n);

class MemoryStream
{
public:
	MemoryStream(unsigned char* pStreamStart, size_t streamSize)
		: m_pStreamStart( pStreamStart )
		, m_streamSize(streamSize)
		, m_pStreamCurrent( pStreamStart )
	{}

	~MemoryStream()
	{
		m_pStreamStart = nullptr;
		m_streamSize = 0;
		m_pStreamCurrent = nullptr;
	}

	// Really Dumb Reader Template
	template <class T>
	T Read()
	{
		T result;
	
		memcpy(&result, m_pStreamCurrent, sizeof(T));
		m_pStreamCurrent += sizeof(T);
	
		return result;
	}

	template <class T>
	void Read(T& result)
	{
		memcpy(&result, m_pStreamCurrent, sizeof(result));
		m_pStreamCurrent += sizeof(result);
	}

	void ReadBytes(u8* pDest, size_t numBytes)
	{
		memcpy(pDest, m_pStreamCurrent, numBytes);
		m_pStreamCurrent += numBytes;
	}

	std::string ReadPString()
	{
		std::string result;

		u8 length = *m_pStreamCurrent++;

		result.insert(0, (char*)m_pStreamCurrent, (size_t)length);

		m_pStreamCurrent += length;

		return result;
	}

	std::string ReadLine()
	{
		unsigned char* pStreamEnd = m_pStreamStart + m_streamSize;

		std::string result;

		u8 *pEndOfLine = m_pStreamCurrent;

		while (pEndOfLine < pStreamEnd)
		{
			if ((0x0a != *pEndOfLine) && (0x0d != *pEndOfLine))
			{
				pEndOfLine++;
			}
			else
			{
				break;
			}
		}

		size_t length = pEndOfLine - m_pStreamCurrent;

		if (length)
		{
			// Capture the line
			result.insert(0, (char*)m_pStreamCurrent, (size_t)length);
			m_pStreamCurrent += length;
		}

		// Figure out if that loop broke out because we hit the end of the file
		while (pEndOfLine < pStreamEnd)
		{
			if ((0x0a == *pEndOfLine) || (0x0d == *pEndOfLine))
			{
				pEndOfLine++;
			}
			else
			{
				break;
			}
		}

		m_pStreamCurrent = pEndOfLine;
		if (m_pStreamCurrent > pStreamEnd)
		{
			m_pStreamCurrent = pStreamEnd;
		}
		return result;

	}

	size_t SeekCurrent(int delta)
	{
		m_pStreamCurrent += delta;
		return m_pStreamCurrent - m_pStreamStart;
	}

	size_t SeekSet(size_t offset)
	{
		m_pStreamCurrent = m_pStreamStart + offset;
		return m_pStreamCurrent - m_pStreamStart;
	}

	unsigned char* GetPointer() { return m_pStreamCurrent; }

	size_t NumBytesAvailable() {

		unsigned char* pStreamEnd = m_pStreamStart + m_streamSize;

		if (pStreamEnd - m_pStreamCurrent >= 0)
		{
			return pStreamEnd - m_pStreamCurrent;
		}
		else
		{
			return 0;
		}
	}



private:

	unsigned char* m_pStreamStart;
	size_t m_streamSize;

	unsigned char* m_pStreamCurrent;
	
};


#endif // MEMSTREAM_H_


