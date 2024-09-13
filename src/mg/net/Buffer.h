#pragma once

#include "mg/box/ForwardList.h"
#include "mg/box/SharedPtr.h"
#include "mg/box/ThreadLocalPool.h"

F_DECLARE_STRUCT(mg, box, IOVec)

namespace mg {
namespace net {

	//
	// A set of buffer classes to represent data for network sending and receiving.
	//
	// Note the used terminology:
	//
	// - When API uses 'Ref' or 'Raw' (AppendRef(), SendRef(), BufferRaw, etc) - it means
	//     the original data is used. No copying occurs. Hence it is caller's
	//     responsibility to ensure the data stays valid as long as needed.
	//
	// - When API uses 'Copy' (AppendCopy(), SendCopy(), BufferCopy, etc) - it means the
	//     original data can be freely discarded or changed or reused after the call.
	//     Everything was copied.
	//
	// - When API uses 'Move' - it means the full ownership of the data is taken. The
	//     object should not be touched after the move.
	//

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
		void Propagate(
			uint32_t aSize);

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

#if IS_COMPILER_MSVC
#pragma warning(push)
	// "Variable '...' is uninitialized.Always initialize a member variable(type.6)".
#pragma warning(disable: 26495)
#endif

	static constexpr uint64_t theBufferCopySize = 1024 - sizeof(Buffer);

	// Data should be copied in there. Use when the original data is going to be changed
	// or deleted before it is sent to network. Otherwise can be used for receiving new
	// data.
	//
	class BufferCopy
		: public Buffer
		, public mg::box::ThreadPooled<BufferCopy>
	{
		SHARED_PTR_RENEW_API(BufferCopy)

	private:
		BufferCopy() : Buffer(myCopyData, 0, theBufferCopySize) {}
		~BufferCopy() override = default;

	public:
		uint8_t myCopyData[theBufferCopySize];
	};

	static_assert(sizeof(BufferCopy) == 1024, "BufferCopy should be 1KB");

#if IS_COMPILER_MSVC
#pragma warning(pop)
#endif

	//////////////////////////////////////////////////////////////////////////////////////

