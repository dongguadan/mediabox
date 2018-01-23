#ifndef _FMFTRELIABLEBASE_H
#define _FMFTRELIABLEBASE_H

struct _rbudpHeader
{
	int seq;
	int group;
};

struct _endOfUdp
{
	int round;
	char end[3];
};

class CFMFTReliableBase
{
public:
	CFMFTReliableBase();
	~CFMFTReliableBase();
	int Init(int group, char * buffer, int bufSize, int sendRate, int packetSize);
	int GetPacket(char *buffer, int bufSize);
	int GetCursor();
	int GetID();
	bool GetStatus();
	int GetRemainNumberOfPackets();
	int UpdateErrorMap(long long seq, char *buffer, int bufSize);
	int GetErrorMap(char *buffer, int bufSize);
	int SetErrorMap(char *buffer, int bufSize);
private:
	char    *m_mainBuffer;
	int     m_dataSize;
	int     m_sendRate;
	int     m_payloadSize;
	int     m_headerSize;
	int     m_packetSize;
	int     m_totalNumberOfPackets;
	int     m_remainNumberOfPackets;
	int     m_receivedNumberOfPackets;
	int     m_lastPayloadSize;
	int     m_usecsPerPacket;
	int     m_udpSockBufSize;

	int     m_cursor;
	int     m_group;
	int     m_running;

	long long    *m_hashTable;
	char         *m_errorBitmap;
	int          m_sizeofErrorBitmap;
	int          m_isFirstBlast;

	bool         m_peerswap;

	struct _rbudpHeader m_sendHeader;
	struct _rbudpHeader m_recvHeader;
private:
	static long long htonll(long long);
	static long long ntohll(long long);
	static int       swab32(int);

	void initErrorBitmap();
	void updateErrorBitmap(long long seq);
	int  updateHashTable();
	int  ptohseq(int seq);
};


#endif
