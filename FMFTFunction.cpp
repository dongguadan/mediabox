#include "FMFTFunction.h"
#include <windows.h>
#include <time.h>
#include <process.h> 
#include <stdio.h>

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

int gettimeofday(struct timeval *tp, void *tzp)
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

void itimeofday(long *sec, long *usec)
{
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
}

unsigned long long iclock64(void)
{
	long s, u;
	unsigned long long value;
	itimeofday(&s, &u);
	value = ((unsigned long long)s) * 1000 + (u / 1000);
	return value;
}

unsigned int iclock()
{
	return (unsigned int)(iclock64() & 0xfffffffful);
}
