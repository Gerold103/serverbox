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

	//////////////////////////////////////////////////////////////////////////////////////

	void
	BufferStream::EnsureWriteSize(
		uint64_t aSize)
	{
		if (myWSize >= aSize)
			return;
		if (myWPos == nullptr)
		{
			if (!myHead.IsSet())
			{
				MG_DEV_ASSERT(myRSize == 0);
				MG_DEV_ASSERT(myWPos == nullptr);
				MG_DEV_ASSERT(myWSize == 0);
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
		MG_DEV_ASSERT(aByteOffset < aHead->myPos);
		buf->myData = aHead->myWData + aByteOffset;
		buf->mySize = aHead->myPos - aByteOffset;
		++buf;
		aHead = aHead->myNext.GetPointer();
		while (buf < bufEnd && aHead != nullptr)
		{
			MG_DEV_ASSERT(aHead->myPos > 0);
			buf->myData = aHead->myWData;
			buf->mySize = aHead->myPos;
			++buf;
			aHead = aHead->myNext.GetPointer();
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
		uint32_t rc = BuffersToIOVecsForWrite(aHead->myHead, aByteOffset,
			aVectors, aVectorCount);
		uint32_t res = rc;
		while (true)
		{
			MG_DEV_ASSERT(rc <= aVectorCount);
			aVectorCount -= rc;
			if (aVectorCount == 0)
				return res;
			aVectors += rc;

			aHead = aHead->myNext;
			if (aHead == nullptr)
				return res;
			rc = BuffersToIOVecsForWrite(aHead->myHead, 0, aVectors, aVectorCount);
			res += rc;

		}
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
