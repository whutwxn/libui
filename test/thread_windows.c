// 23 april 2019
#define UNICODE
#define _UNICODE
#define STRICT
#define STRICT_TYPED_ITEMIDS
#define WINVER			0x0600
#define _WIN32_WINNT		0x0600
#define _WIN32_WINDOWS	0x0600
#define _WIN32_IE			0x0700
#define NTDDI_VERSION		0x06000000
#include <windows.h>
#include <process.h>
#include <stdlib.h>
#include "thread.h"

// Do not put any test cases in this file; they will not be run.

static HRESULT lastErrorCodeToHRESULT(DWORD lastError)
{
	if (lastError == 0)
		return E_FAIL;
	return HRESULT_FROM_WIN32(lastError);
}

static HRESULT lastErrorToHRESULT(void)
{
	return lastErrorCodeToHRESULT(GetLastError());
}

static HRESULT __cdecl hr_beginthreadex(void *security, unsigned stackSize, unsigned (__stdcall *threadProc)(void *arg), void *threadProcArg, unsigned flags, unsigned *thirdArg, uintptr_t *handle)
{
	DWORD lastError;

	// _doserrno is the equivalent of GetLastError(), or at least that's how _beginthreadex() uses it.
	_doserrno = 0;
	*handle = _beginthreadex(security, stackSize, threadProc, threadProcArg, flags, thirdArg);
	if (*handle == 0) {
		lastError = (DWORD) _doserrno;
		return lastErrorCodeToHRESULT(lastError);
	}
	return S_OK;
}

static HRESULT WINAPI hrWaitForSingleObject(HANDLE handle, DWORD timeout)
{
	DWORD ret;

	SetLastError(0);
	ret = WaitForSingleObject(handle, timeout);
	if (ret == WAIT_FAILED)
		return lastErrorToHRESULT();
	return S_OK;
}

static HRESULT WINAPI hrCreateWaitableTimerW(LPSECURITY_ATTRIBUTES attributes, BOOL manualReset, LPCWSTR name, HANDLE *handle)
{
	SetLastError(0);
	*handle = CreateWaitableTimerW(attributes, manualReset, name);
	if (*handle == NULL)
		return lastErrorToHRESULT();
	return S_OK;
}

static HRESULT WINAPI hrSetWaitableTimer(HANDLE timer, const LARGE_INTEGER *duration, LONG period, PTIMERAPCROUTINE completionRoutine, LPVOID completionData, BOOL resume)
{
	BOOL ret;

	SetLastError(0);
	ret = SetWaitableTimer(timer, duration, period, completionRoutine, completionData, resume);
	if (ret == 0)
		return lastErrorToHRESULT();
	return S_OK;
}

struct threadThread {
	uintptr_t handle;
	void (*f)(void *data);
	void *data;
};

static unsigned __stdcall threadThreadProc(void *data)
{
	threadThread *t = (threadThread *) data;

	(*(t->f))(t->data);
	return 0;
}

threadSysError threadNewThread(void (*f)(void *data), void *data, threadThread **t)
{
	threadThread *tout;
	HRESULT hr;

	*t = NULL;

	tout = (threadThread *) malloc(sizeof (threadThread));
	if (tout == NULL)
		return (threadSysError) E_OUTOFMEMORY;
	tout->f = f;
	tout->data = data;

	hr = hr_beginthreadex(NULL, 0, threadThreadProc, tout, 0, NULL, &(tout->handle));
	if (hr != S_OK) {
		free(tout);
		return (threadSysError) hr;
	}

	*t = tout;
	return 0;
}

threadSysError threadThreadWaitAndFree(threadThread *t)
{
	HRESULT hr;

	hr = hrWaitForSingleObject((HANDLE) (t->handle), INFINITE);
	if (hr != S_OK)
		return (threadSysError) hr;
	CloseHandle((HANDLE) (t->handle));
	free(t);
	return 0;
}

threadSysError threadSleep(threadDuration d)
{
	HANDLE timer;
	LARGE_INTEGER duration;
	HRESULT hr;

	duration.QuadPart = d / 100;
	duration.QuadPart = -duration.QuadPart;
	hr = hrCreateWaitableTimerW(NULL, TRUE, NULL, &timer);
	if (hr != S_OK)
		return (threadSysError) hr;
	hr = hrSetWaitableTimer(timer, &duration, 0, NULL, NULL, FALSE);
	if (hr != S_OK) {
		CloseHandle(timer);
		return (threadSysError) hr;
	}
	hr = hrWaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
	return (threadSysError) hr;
}
