#pragma once

#include "mg/box/SharedPtr.h"
#include "mg/box/ThreadLocalPool.h"

namespace mg {
namespace net {

	class BufferCopy;
	class BufferRaw;

	class Buffer
	{
	public:
		SHARED_PTR_TYPE(Buffer)

	private:
		Buffer();
		Buffer(
			const void* aData,
			uint32_t aPos,
			uint32_t aCapacity);
		virtual ~Buffer();

		void PrivRef() { myRef.Inc(); }
		void PrivUnref() { if (myRef.Dec()) delete this; }

	public:
		uint32_t myPos;
		uint32_t myCapacity;
		union
		{
			const uint8_t* myRData;
			uint8_t* myWData;
		};
		Buffer::Ptr myNext;
	private:
		mg::box::RefCount myRef;

		friend class BufferCopy;
		friend class BufferRaw;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	static constexpr uint64_t theBufferCopySize = 1024 - sizeof(Buffer);

	class BufferCopy
		: public Buffer
		, public mg::box::ThreadPooled<BufferCopy>
	{
	public: 
		SHARED_PTR_TYPE(BufferCopy)
		SHARED_PTR_NEW(BufferCopy)

	private:
		BufferCopy();
		~BufferCopy() override = default;

	public:
		uint8_t myCopyData[theBufferCopySize];
	};

	static_assert(sizeof(BufferCopy) == 1024, "BufferCopy should be 1KB");

	//////////////////////////////////////////////////////////////////////////////////////

	class BufferRaw
		: public Buffer
	{
	public: 
		SHARED_PTR_TYPE(BufferRaw)
		SHARED_PTR_NEW(BufferRaw)

	private:
		BufferRaw();
		BufferRaw(
			const void* aData,
			uint32_t aPos,
			uint32_t aCapacity);
		~BufferRaw() override = default;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	inline
	Buffer::Buffer()
		: myPos(0)
		, myCapacity(0)
		, myRData(nullptr)
	{
	}

	inline
	Buffer::Buffer(
		const void* aData,
		uint32_t aPos,
		uint32_t aCapacity)
		: myPos(aPos)
		, myCapacity(aCapacity)
		, myRData((const uint8_t*)aData)
	{
	}

	inline
	BufferCopy::BufferCopy()
		: Buffer(myCopyData, 0, theBufferCopySize)
	{
	}

	inline
	BufferRaw::BufferRaw()
		: Buffer()
	{
	}

	inline
	BufferRaw::BufferRaw(
		const void* aData,
		uint32_t aPos,
		uint32_t aCapacity)
		: Buffer(aData, aPos, aCapacity)
	{
	}

}
}
