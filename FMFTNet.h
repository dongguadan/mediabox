#ifndef _FMFTNET_H
#define _FMFTNET_H

#include <winsock2.h>
#include <string>
#include "ikcp.h"
using namespace std;

#define FMFT_MTU   1400
#define FMFT_SNIFF "sniff"
#define FMFT_TIMEDIFF  150000  //us
#define FMFT_TIMESNIFF 10000  //ms

class FMFTNet
{
public:
	FMFTNet();
	~FMFTNet();
	int Init(unsigned long long bufSize, unsigned long long rate, int mtu);
	int Start(string remote, int sniffPort, int remotePort, int localPort);
	int Stop();
private:
	bool                    m_Running;

	HANDLE                  m_hThreadSend;
	unsigned int            m_uThreadIDSend;
	static unsigned int WINAPI InitialThreadProcSend(void *pv);
	unsigned long ThreadProcSend();

	HANDLE                  m_hThreadRecv;
	unsigned int            m_uThreadIDRecv;
	static unsigned int WINAPI InitialThreadProcRecv(void *pv);
	unsigned long ThreadProcRecv();

	static int udp_output(const char *buffer, int len, IKCPCB *cb, void *user);
	long handle_vnet_send(const char *buffer, long len);

private:
	unsigned long long m_bufSize;
	unsigned long long m_rate;
	int                m_mtu;
	unsigned long long m_usecsPerPacket;

	int                m_udp_socket;
	int                m_udp_remote_port;
	int                m_udp_local_port;
	int                m_rudp_remote_port;
	string             m_remote_host;
	struct sockaddr_in m_udp_remoteAddr;
	struct sockaddr_in m_rudp_remoteAddr;
	ikcpcb            *m_kcp;

	bool              m_IsModify;
};

#endif
