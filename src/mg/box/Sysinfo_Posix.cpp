#include "Sysinfo.h"

#include "mg/box/Error.h"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#if IS_PLATFORM_APPLE
#include <sys/sysctl.h>
#else
#include <sys/sysinfo.h>
#endif

#define MG_SYSINFO_BUFFER_SIZE 2048

namespace mg {
namespace box {

	static bool SysCheckIsWSL();

	static uint32_t SysCalcCPUCoreCount();

	uint32_t
	SysGetCPUCoreCount()
	{
		static uint32_t coreCount = SysCalcCPUCoreCount();
		return coreCount;
	}

	bool
	SysIsWSL()
	{
		static bool isWSL = SysCheckIsWSL();
		return isWSL;
	}

	//////////////////////////////////////////////////////////////////////////////////////

	static bool
	SysCheckIsWSL()
	{
#if IS_PLATFORM_APPLE
		return false;
#else
		// WSL can't be detected at compile time because it runs the same binaries as real
		// Linux. But at runtime there are signs which tell this is WSL. In particular,
		// the OS version mentions Microsoft. It is available in 2 places, here both are
		// checked just in case.
		//
		// There are also no checks for WSL version: 1 or 2. Although they could be added
		// in the future by looking at
		// - /proc/mounts - presence of wslfs == WSL1;
		// - /proc/version might contain WSL2 string;
		// - /proc/cmdline is different on 1 and 2;
		// - some /proc/ subfolders are just absent on WSL1;
		// - /proc/version - has 'Microsoft' on WSL1, but 'microsoft' on WSL2.

		constexpr int bufSize = MG_SYSINFO_BUFFER_SIZE;
		ssize_t rc;
		char buf[bufSize];
		mg::box::Error::Ptr err;

		int fd = open("/proc/version", O_RDONLY, 0);
		if (fd < 0)
		{
			err = mg::box::ErrorRaiseErrno("open(/proc/version)");
			goto error;
		}
		rc = read(fd, buf, bufSize - 1);
		if (rc < 0)
		{
			err = mg::box::ErrorRaiseErrno("read(version)");
			goto error;
		}
		close(fd);
		fd = -1;
		buf[rc] = 0;
		if (strcasestr(buf, "microsoft") != nullptr)
			return true;

		fd = open("/proc/sys/kernel/osrelease", O_RDONLY, 0);
		if (fd < 0)
		{
			err = mg::box::ErrorRaiseErrno("open(/sys/kernel/osrelease)");
			goto error;
		}
		rc = read(fd, buf, bufSize - 1);
		if (rc < 0)
		{
			err = mg::box::ErrorRaiseErrno("read(osrelease)");
			goto error;
		}
		close(fd);
		fd = -1;
		buf[rc] = 0;
		return strcasestr(buf, "microsoft") != nullptr;

	error:
		if (fd >= 0)
			close(fd);
		MG_BOX_ASSERT_F(false, "failed to check WSL: %s", err->myMessage.c_str());
		return false;
#endif
	}

	static uint32_t
	SysCalcCPUCoreCount()
	{
#if IS_PLATFORM_APPLE
		int32_t val = 0;
		size_t valSize = sizeof(val);
		int rc = sysctlbyname("hw.ncpu", &val, &valSize, nullptr, 0);
		MG_BOX_ASSERT_F(rc == 0, "Couldn't get CPU core count: %d %s",
			errno, strerror(errno));
		MG_BOX_ASSERT_F(val > 0, "CPU core count should be > 0");
		return val;
#else
		return get_nprocs();
#endif
	}

}
}
