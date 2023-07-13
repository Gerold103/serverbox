#include "mg/box/Sysinfo.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace box {

	void
	UnitTestSysinfo()
	{
		TestSuiteGuard suite("Sysinfo");

		// Just ensure it is not crashing.
		Report("Is WSL:     %d", (int)mg::box::SysIsWSL());
		Report("Core count: %u", mg::box::SysGetCPUCoreCount());
	}

}
}
}
