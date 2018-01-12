#ifndef _FMFTNET_H
#define _FMFTNET_H

#include <winsock2.h>
#include <string>
using namespace std;

#define FMFT_MTU   1400

class FMFTNet
{
public:
	FMFTNet();
	~FMFTNet();
	int Init(unsigned long long bufSize, unsigned long long rate, int mtu);
	int Start(string remote, int remotePort, int localPort);
	int Stop();
public:
	bool                    m_Running;
	HANDLE                  m_hThread;
	unsigned int            m_uThreadID;

	static unsigned int WINAPI InitialThreadProc(void *pv);
	unsigned long ThreadProc();
private:
	unsigned long long m_bufSize;
	unsigned long long m_rate;
	int                m_mtu;
	unsigned long long m_usecsPerPacket;

	int                m_udp_socket;
	int                m_udp_remote_port;
	int                m_udp_local_port;
	string             m_remote_host;
	struct sockaddr_in m_udp_remoteAddr;

};

#endif
