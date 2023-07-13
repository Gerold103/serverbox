#include "Sysinfo.h"

namespace mg {
namespace box {

	uint32_t
	SysGetCPUCoreCount()
	{
		static uint32_t coreCount = []() {
			SYSTEM_INFO sysinfo;
			GetSystemInfo(&sysinfo);
			return sysinfo.dwNumberOfProcessors;
		}();
		return coreCount;
	}

	bool
	SysIsWSL()
	{
		return false;
	}

}
}
