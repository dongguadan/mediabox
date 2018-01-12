#include "FMFTNet.h"

#include <windows.h>
#include <time.h>
#include <process.h> 

#pragma comment(lib, "ws2_32.lib")

#define USEC(st, fi) (((fi)->tv_sec-(st)->tv_sec)*1000000+((fi)->tv_usec-(st)->tv_usec))

void fmft_log(char* message, ...){

	char ThreadMessage[1024] = { 0 };
	sprintf_s(ThreadMessage, "\nThreadID:%lu--", GetCurrentThreadId());
	int ThreadMessageLen = strlen(ThreadMessage);

	va_list vlArgs = NULL;
	va_start(vlArgs, message);
	size_t nLen = _vscprintf(message, vlArgs) + 1 + ThreadMessageLen;
	char *strBuffer = new char[nLen];
	if (strBuffer == NULL)
	{
		return;
	}
	_vsnprintf_s(strBuffer, nLen, nLen, message, vlArgs);
	va_end(vlArgs);

	memmove(strBuffer + ThreadMessageLen, strBuffer, nLen - ThreadMessageLen);
	memcpy(strBuffer, ThreadMessage, ThreadMessageLen);
	OutputDebugStringA(strBuffer);

	delete[]strBuffer;
}

static int gettimeofday(struct timeval *tp, void *tzp)
{
	time_t clock;
	struct tm tm;
	SYSTEMTIME wtm;
	GetLocalTime(&wtm);
	tm.tm_year = wtm.wYear - 1900;
	tm.tm_mon = wtm.wMonth - 1;
	tm.tm_mday = wtm.wDay;
	tm.tm_hour = wtm.wHour;
	tm.tm_min = wtm.wMinute;
	tm.tm_sec = wtm.wSecond;
	clock = mktime(&tm);
	tp->tv_sec = clock;
	tp->tv_usec = wtm.wMilliseconds * 1000;
	return 0;
}

FMFTNet::FMFTNet()
{
	OutputDebugStringA("FMFTNet\n");
	m_Running = false;
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

int FMFTNet::Init(unsigned long long bufSize, unsigned long long rate, int mtu)
{
	m_bufSize = bufSize;
	m_rate    = rate;
	m_mtu     = mtu;
	m_usecsPerPacket = 8 * m_mtu * 1000 / m_rate;
	fmft_log("FMFTNet -- m_bufSize : %llu, m_rate : %lld, m_mtu : %d, m_usecsPerPacket : %lld\n", m_bufSize, m_rate, m_mtu, m_usecsPerPacket);
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

	m_hThread = (HANDLE)_beginthreadex(NULL, 0, InitialThreadProc, (void *)this, 0, &m_uThreadID);
	if (m_hThread == NULL)
	{
		fmft_log("FMFTNet _beginthreadex error\n");
		return -1;
	}

	return 0;
}

int FMFTNet::Stop()
{
	m_Running = false;
	return 0;
}

unsigned int FMFTNet::InitialThreadProc(void *pv)
{
	FMFTNet *pThis = (FMFTNet *)pv;
	return pThis->ThreadProc();
}


unsigned long FMFTNet::ThreadProc()
{
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





	struct timeval start, now;
	unsigned long long index = 0;
	unsigned long long total = 0;
	char *payload = (char *)malloc(sizeof(unsigned char) * m_mtu);
	if (payload == NULL)
	{
		fmft_log("FMFTNet malloc error\n");
		return -1;
	}

	char *getload = (char *)malloc(sizeof(unsigned char) * m_mtu);
	if (getload == NULL)
	{
		fmft_log("FMFTNet malloc error\n");
		return -1;
	}

	OutputDebugStringA("FMFTNet start\n");

	m_Running = true;
	gettimeofday(&start, NULL);

	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 10;
	fd_set rfd;

	while ((m_Running == true) && (total < m_bufSize))
	{
		unsigned long long sendSize = 0;
		gettimeofday(&now, NULL);
		if (USEC(&start, &now) < m_usecsPerPacket * index)
		{

		}
		else
		{
			if ((total + m_mtu) <= m_bufSize)
			{
				sendSize = m_mtu;
			}
			else
			{
				sendSize = m_bufSize - total;
			}

			if (sendto(m_udp_socket, payload, sendSize, 0, (const struct sockaddr *)&m_udp_remoteAddr, sizeof(m_udp_remoteAddr)) < 0)
			{
				fmft_log("FMFTNet sendto() error\n");
				break;
			}
			else
			{
				total += sendSize;
				index++;
			}
		}

	}

	delete payload;
	payload = NULL;

	delete getload;
	getload = NULL;

	OutputDebugStringA("FMFTNet finish\n");
	return 0;
}