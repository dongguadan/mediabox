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
}

FMFTNet::~FMFTNet()
{
	m_Running = false;

	if (m_hThreadSend != NULL)
	{
		WaitForSingleObject(m_hThreadSend, INFINITE);
		CloseHandle(m_hThreadSend);
		m_hThreadSend = NULL;
		m_uThreadIDSend = 0;
	}

	if (m_hThreadRecv != NULL)
	{
		WaitForSingleObject(m_hThreadRecv, INFINITE);
		CloseHandle(m_hThreadRecv);
		m_hThreadRecv = NULL;
		m_uThreadIDRecv = 0;
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
	m_hThreadSend = (HANDLE)_beginthreadex(NULL, 0, InitialThreadProcSend, (void *)this, 0, &m_uThreadIDSend);
	if (m_hThreadSend == NULL)
	{
		fmft_log("FMFTNet _beginthreadex InitialThreadProcSend error\n");
		return -1;
	}

	m_hThreadRecv = (HANDLE)_beginthreadex(NULL, 0, InitialThreadProcRecv, (void *)this, 0, &m_uThreadIDRecv);
	if (m_hThreadRecv == NULL)
	{
		fmft_log("FMFTNet _beginthreadex InitialThreadProcRecv  error\n");
		return -1;
	}

	return 0;
}

int FMFTNet::Stop()
{
	m_Running = false;
	return 0;
}

unsigned int FMFTNet::InitialThreadProcSend(void *pv)
{
	FMFTNet *pThis = (FMFTNet *)pv;
	return pThis->ThreadProcSend();
}

unsigned long FMFTNet::ThreadProcSend()
{
	struct timeval start, now;
	unsigned long long index = 0;
	unsigned long long total = 0;
	char *payload = (char *)malloc(sizeof(unsigned char) * FMFT_LEN);
	if (payload == NULL)
	{
		fmft_log("FMFTNet malloc error\n");
		return -1;
	}

	OutputDebugStringA("FMFTNet start\n");

	gettimeofday(&start, NULL);

	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 10;
	fd_set rfd;

	while ((m_Running == true) && (total < m_totalSize))
	{
		unsigned long long sendSize = 0;
		if ((total + FMFT_LEN) <= m_totalSize)
		{
			sendSize = FMFT_LEN;
		}
		else
		{
			sendSize = m_totalSize - total;
		}

		m_rbudp = new CFMFTReliableBase();
		if (m_rbudp == NULL)
		{
			fmft_log("FMFTNet m_rbudp error\n");
			return -1;
		}

		if (m_rbudp->Init(payload, sendSize, m_rate, m_mtu) < 0)
		{
			fmft_log("FMFTNet m_rbudp Init error\n");
			return -1;
		}

		unsigned long long packetTotal = 0;
		while (m_Running == true)
		{
			if (m_IsModify == true)
			{
				m_IsModify = false;
				gettimeofday(&start, NULL);
				index = 0;
				continue;
			}

			gettimeofday(&now, NULL);
			if (USEC(&start, &now) < m_usecsPerPacket * index)
			{
				Sleep(1);
			}
			else
			{
				/*********test********/
				int packetSize = m_rbudp->GetPacket(payload, FMFT_LEN);
				if (packetSize < 0)
				{
					fmft_log("FMFTNet GetPacket() error(%lld)\n", packetSize);
					break;
				}
				else if (packetSize == 0)
				{
					fmft_log("FMFTNet GetPacket() finished\n");
					total += sendSize;
					break;
				}

				if (((packetTotal / packetSize) % 100) == 0)
				{
					m_rbudp->UpdateErrorMap(0);
				}
				/*********************/


				if (sendto(m_udp_socket, payload, packetSize, 0, (const struct sockaddr *)&m_udp_remoteAddr, sizeof(m_udp_remoteAddr)) < 0)
				{
					fmft_log("FMFTNet sendto() error\n");
					break;
				}
				else
				{
					index++;
					packetTotal += packetSize;
				}
			}
		}

		fmft_log("sendSize : %lld, total : %lld\n", sendSize, total);
		delete m_rbudp;
		m_rbudp = NULL;
	}

	delete payload;
	payload = NULL;

	OutputDebugStringA("FMFTNet finish\n");
	return 0;
}

unsigned int FMFTNet::InitialThreadProcRecv(void *pv)
{
	FMFTNet *pThis = (FMFTNet *)pv;
	return pThis->ThreadProcRecv();
}

unsigned long FMFTNet::ThreadProcRecv()
{
	struct sockaddr_in remote;
	int addr_len = sizeof(struct sockaddr_in);
	char *byteRecv = (char *)malloc(sizeof(unsigned char) * m_mtu);
	if (byteRecv == NULL)
	{
		OutputDebugStringA("ThreadProcRecv malloc error\n");
		return -1;
	}

	char *log = (char *)malloc(sizeof(unsigned char) * m_mtu);
	if (log == NULL)
	{
		OutputDebugStringA("ThreadProcRecv malloc error\n");
		return -1;
	}

	m_kcp = ikcp_create(1, this);
	if (m_kcp == NULL)
	{
		OutputDebugStringA("ThreadProcRecv ikcp_create error\n");
		return -1;
	}

	m_kcp->output = FMFTNet::udp_output;

	ikcp_nodelay(m_kcp, 0, 10, 0, 1, 0);

	fd_set fds;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1000*100;
	if (setsockopt(m_udp_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0)
	{
		OutputDebugStringA("ThreadProcRecv socket option error\n");
		return -1;
	}

	int snd = m_kcp->nsnd_buf;
	memset(log, 0, m_mtu);
	sprintf(log, "ThreadProcRecv start wnd:%d\n", snd);
	OutputDebugStringA(log);

	struct timeval sniff_start;
	struct timeval sniff_stop;
	unsigned long long usecsPerPacket_bak = m_usecsPerPacket;
	bool isBalance = false;

	while (m_Running == true)
	{
		ikcp_update(m_kcp, iclock());
		snd = m_kcp->nsnd_buf;
		if (snd <= 0)
		{
			ikcp_send(m_kcp, FMFT_SNIFF, strlen(FMFT_SNIFF));
			memset(log, 0, m_mtu);
			sprintf(log, "send content:%s, len:%d, snd:%d\n", FMFT_SNIFF, strlen(FMFT_SNIFF), snd);
			OutputDebugStringA(log);

			gettimeofday(&sniff_start, NULL);
		}

		FD_ZERO(&fds);
		FD_SET(m_udp_socket, &fds);
		int ret = select(m_udp_socket, &fds, NULL, NULL, &tv);
		if (ret == 0)
		{
			continue;
		}
		else if (ret == -1)
		{
			OutputDebugStringA("Error..\n");
			break;
		}

		int recvlen = recvfrom(m_udp_socket, byteRecv, m_mtu, 0, (struct sockaddr *)&remote, &addr_len);
		if (recvlen > 0)
		{
			ikcp_input(m_kcp, byteRecv, recvlen);
			memset(log, 0, m_mtu);
			sprintf(log, "recv len:%d\n\n", recvlen);
			OutputDebugStringA(log);

			gettimeofday(&sniff_stop, NULL);
			long long timediff = USEC(&sniff_start, &sniff_stop);
			memset(log, 0, m_mtu);
			sprintf(log, "timediff : %lld\n\n", timediff);
			OutputDebugStringA(log);

			if (isBalance == false)
			{
				if (timediff < FMFT_TIMEDIFF)
				{
					usecsPerPacket_bak = m_usecsPerPacket;
					m_usecsPerPacket /= 2;
				}
				else
				{
					isBalance = true;
					m_usecsPerPacket = usecsPerPacket_bak;
				}
				m_IsModify = true;
				memset(log, 0, m_mtu);
				sprintf(log, "m_usecsPerPacket : %lld\n\n", m_usecsPerPacket);
				OutputDebugStringA(log);
			}
			Sleep(10000);
		}
		else
		{
			OutputDebugStringA("recv none\n");
		}
	}

	delete byteRecv;
	byteRecv = NULL;

	delete log;
    log = NULL;

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