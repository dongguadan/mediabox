#include "FMFTReliableBase.h"
#include "FMFTFunction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctime>

CFMFTReliableBase::CFMFTReliableBase()
{
	//fmft_log("CFMFTReliableBase\n");
	return;
}

CFMFTReliableBase::~CFMFTReliableBase()
{
	if (m_mainBuffer != NULL)
	{
		delete m_mainBuffer;
		m_mainBuffer = NULL;
	}
	//fmft_log("~CFMFTReliableBase\n");
	return;
}

int CFMFTReliableBase::Init(int group, char * buffer, int bufSize, int sendRate, int packetSize)
{
	m_mainBuffer = (char *)malloc(bufSize * sizeof(char));
	if (m_mainBuffer == NULL)
	{
		fmft_log("CFMFTReliableBase::Init malloc error\n");
		return -1;
	}
	memcpy(m_mainBuffer, buffer, bufSize);

	m_group = group;
	m_dataSize = bufSize;
	m_sendRate = sendRate;
	m_payloadSize = packetSize;
	m_headerSize = sizeof(struct _rbudpHeader);
	m_packetSize = m_payloadSize + m_headerSize;
	m_usecsPerPacket = 8 * m_payloadSize * 1000 / sendRate;
	m_isFirstBlast = 1;

	if (m_dataSize % m_payloadSize == 0)
	{
		m_totalNumberOfPackets = m_dataSize / m_payloadSize;
		m_lastPayloadSize = m_payloadSize;
	}
	else
	{
		m_totalNumberOfPackets = m_dataSize / m_payloadSize + 1; /* the last packet is not full */
		m_lastPayloadSize = m_dataSize - m_payloadSize * (m_totalNumberOfPackets - 1);
	}

	m_remainNumberOfPackets = m_totalNumberOfPackets;
	m_sizeofErrorBitmap = m_totalNumberOfPackets / 8 + 2;
	m_errorBitmap = (char *)malloc(m_sizeofErrorBitmap);
	m_hashTable = (long long *)malloc(m_totalNumberOfPackets * sizeof(long long));

	m_cursor = 0;

	for (int i = 0; i < m_totalNumberOfPackets; i++)
	{
		m_hashTable[i] = i;
	}

	initErrorBitmap();

	m_running = true;

	return 0;
}

int CFMFTReliableBase::swab32(int val)
{
	return (((val >> 24) & 0xFF) | ((val >> 8) & 0xFF00)
		| ((val & 0xFF00) << 8) | ((val & 0xFF) << 24));
}

long long CFMFTReliableBase::htonll(long long lll)
{
	long long nll = 0;
	unsigned char *cp = (unsigned char *)&nll;

	cp[0] = (lll >> 56) & 0xFF;
	cp[1] = (lll >> 48) & 0xFF;
	cp[2] = (lll >> 40) & 0xFF;
	cp[3] = (lll >> 32) & 0xFF;
	cp[4] = (lll >> 24) & 0xFF;
	cp[5] = (lll >> 16) & 0xFF;
	cp[6] = (lll >> 8) & 0xFF;
	cp[7] = (lll >> 0) & 0xFF;

	return nll;
}

long long CFMFTReliableBase::ntohll(long long nll)
{
	unsigned char *cp = (unsigned char *)&nll;

	return ((long long)cp[0] << 56) |
		((long long)cp[1] << 48) |
		((long long)cp[2] << 40) |
		((long long)cp[3] << 32) |
		((long long)cp[4] << 24) |
		((long long)cp[5] << 16) |
		((long long)cp[6] << 8) |
		((long long)cp[7] << 0);
}

void CFMFTReliableBase::initErrorBitmap()
{
	int i;
	int startOfLastByte = m_totalNumberOfPackets - (m_sizeofErrorBitmap - 2) * 8;
	unsigned char bits[8] = { 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080 };

	for (i = 0; i < m_sizeofErrorBitmap; i++)
	{
		m_errorBitmap[i] = 0;
	}

	for (i = startOfLastByte; i < 8; i++)
	{
		m_errorBitmap[m_sizeofErrorBitmap - 1] |= bits[i];
	}

	/* Hack: we're not sure whether the peer is the same
	* endian-ness we are, and the RBUDP protocol doesn't specify.
	* Let's assume that it's like us, but keep a flag
	* to set if we see unreasonable-looking sequence numbers.
	*/
	m_peerswap = false;
}

int  CFMFTReliableBase::ptohseq(int origseq)
{
	int seq = origseq;

	if (m_peerswap)
	{
		seq = swab32(origseq);
	}

	if (seq < 0 || (seq >> 3) >= m_sizeofErrorBitmap - 1)
	{
		if (!m_peerswap)
		{
			m_peerswap = true;
			return ptohseq(seq);
		}
		else
		{
			return 0;
		}
	}
	return seq;
}


