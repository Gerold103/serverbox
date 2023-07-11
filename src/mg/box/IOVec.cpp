#include "IOVec.h"

#include "mg/box/Assert.h"

#include <type_traits>

namespace mg {
namespace box {

	static_assert(sizeof(IOVec) == sizeof(IOVecNative),
		"IoVec matches native vec");

#if IS_PLATFORM_WIN
	static_assert(offsetof(IOVec, mySize) == offsetof(WSABUF, len),
		"IOVec::mySize offset");
	static_assert(offsetof(IOVec, myData) == offsetof(WSABUF, buf),
		"IOVec::myData offset");

	static_assert(std::is_same<
		decltype(((IOVec*)nullptr)->mySize),
		decltype(((WSABUF*)nullptr)->len)>::value,
		"IOVec::mySize type");

	static_assert(std::is_same<
		decltype(((IOVec*)nullptr)->myData),
		decltype(((WSABUF*)nullptr)->buf)>::value,
		"IOVec::myData type");
#else
	static_assert(offsetof(IOVec, mySize) == offsetof(iovec, iov_len),
		"IOVec::mySize offset");
	static_assert(offsetof(IOVec, myData) == offsetof(iovec, iov_base),
		"IOVec::myData offset");

	static_assert(std::is_same<
		decltype(((IOVec*)nullptr)->mySize),
		decltype(((iovec*)nullptr)->iov_len)>::value,
		"IOVec::mySize type");

	static_assert(std::is_same<
		decltype(((IOVec*)nullptr)->myData),
		decltype(((iovec*)nullptr)->iov_base)>::value,
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
