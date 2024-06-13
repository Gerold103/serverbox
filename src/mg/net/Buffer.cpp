#include "Buffer.h"

#include "mg/box/IOVec.h"

namespace mg {
namespace net {

	static Buffer* BuffersCopyAppend(
		Buffer* aHead,
		const void* aData,
		uint64_t aSize);

	//////////////////////////////////////////////////////////////////////////////////////

	Buffer::~Buffer()
	{
		// Avoid recursion for a long buffer list.
		Buffer* curr = myNext.Unwrap();
		while (curr != nullptr)
		{
			if (!curr->myRef.Dec())
				return;
			Buffer* next = curr->myNext.Unwrap();
			delete curr;
			curr = next;
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////

	void
	BufferLinkList::SkipData(
		uint32_t& aByteOffset,
		uint64_t aByteCount)
	{
		aByteCount += aByteOffset;
		while (aByteCount != 0)
		{
			BufferLink* link = myLinks.GetFirst();
			Buffer* pos = link->myHead.GetPointer();
			while (pos != nullptr)
			{
				if (pos->myPos > aByteCount)
				{
					link->myHead.Set(pos);
					aByteOffset = (uint32_t)aByteCount;
					return;
				}
				aByteCount -= pos->myPos;
				// Do not alter the buffer members. The buffers can be shared by multiple
				// links. Alteration here would break the other links.
				pos = pos->myNext.GetPointer();
			}
			delete myLinks.PopFirst();
		}
		aByteOffset = 0;
	}

	void
	BufferLinkList::SkipEmptyPrefix(
		uint32_t& aByteOffset)
	{
		BufferLink* link = myLinks.GetFirst();
		// Fast path - either fully empty or has data right in the beginning.
		if (link == nullptr)
		{
			MG_DEV_ASSERT(aByteOffset == 0);
			return;
		}
		if (link->myHead.GetPointer() && link->myHead->myPos > aByteOffset)
			return;
		// Slow path - doesn't happen normally but might happen.
		do
		{
			Buffer::Ptr& head = link->myHead;
			while (head.GetPointer() != nullptr)
			{
				if (head->myPos > 0)
				{
					if (aByteOffset < head->myPos)
						return;
					MG_DEV_ASSERT(aByteOffset == head->myPos);
					aByteOffset = 0;
				}
				head = std::move(head->myNext);
			}
			link = link->myNext;
			delete myLinks.PopFirst();
		} while (link != nullptr);
	}

	//////////////////////////////////////////////////////////////////////////////////////

	BufferStream::BufferStream()
		: myRSize(0)
		, myREnd(nullptr)
		, myWPos(nullptr)
		, myWSize(0)
		, myTail(nullptr)
	{
	}

	void
	BufferStream::EnsureWriteSize(
		uint64_t aSize)
	{
		if (myWSize >= aSize)
			return;
		if (myWPos == nullptr)
		{
			MG_DEV_ASSERT(myWSize == 0);
			if (!myHead.IsSet())
			{
				MG_DEV_ASSERT(myRSize == 0);
				MG_DEV_ASSERT(myTail == nullptr);
				myTail = (myHead = BufferCopy::NewShared()).GetPointer();
			}
			else
			{
				MG_DEV_ASSERT(myTail->myPos == myTail->myCapacity);
				myTail = (myTail->myNext = BufferCopy::NewShared()).GetPointer();
			}
			myWPos = myTail;
			myWSize = theBufferCopySize;
			if (myWSize >= aSize)
				return;
		}
		do {
			myTail = (myTail->myNext = BufferCopy::NewShared()).GetPointer();
			myWSize += theBufferCopySize;
		} while (myWSize < aSize);
		MG_DEV_ASSERT(myWPos->myPos < myWPos->myCapacity);
	}

	void
	BufferStream::PropagateWritePos(
		uint64_t aSize)
	{
		MG_DEV_ASSERT(myWSize >= aSize);
		myWPos = BuffersPropagateOnWrite(myWPos, aSize);
		myWSize -= aSize;
		myRSize += aSize;
		myREnd = myWPos;
		if (myWSize == 0)
		{
			MG_DEV_ASSERT(myWPos == nullptr || myWPos->myPos == myWPos->myCapacity);
			myWPos = nullptr;
		} else if (myWPos->myPos == myWPos->myCapacity) {
			myWPos = myWPos->myNext.GetPointer();
			MG_DEV_ASSERT(myWPos->myPos < myWPos->myCapacity);
		}
	}

	void
	BufferStream::WriteCopy(
		const void* aData,
		uint64_t aSize)
	{
		if (aSize == 0)
			return;
		if (myWPos == nullptr)
		{
			MG_DEV_ASSERT(myWSize == 0);
			if (!myHead.IsSet())
			{
				MG_DEV_ASSERT(myRSize == 0);
				MG_DEV_ASSERT(myREnd == nullptr);
				MG_DEV_ASSERT(myTail == nullptr);
				myTail = (myHead = BufferCopy::NewShared()).GetPointer();
			}
			else
			{
				MG_DEV_ASSERT(myTail->myPos == myTail->myCapacity);
				myTail = (myTail->myNext = BufferCopy::NewShared()).GetPointer();
			}
			myWPos = myTail;
			myWSize = theBufferCopySize;
		}
		myRSize += aSize;
		while (true)
		{
			MG_DEV_ASSERT(myWPos->myPos < myWPos->myCapacity);
			uint32_t cap = myWPos->myCapacity - myWPos->myPos;
			if (cap > aSize)
			{
				memcpy(myWPos->myWData + myWPos->myPos, aData, aSize);
				myWPos->myPos += (uint32_t)aSize;
				myREnd = myWPos;
				myWSize -= aSize;
				return;
			}
			memcpy(myWPos->myWData + myWPos->myPos, aData, cap);
			myWPos->myPos = myWPos->myCapacity;
			if (cap == aSize)
			{
				myREnd = myWPos;
				myWPos = nullptr;
				myWSize = 0;
				return;
			}
			aData = (const uint8_t*)aData + cap;
			aSize -= cap;
			MG_DEV_ASSERT(myHead.IsSet());
			MG_DEV_ASSERT(myTail->myPos == myTail->myCapacity);
			myTail = (myTail->myNext = BufferCopy::NewShared()).GetPointer();
			myWPos = myTail;
			myWSize = theBufferCopySize;
		}
	}

	void
	BufferStream::WriteRef(
		Buffer::Ptr&& aHead)
	{
		if (!aHead.IsSet())
			return;
		uint64_t size = 0;
		Buffer* end = aHead.GetPointer();
		size += end->myPos;
		while (end->myNext.IsSet())
		{
			end = end->myNext.GetPointer();
			size += end->myPos;
		}
		if (myWPos == nullptr)
		{
			MG_DEV_ASSERT(myWSize == 0);
			if (!myHead.IsSet())
			{
				MG_DEV_ASSERT(myRSize == 0);
				MG_DEV_ASSERT(myREnd == nullptr);
				MG_DEV_ASSERT(myTail == nullptr);
				myHead = std::move(aHead);
			}
			else
			{
				MG_DEV_ASSERT(myTail->myPos == myTail->myCapacity);
				myTail->myNext = std::move(aHead);
			}
			myTail = end;
			myREnd = end;
			myRSize += size;
			myWSize = myTail->myCapacity - myTail->myPos;
			if (myWSize > 0)
				myWPos = myTail;
			return;
		}
		MG_DEV_ASSERT(myWSize > 0);
		MG_DEV_ASSERT(myWPos->myPos < myWPos->myCapacity);
		if (myREnd == nullptr)
		{
			// There are empty buffers, no data to read. Happens after reservation on an
			// empty stream and after popping all data from a stream having at least one
			// empty buffer reserved in the end.
			MG_DEV_ASSERT(myRSize == 0);
			MG_DEV_ASSERT(myWPos == myHead.GetPointer());
			Buffer::Ptr oldHead = std::move(myHead);
			myHead = std::move(aHead);
			myREnd = end;
			myRSize += size;
			myWSize += end->myCapacity - end->myPos;
			if (end->myCapacity > end->myPos)
				myWPos = end;
			end->myNext = std::move(oldHead);
			return;
		}
		MG_DEV_ASSERT(myREnd == myWPos || myREnd->myNext == myWPos);
		if (myWPos->myPos > 0)
		{
			// Last writable buffer has data. Need to place the new data after it, to keep
			// the order correct.
			end->myNext = std::move(myWPos->myNext);
			myWPos->myNext = std::move(aHead);
		}
		else
		{
			// Last writable buffer has no data. Place the new data before it. This way
			// the full writable buffer remains in place for next writes and doesn't
			// become a hole in the stream between the old and new data.
			end->myNext = std::move(myREnd->myNext);
			myREnd->myNext = std::move(aHead);
		}
		myREnd = end;
		myRSize += size;

		myWSize -= myWPos->myCapacity - myWPos->myPos;
		myWPos = end;
		myWSize += myWPos->myCapacity - myWPos->myPos;
		if (myWPos->myPos == myWPos->myCapacity)
		{
			myWPos = myWPos->myNext.GetPointer();
		}
	}

	void
	BufferStream::SkipData(
		uint64_t aSize)
	{
		MG_DEV_ASSERT(myRSize >= aSize);
		BuffersPropagateOnRead(myHead, aSize);
		myRSize -= aSize;
		if (!myHead.IsSet())
			Clear();
	}

	void
	BufferStream::ReadData(
		void* aData,
		uint64_t aSize)
	{
		if (aSize == 0)
			return;
		MG_DEV_ASSERT(myRSize >= aSize);
		myRSize -= aSize;
		while (true)
		{
			uint64_t cap = myHead->myPos;
			uint64_t toRead = cap > aSize ? aSize : cap;
			memcpy(aData, myHead->myRData, toRead);
			aSize -= toRead;
			if (aSize == 0)
			{
				myHead->Propagate((uint32_t)toRead);
				if (myHead->myPos == 0 && myHead->myCapacity == 0)
				{
					myHead = std::move(myHead->myNext);
					if (!myHead.IsSet())
					{
						MG_DEV_ASSERT(myWSize == 0);
						Clear();
					}
				}
				MG_DEV_ASSERT(!myHead.IsSet() || myRSize > 0 || myWSize > 0);
				return;
			}
			aData = (uint8_t*)aData + toRead;
			myHead = std::move(myHead->myNext);
		}
	}

	void
	BufferStream::PeekData(
		void* aData,
		uint64_t aSize) const
	{
		if (aSize == 0)
			return;
		MG_DEV_ASSERT(myRSize >= aSize);
		const Buffer* pos = myHead.GetPointer();
		while (true)
		{
			uint64_t cap = pos->myPos;
			uint64_t toRead = cap >= aSize ? aSize : cap;
			memcpy(aData, pos->myRData, toRead);
			aSize -= toRead;
			if (aSize == 0)
				return;
			aData = (uint8_t*)aData + toRead;
			pos = pos->myNext.GetPointer();
		}
	}

	Buffer::Ptr
	BufferStream::PopData()
	{
		if (myREnd == nullptr)
		{
			MG_DEV_ASSERT(myRSize == 0);
			return {};
		}
		MG_DEV_ASSERT(myRSize > 0);
		MG_DEV_ASSERT(myREnd->myPos > 0);
		Buffer::Ptr res = std::move(myHead);
		myHead = std::move(myREnd->myNext);
		if (!myHead.IsSet())
		{
			Clear();
			return res;
		}
		myWSize -= myREnd->myCapacity - myREnd->myPos;
		myRSize = 0;
		myREnd = nullptr;
		myWPos = myHead.GetPointer();
		return res;
	}

	void
	BufferStream::Clear()
	{
		myHead.Clear();
		myRSize = 0;
		myREnd = nullptr;
		myWPos = nullptr;
		myWSize = 0;
		myTail = nullptr;
	}

	//////////////////////////////////////////////////////////////////////////////////////

	Buffer::Ptr
	BuffersCopy(
		const void* aData,
		uint64_t aSize)
	{
		if (aSize == 0)
			return {};
		Buffer::Ptr res = BufferCopy::NewShared();
		BuffersCopyAppend(res.GetPointer(), aData, aSize);
		return res;
	}

	Buffer::Ptr
	BuffersCopy(
		const Buffer* aHead)
	{
		if (aHead == nullptr)
			return {};
		while (aHead->myPos == 0)
		{
			aHead = aHead->myNext.GetPointer();
			if (aHead == nullptr)
				return {};
		}
		Buffer::Ptr res = BufferCopy::NewShared();
		Buffer* tail = res.GetPointer();
		do {
			tail = BuffersCopyAppend(tail, aHead->myWData, aHead->myPos);
			aHead = aHead->myNext.GetPointer();
		} while (aHead != nullptr);
		return res;
	}

	Buffer::Ptr
	BuffersCopy(
		const BufferLink* aHead)
	{
		const Buffer* buf = nullptr;
		while (aHead != nullptr)
		{
			buf = aHead->myHead.GetPointer();
			while (buf != nullptr)
			{
				if (buf->myPos > 0)
					goto start_copy;
				buf = buf->myNext.GetPointer();
			}
			aHead = aHead->myNext;
		}
		return {};

	start_copy:;
		Buffer::Ptr res = BufferCopy::NewShared();
		Buffer* tail = res.GetPointer();
		while (true)
		{
			while (buf != nullptr)
			{
				tail = BuffersCopyAppend(tail, buf->myWData, buf->myPos);
				buf = buf->myNext.GetPointer();
			}
			aHead = aHead->myNext;
			if (aHead == nullptr)
				break;
			buf = aHead->myHead.GetPointer();
		}
		return res;
	}

	Buffer::Ptr
	BuffersRef(
		const void* aData,
		uint64_t aSize)
	{
		if (aSize <= UINT32_MAX)
		{
			if (aSize == 0)
				return {};
			return BufferRaw::NewShared(aData, (uint32_t)aSize);
		}
		Buffer::Ptr res = BufferRaw::NewShared(aData, UINT32_MAX);
		Buffer* tail = res.GetPointer();
		aSize -= UINT32_MAX;
		aData = (const uint8_t*)aData + UINT32_MAX;
		while (aSize > UINT32_MAX)
		{
			tail = (tail->myNext =
				BufferRaw::NewShared(aData, UINT32_MAX)).GetPointer();
			aSize -= UINT32_MAX;
			aData = (const uint8_t*)aData + UINT32_MAX;
		}
		MG_DEV_ASSERT(aSize > 0);
		tail->myNext = BufferRaw::NewShared(aData, (uint32_t)aSize);
		return res;
	}

	Buffer*
	BuffersPropagateOnWrite(
		Buffer* aHead,
		uint64_t aSize)
	{
		while (aSize > 0)
		{
			uint64_t cap = aHead->myCapacity - aHead->myPos;
			if (cap > aSize)
			{
				aHead->myPos += (uint32_t)aSize;
				return aHead;
			}
			aHead->myPos = aHead->myCapacity;
			if (cap == aSize)
				return aHead;
			aHead = aHead->myNext.GetPointer();
			aSize -= cap;
		}
		return aHead;
	}

	void
	BuffersPropagateOnRead(
		Buffer::Ptr& aHead,
		uint64_t aSize)
	{
		while (aSize > 0)
		{
			if (aHead->myPos >= aSize)
			{
				aHead->Propagate((uint32_t)aSize);
				if (aHead->myPos == 0 && aHead->myCapacity == 0)
					aHead = std::move(aHead->myNext);
				return;
			}
			aSize -= aHead->myPos;
			aHead = std::move(aHead->myNext);
		}
	}

	uint32_t
	BuffersToIOVecsForWrite(
		const Buffer* aHead,
		uint32_t aByteOffset,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount)
	{
		mg::box::IOVec* buf = aVectors;
		mg::box::IOVec* bufEnd = aVectors + aVectorCount;
		for (; aHead != nullptr && buf < bufEnd; aHead = aHead->myNext.GetPointer())
		{
			if (aHead->myPos == 0)
				continue;
			MG_DEV_ASSERT(aByteOffset < aHead->myPos);
			buf->myData = aHead->myWData + aByteOffset;
			buf->mySize = aHead->myPos - aByteOffset;
			++buf;
			aByteOffset = 0;
		}
		return (uint32_t)(buf - aVectors);
	}

	uint32_t
	BuffersToIOVecsForWrite(
		const BufferLink* aHead,
		uint32_t aByteOffset,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount)
	{
		mg::box::IOVec* buf = aVectors;
		mg::box::IOVec* bufEnd = aVectors + aVectorCount;
		while (aHead != nullptr)
		{
			const Buffer* it = aHead->myHead.GetPointer();
			for (; it != nullptr && buf < bufEnd; it = it->myNext.GetPointer())
			{
				if (it->myPos == 0)
					continue;
				MG_DEV_ASSERT(aByteOffset < it->myPos);
				buf->myData = it->myWData + aByteOffset;
				buf->mySize = it->myPos - aByteOffset;
				++buf;
				aByteOffset = 0;
			}
			if (buf == bufEnd)
				return aVectorCount;
			aHead = aHead->myNext;
		}
		return (uint32_t)(buf - aVectors);
	}

	uint32_t
	BuffersToIOVecsForRead(
		Buffer* aHead,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount)
	{
		mg::box::IOVec* buf = aVectors;
		mg::box::IOVec* bufEnd = aVectors + aVectorCount;
		buf->myData = aHead->myWData + aHead->myPos;
		MG_DEV_ASSERT(aHead->myCapacity > aHead->myPos);
		buf->mySize = aHead->myCapacity - aHead->myPos;
		++buf;
		aHead = aHead->myNext.GetPointer();
		while (aHead != nullptr && buf < bufEnd)
		{
			MG_DEV_ASSERT(aHead->myPos == 0);
			MG_DEV_ASSERT(aHead->myCapacity > 0);
			buf->myData = aHead->myWData;
			buf->mySize = aHead->myCapacity;
			++buf;
			aHead = aHead->myNext.GetPointer();
		}
		return (uint32_t)(buf - aVectors);
	}

	//////////////////////////////////////////////////////////////////////////////////////

	static Buffer*
	BuffersCopyAppend(
		Buffer* aHead,
		const void* aData,
		uint64_t aSize)
	{
		if (aSize == 0)
			return aHead;
		uint32_t cap = aHead->myCapacity - aHead->myPos;
		if (cap >= aSize)
		{
			memcpy(aHead->myWData + aHead->myPos, aData, aSize);
			aHead->myPos += (uint32_t)aSize;
			return aHead;
		}
		memcpy(aHead->myWData + aHead->myPos, aData, cap);
		aHead->myPos = aHead->myCapacity;
		aSize -= cap;
		do
		{
			aData = (const uint8_t*)aData + cap;
			aHead = (aHead->myNext = BufferCopy::NewShared()).GetPointer();
			if (aSize > theBufferCopySize)
				cap = theBufferCopySize;
			else
				cap = (uint32_t)aSize;
			aHead->myPos = cap;
			memcpy(aHead->myWData, aData, cap);
			aSize -= cap;
		} while (aSize > 0);
		return aHead;
	}

}
}