void CFMFTReliableBase::updateErrorBitmap(long long seq)
{
	long long index_in_list, offset_in_index;
	unsigned char bits[8] = { 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080 };
	if (m_peerswap)
	{
		seq = swab32(seq);
	}

	if (seq < 0 || (seq >> 3) >= m_sizeofErrorBitmap - 1)
	{
		if (!m_peerswap)
		{
			m_peerswap = true;
		}
		seq = swab32(seq);
	}
	if (seq < 0 || (seq >> 3) >= m_sizeofErrorBitmap - 1)
	{
		return;
	}
	// seq starts with 0
	index_in_list = seq >> 3;
	index_in_list++;
	offset_in_index = seq % 8;
	m_errorBitmap[index_in_list] |= bits[offset_in_index];
}

int CFMFTReliableBase::updateHashTable()
{
	int count = 0;
	int i, j;
	unsigned char bits[8] = { 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080 };

	for (i = 1; i<m_sizeofErrorBitmap; i++)
	{
		for (j = 0; j<8; j++)
		{
			if ((m_errorBitmap[i] & bits[j]) == 0)
			{
				m_hashTable[count] = (i - 1) * 8 + j;
				count++;
			}
		}
	}

	if (count == 0)
	{
		m_errorBitmap[0] = 1;
	}

	return count;
}

int CFMFTReliableBase::GetPacket(char *buffer, int bufSize)
{
	if (m_remainNumberOfPackets <= 0)
	{
		return 0;
	}

	int len = 0;
	int actualPayloadSize = 0;

	if (m_hashTable[m_cursor] < m_totalNumberOfPackets - 1)
	{
		actualPayloadSize = m_payloadSize;
	}
	else
	{
		actualPayloadSize = m_lastPayloadSize;
	}

	if (bufSize < (actualPayloadSize + m_headerSize))
	{
		fmft_log("CFMFTReliableBase::GetPacket error\n");
		return -1;
	}

	m_sendHeader.seq = m_hashTable[m_cursor];
	m_sendHeader.group = m_group;
	memcpy(buffer, &m_sendHeader, m_headerSize);
	memcpy(buffer + m_headerSize, (char*)((char*)m_mainBuffer + (m_sendHeader.seq * m_payloadSize)), actualPayloadSize);

	len = actualPayloadSize + m_headerSize;

	m_cursor++;
	if (m_cursor >= m_remainNumberOfPackets)
	{
		m_cursor = 0;
		m_running = false;
	}

	return len;
}

int CFMFTReliableBase::GetRemainNumberOfPackets()
{
	return m_remainNumberOfPackets;
}

int CFMFTReliableBase::GetCursor()
{
	return m_cursor;
}

int CFMFTReliableBase::UpdateErrorMap(long long seq, char *buffer, int bufSize)
{
	seq = ptohseq(seq);
	updateErrorBitmap(seq);
	m_remainNumberOfPackets = updateHashTable();

	memcpy(m_mainBuffer + (seq*m_payloadSize), buffer, bufSize);

	//fmft_log("CFMFTReliableBase::UpdateErrorMap : group(%d)\n", m_group);
	//fmft_log("CFMFTReliableBase::UpdateErrorMap : seq(%d)\n", seq);
	//fmft_log("CFMFTReliableBase::UpdateErrorMap : m_remainNumberOfPackets(%d)\n\n", m_remainNumberOfPackets);

	//printf("CFMFTReliableBase::UpdateErrorMap : group(%d)\n", m_group);
	//printf("CFMFTReliableBase::UpdateErrorMap : seq(%d)\n", seq);
	//printf("CFMFTReliableBase::UpdateErrorMap : m_remainNumberOfPackets(%d)\n\n", m_remainNumberOfPackets);

	m_running = true;
	return 0;
}

int CFMFTReliableBase::GetErrorMap(char *buffer, int bufSize)
{
	if (bufSize < m_sizeofErrorBitmap)
	{
		fmft_log("CFMFTReliableBase::GetErrorMap error\n");
		return -1;
	}
	memcpy(buffer, m_errorBitmap, m_sizeofErrorBitmap);
	return m_sizeofErrorBitmap;
}

int CFMFTReliableBase::SetErrorMap(char *buffer, int bufSize)
{
	if (bufSize > m_sizeofErrorBitmap)
	{
		fmft_log("CFMFTReliableBase::SetErrorMap error\n");
		return -1;
	}
	memcpy(m_errorBitmap, buffer, m_sizeofErrorBitmap);
	m_remainNumberOfPackets = updateHashTable();

	//fmft_log("CFMFTReliableBase::UpdateErrorMap : group(%d)\n", m_group);
	//fmft_log("CFMFTReliableBase::UpdateErrorMap : m_remainNumberOfPackets(%d)\n\n", m_remainNumberOfPackets);
	m_running = true;
	return m_sizeofErrorBitmap;
}

int CFMFTReliableBase::GetID()
{
	return m_group;
}

bool CFMFTReliableBase::GetStatus()
{
	return m_running;
}