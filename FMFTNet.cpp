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

int FMFTNet::Start(string remote, int sniffPort, int remotePort, int localPort)
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
	memset(&m_rudp_remoteAddr, 0, sizeof(m_rudp_remoteAddr));
	m_rudp_remoteAddr.sin_family = AF_INET;

	if (phe = gethostbyname(m_remote_host.c_str()))
	{
		memcpy(&m_udp_remoteAddr.sin_addr, phe->h_addr, phe->h_length);
		memcpy(&m_rudp_remoteAddr.sin_addr, phe->h_addr, phe->h_length);
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
	m_rudp_remoteAddr.sin_port = htons(sniffPort);


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
	if (sendto(m_udp_socket, buffer, len, 0, (const struct sockaddr *)&m_rudp_remoteAddr, sizeof(m_rudp_remoteAddr)) < 0)
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
		OutputDebugStringA("ThreadProc ikcp_create error\n");
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
	tv.tv_usec = 10;
	if (setsockopt(m_udp_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0)
	{
		OutputDebugStringA("ThreadProc socket option error\n");
		return -1;
	}

	struct timeval sniff_start;
	struct timeval sniff_stop;
	gettimeofday(&sniff_start, NULL);
	gettimeofday(&sniff_stop, NULL);
	long long timesniff = 0;

	unsigned long long index = 0;
	struct timeval start;
	struct timeval now;
	gettimeofday(&start, NULL);

	OutputDebugStringA("ThreadProc start\n");

	while (m_Running == true)
	{
		ikcp_update(m_kcp, iclock());
		gettimeofday(&sniff_stop, NULL);
		timesniff = USEC(&sniff_start, &sniff_stop);
		if (timesniff > FMFT_TIMESNIFF)
		{
			gettimeofday(&sniff_start, NULL);
			int snd = m_kcp->nsnd_buf;
			if (snd <= 0)
			{
				ikcp_send(m_kcp, FMFT_SNIFF, strlen(FMFT_SNIFF));
				fmft_log("send content:%s, len:%d, snd:%d\n", FMFT_SNIFF, strlen(FMFT_SNIFF), snd);
			}
		}

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
			OutputDebugStringA("ThreadProc select error\n");
			break;
		}

		if (FD_ISSET(m_udp_socket, &read_fdset))
		{
			char byteRecv[1400] = { 0 };
			int recvlen = recvfrom(m_udp_socket, byteRecv, sizeof(byteRecv), 0, (struct sockaddr *)&remote, &addr_len);
			if (recvlen > 0)
			{
				ikcp_input(m_kcp, byteRecv, recvlen);
				fmft_log("recv len : %d\n\n", recvlen);
			}
		}

		if (FD_ISSET(m_udp_socket, &write_fdset))
		{
			gettimeofday(&now, NULL);
			if (USEC(&start, &now) < (GetUsecsPerPacket() * index))
			{
				Sleep(1);
			}
			else
			{
				char byteSend[1400] = { 0 };
				if (sendto(m_udp_socket, byteSend, sizeof(byteSend), 0, (const struct sockaddr *)&m_udp_remoteAddr, sizeof(m_udp_remoteAddr)) < 0)
				{
					OutputDebugStringA("ThreadProc sendto() error\n");
					break;
				}
				else
				{
					index++;
				}
			}
		}
	}

	OutputDebugStringA("ThreadProc stop\n");

	ikcp_release(m_kcp);
	return 0;
}