	// Data is not copied anyhow. It should live at least as long as the buffer object. It
	// is also typically expected not to change while the buffer exists.
	//
	class BufferRaw
		: public Buffer
	{
	public:
		SHARED_PTR_RENEW_API(BufferRaw)

	private:
		BufferRaw() : Buffer() {}
		BufferRaw(
			const void* aData,
			uint32_t aPos,
			uint32_t aCapacity) : Buffer(aData, aPos, aCapacity) {}
		BufferRaw(
			const void* aData,
			uint32_t aPos) : BufferRaw(aData, aPos, aPos) {}
		~BufferRaw() override = default;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	// A list of buffers, which can be stored in a list of such lists. This is handy when
	// need to carry a sequence of buffers but can not change any of them at all. Can only
	// reference them.
	//
	// Particularly useful when want to broadcast same buffers into multiple places. For
	// example, to send into multiple sockets. It will be safe, because the sockets can
	// send the same buffers in parallel without changing them at all and hence not
	// affecting each other.
	//
	class BufferLink
	{
	public:
		BufferLink() : myNext(nullptr) {}
		BufferLink(
			const Buffer* aHead) : myHead((Buffer*)aHead), myNext(nullptr) {}
		BufferLink(
			const Buffer::Ptr& aHead) : myHead(aHead), myNext(nullptr) {}
		BufferLink(
			Buffer::Ptr&& aHead) : myHead(std::move(aHead)), myNext(nullptr) {}

		Buffer::Ptr myHead;
		BufferLink* myNext;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	class BufferLinkList
	{
	public:
		BufferLinkList() = default;
		BufferLinkList(
			const BufferLinkList&) = delete;
		BufferLinkList(
			BufferLinkList&&) = delete;
		~BufferLinkList() { Clear(); }

		void AppendRef(
			const Buffer* aHead);
		void AppendRef(
			const Buffer::Ptr& aHead) { AppendRef(aHead.GetPointer()); }
		void AppendRef(
			Buffer::Ptr&& aHead);
		void AppendRef(
			const void* aData,
			uint64_t aSize);
		void AppendMove(
			BufferLink* aHead);
		void Append(
			BufferLinkList&& aList) { myLinks.Append(std::move(aList.myLinks)); }
		void AppendCopy(
			const void* aData,
			uint64_t aSize);
		void SkipData(
			uint32_t& aByteOffset,
			uint64_t aByteCount);
		void SkipEmptyPrefix(
			uint32_t& aByteOffset);

		void Clear() { while (!myLinks.IsEmpty()) delete myLinks.PopFirst(); }

		const BufferLink* GetFirst() const { return myLinks.GetFirst(); }
		BufferLink* GetFirst() { return myLinks.GetFirst(); }
		BufferLink* PopFirst() { return myLinks.PopFirst(); }

		bool IsEmpty() const { return myLinks.IsEmpty(); }

	private:
		using List = mg::box::ForwardList<BufferLink>;

		List myLinks;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	class BufferStream
	{
	public:
		BufferStream();
		BufferStream(
			const BufferStream&) = delete;

		Buffer* GetWritePos() { return myWPos; }
		void PropagateWritePos(
			uint64_t aSize);
		const Buffer* GetReadPos() const { return myHead.GetPointer(); }

		uint64_t GetReadSize() const { return myRSize; }
		uint64_t GetWriteSize() const { return myWSize; }

		void EnsureWriteSize(
			uint64_t aSize);
		void WriteCopy(
			const void* aData,
			uint64_t aSize);
		void WriteRef(
			Buffer::Ptr&& aHead);

		void SkipData(
			uint64_t aSize);
		void ReadData(
			void* aData,
			uint64_t aSize);
		void PeekData(
			void* aData,
			uint64_t aSize) const;
		Buffer::Ptr PopData();

		void Clear();

		bool IsEmpty() const { return !myHead.IsSet(); }

	private:
		void PrivCheck() const;

		// head -> rend -> wpos -> tail
		// \__________/   \___________/
		//    rsize           wsize
		Buffer::Ptr myHead;
		uint64_t myRSize;
		// Last readable buffer. Having pos > 0. Remembering this position allows to pluck
		// the readable prefix from the stream in O(1) by simply making rend->next the new
		// head of the stream.
		Buffer* myREnd;
		// First writable buffer. Having pos < cap.
		Buffer* myWPos;
		uint64_t myWSize;
		Buffer* myTail;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	class BufferReadStream
	{
	public:
		BufferReadStream(
			BufferStream& aStream) : myStream(&aStream) {}

		const Buffer* GetReadPos() const { return myStream->GetReadPos(); }
		uint64_t GetReadSize() const { return myStream->GetReadSize(); }

		void SkipData(
			uint64_t aSize) { myStream->SkipData(aSize); }
		void ReadData(
			void* aData,
			uint64_t aSize) { myStream->ReadData(aData, aSize); }
		void PeekData(
			void* aData,
			uint64_t aSize) { myStream->PeekData(aData, aSize); }

		bool IsEmpty() const { return myStream->IsEmpty(); }

	private:
		BufferStream* myStream;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	Buffer::Ptr BuffersCopy(
		const void* aData,
		uint64_t aSize);

	Buffer::Ptr BuffersCopy(
		const Buffer* aHead);

	static inline Buffer::Ptr
	BuffersCopy(
		const Buffer::Ptr& aHead) { return BuffersCopy(aHead.GetPointer()); }

	Buffer::Ptr BuffersCopy(
		const BufferLink* aHead);

	Buffer::Ptr BuffersRef(
		const void* aData,
		uint64_t aSize);

	// Grow positions in the buffers up to their capacities to increase the total size by
	// the given value.
	//
	// If this was a single big buffer, it would be like that:
	//
	//   pos += size
	//
	Buffer* BuffersPropagateOnWrite(
		Buffer* aHead,
		uint64_t aSize);

	// Shift the positions up to their capacities to decrease the total size by the given
	// value.
	//
	// If this was a single big buffer, it would be like that:
	//
	//   data += size
	//   pos -= size
	//   capacity -= size
	//
	void BuffersPropagateOnRead(
		Buffer::Ptr& aHead,
		uint64_t aSize);

	// If this was a single big buffer, it would be like that:
	//
	//   iovec.data = data
	//   iovec.size = pos
	//
	uint32_t BuffersToIOVecsForWrite(
		const Buffer* aHead,
		uint32_t aByteOffset,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount);

	static inline uint32_t
	BuffersToIOVecsForWrite(
		const Buffer::Ptr& aHead,
		uint32_t aByteOffset,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount)
	{
		return BuffersToIOVecsForWrite(aHead.GetPointer(), aByteOffset, aVectors,
			aVectorCount);
	}

	uint32_t BuffersToIOVecsForWrite(
		const mg::net::BufferLink* aHead,
		uint32_t aByteOffset,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount);

	static inline uint32_t
	BuffersToIOVecsForWrite(
		const mg::net::BufferLinkList& aList,
		uint32_t aByteOffset,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount)
	{
		return BuffersToIOVecsForWrite(aList.GetFirst(), aByteOffset, aVectors,
			aVectorCount);
	}

	// If this was a single big buffer, it would be like that:
	//
	//   iovec.data = data + pos
	//   iovec.size = capacity - pos
	//
	uint32_t BuffersToIOVecsForRead(
		Buffer* aHead,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount);

	static inline uint32_t
	BuffersToIOVecsForRead(
		Buffer::Ptr& aHead,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount)
	{
		return BuffersToIOVecsForRead(aHead.GetPointer(), aVectors, aVectorCount);
	}

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
		MG_DEV_ASSERT(aCapacity >= aPos);
	}

	inline void
	Buffer::Propagate(
		uint32_t aSize)
	{
		MG_DEV_ASSERT(myPos >= aSize);
		MG_DEV_ASSERT(myPos <= myCapacity);
		myPos -= aSize;
		myCapacity -= aSize;
		myRData += aSize;
		MG_DEV_ASSERT(myWData == myRData);
	}

	//////////////////////////////////////////////////////////////////////////////////////

	inline void
	BufferLinkList::AppendRef(
		const Buffer* aHead)
	{
		if (aHead != nullptr)
			AppendMove(new BufferLink(aHead));
	}

	inline void
	BufferLinkList::AppendRef(
		Buffer::Ptr&& aHead)
	{
		if (aHead.GetPointer() != nullptr)
			AppendMove(new BufferLink(std::move(aHead)));
	}

	inline void
	BufferLinkList::AppendRef(
		const void* aData,
		uint64_t aSize)
	{
		AppendRef(BuffersRef(aData, aSize));
	}

	inline void
	BufferLinkList::AppendMove(
		BufferLink* aHead)
	{
		if (aHead == nullptr)
			return;
		BufferLink* last = aHead;
		while (last->myNext != nullptr)
			last = last->myNext;
		myLinks.Append(aHead, last);
	}

	inline void
	BufferLinkList::AppendCopy(
		const void* aData,
		uint64_t aSize)
	{
		AppendRef(BuffersCopy(aData, aSize));
	}

	//////////////////////////////////////////////////////////////////////////////////////

}
}
