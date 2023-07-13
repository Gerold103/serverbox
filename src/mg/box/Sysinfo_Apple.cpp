#include "Sysinfo.h"

#include "mg/box/Assert.h"

#include <cstring>
#include <cerrno>
#include <sys/sysctl.h>

namespace mg {
namespace box {

	static uint32_t SysCalcCPUCoreCount();

	////////////////////////////////////////////////////////////////////////////

	uint32_t
	SysGetCPUCoreCount()
	{
		static uint32_t coreCount = SysCalcCPUCoreCount();
		return coreCount;
	}

	bool
	SysIsWSL()
	{
		return false;
	}

	////////////////////////////////////////////////////////////////////////////

	static uint32_t
	SysCalcCPUCoreCount()
	{
		int32_t val = 0;
		size_t valSize = sizeof(val);
		int rc = sysctlbyname("hw.ncpu", &val, &valSize, nullptr, 0);
		MG_BOX_ASSERT_F(rc == 0, "Couldn't get CPU core count: %d %s",
			errno, strerror(errno));
		MG_BOX_ASSERT_F(val > 0, "CPU core count should be > 0");
		return val;
	}

}
}
