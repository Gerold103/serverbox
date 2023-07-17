#pragma once

#include "mg/box/ForwardList.h"
#include "mg/box/SharedPtr.h"
#include "mg/box/ThreadLocalPool.h"

F_DECLARE_STRUCT(mg, box, IOVec);

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

		static BufferCopy::Ptr MakeChain(
			const void* aData,
			uint64_t aSize);

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

	class BufferLink
	{
	public:
		BufferLink();
		BufferLink(
			const Buffer* aHead);
		BufferLink(
			const Buffer::Ptr& aHead);
		BufferLink(
			Buffer::Ptr&& aHead);

		Buffer::Ptr myHead;
		BufferLink* myNext;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	class BufferLinkList
	{
	public:
		void AppendRef(
			const Buffer* aHead);

		void AppendRef(
			const void* aData,
			uint64_t aSize);

		void AppendCopy(
			const void* aData,
			uint64_t aSize);

		void DiscardBytes(
			uint32_t& aByteOffset,
			uint64_t aByteCount);

		const BufferLink* GetFirst() const { return myLinks.GetFirst(); }
		BufferLink* GetFirst() { return myLinks.GetFirst(); }
		BufferLink* PopFirst() { return myLinks.PopFirst(); }

		bool IsEmpty() const { return myLinks.IsEmpty(); }

	private:
		using List = mg::box::ForwardList<BufferLink>;

		List myLinks;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	uint32_t BuffersToIOVecsForWrite(
		const Buffer* aHead,
		uint32_t aByteOffset,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount);

	uint32_t BuffersToIOVecsForWrite(
		const mg::net::BufferLinkList& aList,
		uint32_t aByteOffset,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount);

	uint32_t BuffersToIOVecsForRead(
		Buffer* aHead,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount);

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

	inline
	BufferLink::BufferLink()
		: myNext(nullptr)
	{
	}

	inline
	BufferLink::BufferLink(
		const Buffer* aHead)
		: myHead((Buffer*)aHead)
		, myNext(nullptr)
	{
	}

	inline
	BufferLink::BufferLink(
		const Buffer::Ptr& aHead)
		: myHead(aHead)
		, myNext(nullptr)
	{
	}

	inline
	BufferLink::BufferLink(
		Buffer::Ptr&& aHead)
		: myHead(std::move(aHead))
		, myNext(nullptr)
	{
	}

	inline void
	BufferLinkList::AppendRef(
		const Buffer* aHead)
	{
		myLinks.Append(new BufferLink(aHead));
	}

	inline void
	BufferLinkList::AppendRef(
		const void* aData,
		uint64_t aSize)
	{
		myLinks.Append(new BufferLink(
			BufferRaw::NewShared(aData, aSize, 0)));
	}

	inline void
	BufferLinkList::AppendCopy(
		const void* aData,
		uint64_t aSize)
	{
		myLinks.Append(new BufferLink(
			BufferCopy::MakeChain(aData, aSize)));
	}

}
}
