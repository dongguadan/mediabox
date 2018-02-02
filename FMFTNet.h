#ifndef _FMFTNET_H
#define _FMFTNET_H

#include <winsock2.h>
#include <string>
#include <map>
#include "ikcp.h"
#include "FMFTReliableBase.h"
using namespace std;

#define FMFT_MTU   1400
#define FMFT_SNIFF "sniff"
#define FMFT_TIMEDIFF  (120*1000)  //us
#define FMFT_TIMESNIFF (10*1000*1000)  //us
#define FMFT_LEN   (1024*256)
#define FMFT_PACKET_MAX_NUM  500

class FMFTNet
{
public:
	FMFTNet();
	~FMFTNet();
	int Init(unsigned long long totalSize, unsigned long long rate, int mtu);
	int Start(string remote, int remotePort, int localPort);
	int Stop();
private:
	bool                    m_Running;

	HANDLE                  m_hThread;
	unsigned int            m_uThreadID;
	static unsigned int WINAPI InitialThreadProc(void *pv);
	unsigned long ThreadProc();

	static int udp_output(const char *buffer, int len, IKCPCB *cb, void *user);
	long handle_vnet_send(const char *buffer, long len);

private:
	unsigned long long GetUsecsPerPacket();
	unsigned long long SetUsecsPerPacket(unsigned long long usecsPerPacket);

	bool GetModifyStatus();
	int  SetModifyStatus(bool status);

	CFMFTReliableBase* GetRBUDP();
	CFMFTReliableBase* GetRBUDP(int id);
	int InsertRBUDP(int id, CFMFTReliableBase* pRBUDP);
	int RemoveRBUDP(int id);
	bool IsRBUDPExist(int id);
	int  ProcessSniff();

private:
	unsigned long long m_totalSize;
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

	CFMFTReliableBase *m_rbudp;
	map<int, CFMFTReliableBase*> m_rbudp_map;
	bool              m_IsModify;
	struct timeval    m_sniff_start;
	struct timeval    m_sniff_stop;
	struct timeval    m_start;
	struct timeval    m_stop;
	int  m_group;
};

#endif
