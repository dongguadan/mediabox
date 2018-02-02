#include "FMFTNet.h"
#include "FMFTFunction.h"
#include <windows.h>
#include <time.h>
#include <process.h> 

#pragma comment(lib, "ws2_32.lib")


FMFTNet::FMFTNet()
{
	OutputDebugStringA("FMFTNet\n");
	m_Running = false;
	m_hThread     = NULL;
}

FMFTNet::~FMFTNet()
{
	m_Running = false;

	if (m_hThread != NULL)
	{
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread);
		m_hThread = NULL;
		m_uThreadID = 0;
	}

	closesocket(m_udp_socket);
	OutputDebugStringA("~FMFTNet\n");
}

int FMFTNet::Init(unsigned long long totalSize, unsigned long long rate, int mtu)
{
	m_totalSize = totalSize;
	m_rate    = rate;
	m_mtu     = mtu;
	m_usecsPerPacket = 8 * m_mtu * 1000 / m_rate;
	fmft_log("FMFTNet -- m_totalSize : %llu, m_rate : %lld, m_mtu : %d, m_usecsPerPacket : %lld\n", m_totalSize, m_rate, m_mtu, m_usecsPerPacket);
	return 0;
}

int FMFTNet::Start(string remote, int remotePort, int localPort)
{
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err != 0)
	{
		fmft_log("FMFTNet InitSocket error\n");
		return -1;
	}

	m_remote_host = remote;
	m_udp_remote_port = remotePort;
	m_udp_local_port = localPort;

	fmft_log("FMFTNet -- m_remote_host : %s, m_udp_remote_port : %d, m_udp_local_port : %d\n", m_remote_host.c_str(), m_udp_remote_port, m_udp_local_port);

	struct hostent *phe;
	memset(&m_udp_remoteAddr, 0, sizeof(m_udp_remoteAddr));
	m_udp_remoteAddr.sin_family = AF_INET;

	if (phe = gethostbyname(m_remote_host.c_str()))
	{
		memcpy(&m_udp_remoteAddr.sin_addr, phe->h_addr, phe->h_length);
	}
	else if ((m_udp_remoteAddr.sin_addr.s_addr = inet_addr(m_remote_host.c_str())) == INADDR_NONE)
	{
		fmft_log("FMFTNet connectUdp can't get host entry\n");
		return -1;
	}
	else
	{
		fmft_log("FMFTNet connectUdp gethostbyname error\n");
		return -1;
	}
	m_udp_remoteAddr.sin_port = htons(m_udp_remote_port);

	if ((m_udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		fmft_log("FMFTNet connectUdp can't get host entry\n");
		return -1;
	}

	int yes = 1;
	if (setsockopt(m_udp_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(int)) == -1)
	{
		fmft_log("FMFTNet connectUdp setsockopt error\n");
		return -1;
	}

	static struct sockaddr_in udpClientAddr;
	memset(&udpClientAddr, 0, sizeof(udpClientAddr));
	udpClientAddr.sin_family = AF_INET;
	udpClientAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	udpClientAddr.sin_port = htons(m_udp_local_port);

	if ((::bind(m_udp_socket, (struct sockaddr *)&udpClientAddr, sizeof(udpClientAddr))) < 0)
	{
		fmft_log("FMFTNet connectUdp bind error\n");
		return -1;
	}
	m_IsModify = false;
	m_Running = true;
	m_group = 0;
	gettimeofday(&m_sniff_start, NULL);
	gettimeofday(&m_sniff_stop, NULL);
	gettimeofday(&m_start, NULL);
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, InitialThreadProc, (void *)this, 0, &m_uThreadID);
	if (m_hThread == NULL)
	{
		fmft_log("FMFTNet _beginthreadex InitialThreadProc error\n");
		return -1;
	}

	return 0;
}

int FMFTNet::Stop()
{
	m_Running = false;
	return 0;
}

int FMFTNet::udp_output(const char *buffer, int len, IKCPCB *cb, void *user)
{
	if (user == NULL)
	{
		return -1;
	}

	if (len > 0)
	{
		((FMFTNet*)user)->handle_vnet_send(buffer, len);
	}

	return 0;
}

long FMFTNet::handle_vnet_send(const char *buffer, long len)
{
	if (sendto(m_udp_socket, buffer, len, 0, (const struct sockaddr *)&m_udp_remoteAddr, sizeof(m_udp_remoteAddr)) < 0)
	{
		fmft_log("FMFTNet handle_vnet_send() error\n");
		return -1;
	}
	return 0;
}

unsigned long long FMFTNet::GetUsecsPerPacket()
{
	return m_usecsPerPacket;
}

unsigned long long FMFTNet::SetUsecsPerPacket(unsigned long long usecsPerPacket)
{
	m_usecsPerPacket = usecsPerPacket;
	return 0;
}

bool FMFTNet::GetModifyStatus()
{
	return m_IsModify;
}

int  FMFTNet::SetModifyStatus(bool status)
{
	m_IsModify = status;
	return 0;
}

