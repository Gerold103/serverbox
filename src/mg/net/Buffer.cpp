#include "Buffer.h"

#include "mg/box/IOVec.h"

namespace mg {
namespace net {

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

	BufferCopy::Ptr
	BufferCopy::MakeChain(
		const void* aData,
		uint64_t aSize)
	{
		if (aSize == 0)
			return {};
		BufferCopy::Ptr head = BufferCopy::NewShared();
		Buffer* pos = head.GetPointer();
		while (true)
		{
			uint32_t cap;
			if (aSize > theBufferCopySize)
				cap = theBufferCopySize;
			else
				cap = (uint32_t)aSize;
			pos->myPos = cap;
			memcpy(pos->myWData, aData, cap);
			aSize -= cap;
			if (aSize == 0)
				return head;
			aData = (uint8_t*)aData + cap;
			pos = (pos->myNext = BufferCopy::NewShared()).GetPointer();
		}

	}

	void
	BufferLinkList::SkipBytes(
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
					aByteOffset = aByteCount;
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

	uint32_t
	BuffersToIOVecsForWrite(
		const Buffer* aHead,
		uint32_t aByteOffset,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount)
	{
		mg::box::IOVec* buf = aVectors;
		mg::box::IOVec* bufEnd = aVectors + aVectorCount;
		MG_DEV_ASSERT(aByteOffset <= aHead->myPos);
		buf->myData = aHead->myWData + aByteOffset;
		buf->mySize = aHead->myPos - aByteOffset;
		++buf;
		aHead = aHead->myNext.GetPointer();
		while (buf < bufEnd && aHead != nullptr)
		{
			buf->myData = aHead->myWData;
			buf->mySize = aHead->myPos;
			++buf;
			aHead = aHead->myNext.GetPointer();
		}
		return (uint32_t)(buf - aVectors);
	}

	uint32_t
	BuffersToIOVecsForWrite(
		const mg::net::BufferLinkList& aList,
		uint32_t aByteOffset,
		mg::box::IOVec* aVectors,
		uint32_t aVectorCount)
	{
		const mg::net::BufferLink* link = aList.GetFirst();
		if (link == nullptr)
			return 0;

		uint32_t res = BuffersToIOVecsForWrite(link->myHead.GetPointer(), aByteOffset,
			aVectors, aVectorCount);
		while (true)
		{
			MG_DEV_ASSERT(res <= aVectorCount);
			aVectorCount -= res;
			if (aVectorCount == 0)
				return res;
			aVectors += res;

			link = link->myNext;
			if (link == nullptr)
				return res;
			res += BuffersToIOVecsForWrite(link->myHead.GetPointer(), 0,
				aVectors, aVectorCount);
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
		MG_DEV_ASSERT(aHead->myCapacity >= aHead->myPos);
		buf->mySize = aHead->myCapacity - aHead->myPos;
		++buf;
		aHead = aHead->myNext.GetPointer();
		while (aHead != nullptr && buf < bufEnd)
		{
			MG_DEV_ASSERT(aHead->myPos == 0);
			buf->myData = aHead->myWData;
			buf->mySize = aHead->myCapacity;
			++buf;
			aHead = aHead->myNext.GetPointer();
		}
		return (uint32_t)(buf - aVectors);
	}

}
}
