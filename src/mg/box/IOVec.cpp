#include "IOVec.h"

#include "mg/box/Assert.h"

#include <type_traits>

namespace mg {
namespace box {

	static_assert(sizeof(IOVec) == sizeof(IOVecNative),
		"IoVec matches native vec");

#if !IS_PLATFORM_WIN
	// Vectorized system calls can fail when try to send too many vectors. They are not
	// guaranteed to send only a part of data.
	static_assert(theIOVecMaxCount <= IOV_MAX, "Too big vector batch");
#endif

#if IS_PLATFORM_WIN
	static_assert(offsetof(IOVec, mySize) == offsetof(WSABUF, len),
		"IOVec::mySize offset");
	static_assert(offsetof(IOVec, myData) == offsetof(WSABUF, buf),
		"IOVec::myData offset");

	static_assert(std::is_same<
		decltype(IOVec::mySize), decltype(WSABUF::len)>::value,
		"IOVec::mySize type");

	// IOVec::myData is kept 'void*' to avoid explicit casts, but it is the same as
	// 'CHAR*' anyway.
	static_assert(std::is_same<CHAR*, decltype(WSABUF::buf)>::value,
		"IOVec::myData type");
#else
	static_assert(offsetof(IOVec, mySize) == offsetof(iovec, iov_len),
		"IOVec::mySize offset");
	static_assert(offsetof(IOVec, myData) == offsetof(iovec, iov_base),
		"IOVec::myData offset");

	static_assert(std::is_same<
		decltype(IOVec::mySize), decltype(iovec::iov_len)>::value,
		"IOVec::mySize type");

	static_assert(std::is_same<
		decltype(IOVec::myData), decltype(iovec::iov_base)>::value,
		"IOVec::myData type");
#endif

	void
	IOVecPropagate(
		IOVec*& aInOut,
		uint32_t& aInOutCount,
		uint32_t aByteOffset)
	{
		IOVec* cursor = aInOut;
		uint32_t count = aInOutCount;
		while (aByteOffset > 0)
		{
			if (aByteOffset < cursor->mySize)
			{
				cursor->mySize -= aByteOffset;
				cursor->myData = (uint8_t*)cursor->myData + aByteOffset;
				break;
			}
			MG_DEV_ASSERT(count > 0);
			aByteOffset -= cursor->mySize;
			cursor->myData = nullptr;
			cursor->mySize = 0;
			++cursor;
			--count;
		}
		aInOut = cursor;
		aInOutCount = count;
	}

}
}
