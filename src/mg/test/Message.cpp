#include "Message.h"

#include "mg/box/StringFunctions.h"

namespace mg {
namespace tst {

	ReadMessage::ReadMessage(
		mg::net::BufferReadStream& aStream)
		: myStream(&aStream)
		, myIsOpen(false)
	{
	}

	bool
	ReadMessage::IsComplete() const
	{
		MG_DEV_ASSERT(!myIsOpen);
		if (GetAvailSize() < sizeof(myHeader))
			return false;
		MessageHeader header;
		myStream->PeekData(&header, sizeof(header));
		return GetAvailSize() >= sizeof(myHeader) + header.myBodySize;
	}

	void
	ReadMessage::Open()
	{
		MG_DEV_ASSERT(!myIsOpen);
		MG_DEV_ASSERT(GetAvailSize() >= sizeof(myHeader));
		myStream->ReadData(&myHeader, sizeof(myHeader));
		MG_DEV_ASSERT(GetAvailSize() >= myHeader.myBodySize);
		myIsOpen = true;
	}

	void
	ReadMessage::Close()
	{
		MG_DEV_ASSERT(myIsOpen);
		myIsOpen = false;
		myHeader = MessageHeader();
	}

	void
	ReadMessage::ReadString(
		std::string& aOut)
	{
		uint64_t len;
		ReadUInt64(len);
		aOut.resize(0);
		aOut.reserve(len);
		constexpr uint64_t tmpSize = 256;
		char tmp[tmpSize];
		while (len > 0)
		{
			uint64_t toRead = tmpSize >= len ? len : tmpSize;
			ReadData(tmp, toRead);
			aOut.append(tmp, toRead);
			len -= toRead;
		}
	}

	void
	ReadMessage::ReadData(
		void* aData,
		uint64_t aSize)
	{
		myStream->ReadData(aData, aSize);
	}

	void
	ReadMessage::SkipBytes(
		uint64_t aSize)
	{
		myStream->SkipData(aSize);
	}

	//////////////////////////////////////////////////////////////////////////////////////

	WriteMessage::WriteMessage()
		: mySize(0)
		, myTotalSize(0)
		, myPos(nullptr)
		, myHeaderPos(nullptr)
	{
	}

	mg::net::Buffer::Ptr
	WriteMessage::TakeData()
	{
		MG_DEV_ASSERT(myHeaderPos == nullptr);
		MG_DEV_ASSERT(mySize == 0);
		myTotalSize = 0;
		myPos = nullptr;
		return std::move(myHead);
	}

	void
	WriteMessage::Open()
	{
		MG_DEV_ASSERT(myHeaderPos == nullptr);
		MG_DEV_ASSERT(mySize == 0);
		if (!myHead.IsSet())
			myPos = (myHead = mg::net::BufferCopy::NewShared()).GetPointer();
		// The header position is remembered and reserved to save it here later when the
		// packet length is known. That is much easier when the header is contiguous in a
		// single memory block.
		if (myPos->myCapacity - myPos->myPos < sizeof(MessageHeader))
		{
			myPos = (myPos->myNext =
				mg::net::BufferCopy::NewShared()).GetPointer();
		}
		myHeaderPos = myPos->myWData + myPos->myPos;
		myPos->myPos += sizeof(MessageHeader);
		myTotalSize += sizeof(MessageHeader);
	}

	void
	WriteMessage::Close()
	{
		MG_DEV_ASSERT(myHeaderPos != nullptr);
		MessageHeader header;
		header.myBodySize = mySize;
		memcpy(myHeaderPos, &header, sizeof(header));
		myHeaderPos = nullptr;
		mySize = 0;
	}

	void
	WriteMessage::WriteString(
		const char* aStr)
	{
		WriteString(aStr, mg::box::Strlen(aStr));
	}

	void
	WriteMessage::WriteString(
		const char* aStr,
		uint64_t aLen)
	{
		WriteData(&aLen, sizeof(aLen));
		WriteData(aStr, aLen);
	}

	void
	WriteMessage::WriteData(
		const void* aData,
		uint64_t aSize)
	{
		MG_DEV_ASSERT(myHeaderPos != nullptr);
		myTotalSize += aSize;
		mySize += aSize;
		while (true)
		{
			uint64_t cap = myPos->myCapacity - myPos->myPos;
			uint64_t toWrite = cap >= aSize ? aSize : cap;
			memcpy(myPos->myWData + myPos->myPos, aData, toWrite);
			myPos->myPos += (uint32_t)toWrite;
			aSize -= toWrite;
			if (aSize == 0)
				return;
			aData = (const uint8_t*)aData + toWrite;
			myPos = (myPos->myNext =
				mg::net::BufferCopy::NewShared()).GetPointer();
		}
	}

}
}
