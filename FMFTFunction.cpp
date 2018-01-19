#include "FMFTFunction.h"
#include <stdio.h>

#ifdef _MSC_VER
#include <windows.h>
#include <time.h>
#include <process.h>

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

#else
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>

int vscprintf (const char * format, va_list pargs) { 
	int retval; 
	va_list argcopy; 
	va_copy(argcopy, pargs);
	retval = vsnprintf(NULL, 0, format, argcopy);
	va_end(argcopy);
	return retval;
}

void fmft_log(char* message, ...){
	return;
	char ThreadMessage[1024] = { 0 };
	sprintf(ThreadMessage, "\nThreadID:%lu--", pthread_self());
	int ThreadMessageLen = strlen(ThreadMessage);

	//#ifdef _DEBUG
	va_list vlArgs;
	va_start(vlArgs, message);
	size_t nLen = vscprintf(message, vlArgs) + 1 + ThreadMessageLen;
	char *strBuffer = new char[nLen];
	if (strBuffer == NULL)
	{
		return;
	}
	vsnprintf(strBuffer, nLen, message, vlArgs);
	va_end(vlArgs);

	/*append threadID*/
	memmove(strBuffer + ThreadMessageLen, strBuffer, nLen - ThreadMessageLen);
	memcpy(strBuffer, ThreadMessage, ThreadMessageLen);
	printf("%s", strBuffer);
	printf("\n");
	/*append threadID*/

	delete[]strBuffer;
	///#endif
	return;
}


#endif

void fmft_sleep(int millcro)
{
#ifdef _MSC_VER
	Sleep(millcro);
#else
	usleep(millcro * 1000);
#endif
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
