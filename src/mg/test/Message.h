#pragma once

#include "mg/net/Buffer.h"

#include <string>

namespace mg {
namespace tst {

	struct MessageHeader
	{
		MessageHeader() : myBodySize(0) {}

		uint64_t myBodySize;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	class ReadMessage
	{
	public:
		ReadMessage(
			mg::net::BufferReadStream& aStream);

		bool IsComplete() const;
		void Open();
		void Close();
		uint64_t GetAvailSize() const { return myStream->GetReadSize(); }

		void ReadUInt8(
			uint8_t& aOut) { ReadData(&aOut, sizeof(aOut)); }
		void ReadUInt16(
			uint16_t& aOut) { ReadData(&aOut, sizeof(aOut)); }
		void ReadUInt32(
			uint32_t& aOut) { ReadData(&aOut, sizeof(aOut)); }
		void ReadUInt64(
			uint64_t& aOut) { ReadData(&aOut, sizeof(aOut)); }

		void ReadInt8(
			int8_t& aOut) { ReadData(&aOut, sizeof(aOut)); }
		void ReadInt16(
			int16_t& aOut) { ReadData(&aOut, sizeof(aOut)); }
		void ReadInt32(
			int32_t& aOut) { ReadData(&aOut, sizeof(aOut)); }
		void ReadInt64(
			int64_t& aOut) { ReadData(&aOut, sizeof(aOut)); }

		void ReadBool(
			bool& aOut) { ReadData(&aOut, sizeof(aOut)); }
		void ReadString(
			std::string& aOut);
		void ReadData(
			void* aData,
			uint64_t aSize);
		void SkipBytes(
			uint64_t aSize);

	private:
		mg::net::BufferReadStream* myStream;
		MessageHeader myHeader;
		bool myIsOpen;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	class WriteMessage
	{
	public:
		WriteMessage();

		uint64_t GetTotalSize() const { return myTotalSize; }
		mg::net::Buffer::Ptr TakeData();

		void Open();
		void Close();

		void WriteUInt8(
			uint8_t aValue) { WriteData(&aValue, sizeof(aValue)); }
		void WriteUInt16(
			uint16_t aValue) { WriteData(&aValue, sizeof(aValue)); }
		void WriteUInt32(
			uint32_t aValue) { WriteData(&aValue, sizeof(aValue)); }
		void WriteUInt64(
			uint64_t aValue) { WriteData(&aValue, sizeof(aValue)); }

		void WriteInt8(
			uint8_t aValue) { WriteData(&aValue, sizeof(aValue)); }
		void WriteInt16(
			uint16_t aValue) { WriteData(&aValue, sizeof(aValue)); }
		void WriteInt32(
			uint32_t aValue) { WriteData(&aValue, sizeof(aValue)); }
		void WriteInt64(
			uint64_t aValue) { WriteData(&aValue, sizeof(aValue)); }

		void WriteBool(
			bool aValue) { WriteData(&aValue, sizeof(aValue)); }
		void WriteString(
			const std::string& aStr) { WriteString(aStr.c_str(), aStr.length()); }
		void WriteString(
			const char* aStr);
		void WriteString(
			const char* aStr,
			uint64_t aLen);
		void WriteData(
			const void* aData,
			uint64_t aSize);

	private:
		mg::net::Buffer::Ptr myHead;
		uint64_t mySize;
		uint64_t myTotalSize;
		mg::net::Buffer* myPos;
		uint8_t* myHeaderPos;
	};

}
}
