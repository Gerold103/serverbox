#include "Thread.h"

#include <process.h>

namespace mg {
namespace box {

#if IS_COMPILER_MSVC
#pragma warning(push)
	// "Possible infinite loop.Execution restarts in the protected block".
#pragma warning(disable: 6312)
#endif

	void
	ThreadSetCurrentName(
		const char* aName)
	{
		typedef struct tagTHREADNAME_INFO
		{
			DWORD dwType;
			LPCSTR szName;
			DWORD dwThreadID;
			DWORD dwFlags;
		} THREADNAME_INFO;

		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = aName;
		info.dwThreadID = (DWORD)-1;
		info.dwFlags = 0;

		__try
		{
			RaiseException(0x406D1388, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info);
		}
		__except (EXCEPTION_CONTINUE_EXECUTION)
		{
		}
	}

#if IS_COMPILER_MSVC
#pragma warning(pop)
#endif

	ThreadId
	GetCurrentThreadId()
	{
		return (ThreadId) ::GetCurrentThreadId();
	}

	void
	Sleep(
		uint64_t aTimeMillis)
	{
		if (aTimeMillis >= MAXDWORD)
		{
			while (true)
				::Sleep(INFINITE);
			return;
		}
		::Sleep((DWORD)aTimeMillis);
	}

}
}