unsigned int FMFTNet::InitialThreadProc(void *pv)
{
	FMFTNet *pThis = (FMFTNet *)pv;
	return pThis->ThreadProc();
}

unsigned long FMFTNet::ThreadProc()
{
	m_kcp = ikcp_create(1, this);
	if (m_kcp == NULL)
	{
		OutputDebugStringA("FMFTNet::ThreadProc ikcp_create error\n");
		return -1;
	}
	m_kcp->output = FMFTNet::udp_output;
	ikcp_nodelay(m_kcp, 0, 10, 0, 1, 0);

	struct sockaddr_in remote;
	int addr_len = sizeof(struct sockaddr_in);
	fd_set read_fdset;
	fd_set write_fdset;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 5;
	if (setsockopt(m_udp_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0)
	{
		OutputDebugStringA("FMFTNet::ThreadProc socket option error\n");
		return -1;
	}

	OutputDebugStringA("FMFTNet::ThreadProc start\n");

	char sendbuf[FMFT_LEN] = { 0 };
	char *group = (char *)malloc(sizeof(char) * FMFT_LEN);
	if (group == NULL)
	{
		OutputDebugStringA("FMFTNet::ThreadProc group malloc error\n");
		return -1;
	}

	//ProcessSniff();

	unsigned long long index = 0;
	struct timeval start;
	struct timeval now;
	gettimeofday(&start, NULL);

	while (m_Running == true)
	{
		ikcp_update(m_kcp, iclock());

		FD_ZERO(&read_fdset);
		FD_SET(m_udp_socket, &read_fdset);
		FD_ZERO(&write_fdset);
		FD_SET(m_udp_socket, &write_fdset);

		int ret = select(m_udp_socket + 1, &read_fdset, &write_fdset, NULL, &tv);
		if (ret == 0)
		{
			continue;
		}
		else if (ret == -1)
		{
			OutputDebugStringA("FMFTNet::ThreadProc select error\n");
			break;
		}

		if (FD_ISSET(m_udp_socket, &read_fdset))
		{
			char byteRecv[1400] = { 0 };
			int recvlen = recvfrom(m_udp_socket, byteRecv, sizeof(byteRecv), 0, (struct sockaddr *)&remote, &addr_len);
			if (recvlen > 0)
			{
				ikcp_input(m_kcp, byteRecv, recvlen);
				recvlen = ikcp_recv(m_kcp, byteRecv, sizeof(byteRecv));
				if (recvlen > 0)
				{
					_rbudpHeader header;
					memcpy((char *)&header, byteRecv, sizeof(header));
					//fmft_log("\n\nFMFTNet::ThreadProc ikcp_recv len : %d, group : %d\n", recvlen, header.group);
					m_rbudp = GetRBUDP(header.group);
					if (m_rbudp == NULL)
					{
						continue;
					}
					unsigned long long timediff = USEC(&start, &now);
					m_rbudp->SetErrorMap(byteRecv + sizeof(header), recvlen - sizeof(header));
				}
			}
		}

		if (FD_ISSET(m_udp_socket, &write_fdset))
		{
			gettimeofday(&now, NULL);
			if (USEC(&start, &now) < (GetUsecsPerPacket() * index))
			{
				fmft_sleep(1);
			}
			else
			{
				index++;
				m_rbudp = GetRBUDP();
				if (m_rbudp == NULL)
				{
					if (m_rbudp_map.size() < FMFT_PACKET_MAX_NUM)
					{
						//fmft_log("insert group : %d\n", m_group);
						m_rbudp = new CFMFTReliableBase();
						if (m_rbudp == NULL)
						{
							OutputDebugStringA("FMFTNet::ThreadProc m_rbudp error\n");
							return -1;
						}

						if (m_rbudp->Init(m_group, group, FMFT_LEN, m_rate, m_mtu) < 0)
						{
							OutputDebugStringA("FMFTNet::ThreadProc m_rbudp init error\n");
							return -1;
						}

						InsertRBUDP(m_rbudp->GetID(), m_rbudp);
						m_group++;
					}
					else
					{
						continue;
					}
				}

				memset(sendbuf, 0, FMFT_LEN);
				int packetlen = m_rbudp->GetPacket(sendbuf, FMFT_LEN);
				if (packetlen <= 0)
				{
					int id = m_rbudp->GetID();
					RemoveRBUDP(id);
					//gettimeofday(&m_stop, NULL);
					//fmft_log("FMFTNet::ThreadProc m_rbudp(%d) finished\n", id);
					//unsigned long long timeCost = USEC(&m_start, &m_stop) / 1000;
					//fmft_log("FMFTNet::ThreadProc m_rbudp(%d) cost : %lld\n", id, timeCost);
					continue;
				}

				if (sendto(m_udp_socket, sendbuf, packetlen, 0, (const struct sockaddr *)&m_udp_remoteAddr, sizeof(m_udp_remoteAddr)) < 0)
				{
					OutputDebugStringA("FMFTNet::ThreadProc sendto() error\n");
					break;
				}

				if (m_rbudp->GetCursor() == 0)
				{
					_endOfUdp endup;
					endup.group = m_rbudp->GetID();
					ikcp_send(m_kcp, (char *)&endup, sizeof(endup));
				}
			}
		}
	}

	OutputDebugStringA("FMFTNet::ThreadProc stop\n");

	ikcp_release(m_kcp);

	return 0;
}

int  FMFTNet::ProcessSniff()
{
	struct sockaddr_in remote;
	int addr_len = sizeof(struct sockaddr_in);
	fd_set read_fdset;
	fd_set write_fdset;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1000;

	int trySum = 5;
	int tryNum = 0;

	int sniffLen = m_mtu / 2;
	char *byteSniff = (char *)malloc(sizeof(char) * sniffLen);
	if (byteSniff == NULL)
	{
		OutputDebugStringA("FMFTNet::ProcessSniff malloc error\n");
		return -1;
	}
	memset(byteSniff, 0, sniffLen);

	char *byteRecv = (char *)malloc(sizeof(char) * m_mtu);
	if (byteRecv == NULL)
	{
		OutputDebugStringA("FMFTNet::ProcessSniff malloc error\n");
		return -1;
	}
	memset(byteRecv, 0, m_mtu);

	while (true)
	{
		ikcp_update(m_kcp, iclock());
		FD_ZERO(&read_fdset);
		FD_SET(m_udp_socket, &read_fdset);
		FD_ZERO(&write_fdset);
		FD_SET(m_udp_socket, &write_fdset);

		int ret = select(m_udp_socket + 1, &read_fdset, &write_fdset, NULL, &tv);
		if (ret == 0)
		{
			continue;
		}
		else if (ret == -1)
		{
			OutputDebugStringA("FMFTNet::ProcessSniff select error\n");
			break;
		}

		if (FD_ISSET(m_udp_socket, &read_fdset))
		{
			int recvlen = recvfrom(m_udp_socket, byteRecv, m_mtu, 0, (struct sockaddr *)&remote, &addr_len);
			if (recvlen > 0)
			{
				fmft_log("FMFTNet::ProcessSniff recv len : %d\n", recvlen);

				ikcp_input(m_kcp, byteRecv, recvlen);
				gettimeofday(&m_sniff_stop, NULL);
				unsigned long long timediff = USEC(&m_sniff_start, &m_sniff_stop);
				fmft_log("FMFTNet::ProcessSniff m_sniff_start : %lld, m_sniff_stop : %lld, timediff : %lld\n\n", m_sniff_start, m_sniff_stop, timediff);
				tryNum++;
				if (tryNum >= trySum)
				{
					break;
				}
				fmft_sleep(1000);
			}
		}

		if (FD_ISSET(m_udp_socket, &write_fdset))
		{
			int sndbuf = m_kcp->nsnd_buf;
			int sndque = m_kcp->nsnd_que;
			if ((sndbuf <= 0) && (sndque <= 0))
			{
				ikcp_send(m_kcp, byteSniff, sniffLen);
				fmft_log("FMFTNet::ProcessSniff send len : %d\n", sniffLen);
				gettimeofday(&m_sniff_start, NULL);
			}
		}
	}

	delete byteRecv;
	byteRecv = NULL;
	delete byteSniff;
	byteSniff = NULL;

	return 0;
}

CFMFTReliableBase* FMFTNet::GetRBUDP()
{
	CFMFTReliableBase *pRBUDP = NULL;
	map<int, CFMFTReliableBase*>::iterator iter;
	for (iter = m_rbudp_map.begin(); iter != m_rbudp_map.end(); iter++)
	{
		if (iter->second->GetStatus() == true)
		{
			pRBUDP = iter->second;
			break;
		}
	}
	return pRBUDP;
}

CFMFTReliableBase* FMFTNet::GetRBUDP(int id)
{
	CFMFTReliableBase *pRBUDP = NULL;
	map<int, CFMFTReliableBase*>::iterator iter = m_rbudp_map.find(id);
	if (iter != m_rbudp_map.end())
	{
		pRBUDP = iter->second;
	}
	return pRBUDP;
}

int FMFTNet::InsertRBUDP(int id, CFMFTReliableBase* pRBUDP)
{
	map<int, CFMFTReliableBase*>::iterator iter = m_rbudp_map.find(id);
	if (iter != m_rbudp_map.end())
	{
		return 0;
	}
	m_rbudp_map[id] = pRBUDP;
	return 0;
}

int FMFTNet::RemoveRBUDP(int id)
{
	CFMFTReliableBase *pRBUDP = NULL;
	map<int, CFMFTReliableBase*>::iterator iter = m_rbudp_map.find(id);
	if (iter != m_rbudp_map.end())
	{
		pRBUDP = iter->second;
		m_rbudp_map.erase(iter);
		delete pRBUDP;
		pRBUDP = NULL;
	}

	return 0;
}

bool FMFTNet::IsRBUDPExist(int id)
{
	map<int, CFMFTReliableBase*>::iterator iter = m_rbudp_map.find(id);
	if (iter != m_rbudp_map.end())
	{
		return true;
	}
	return false;
}