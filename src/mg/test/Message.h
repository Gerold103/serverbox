#pragma once

#include "mg/box/Definitions.h"

namespace mg {
namespace test {

	class ReadMessage
	{
	public:
		ReadMessage();
		ReadMessage(
			ReadMessage&& aObj);

		void Attach(
			mg::net::BufferStream& aStream);

		bool IsComplete() const;
		bool Open();
		uint64_t GetSize() const;

		bool ReadUInt8(
			uint8_t& aOut);
		bool ReadUInt16(
			uint16_t& aOut);
		bool ReadUInt32(
			uint32_t& aOut);
		bool ReadUInt64(
			uint64_t& aOut);

		bool ReadInt8(
			int8_t& aOut);
		bool ReadInt16(
			int16_t& aOut);
		bool ReadInt32(
			int32_t& aOut);
		bool ReadInt64(
			int64_t& aOut);

		bool ReadBool(
			bool& aOut);
		bool ReadString(
			std::string& aOut);
		bool ReadData(
			void* aData,
			uint64_t aSize);
		bool SkipData(
			uint64_t aSize);

	private:
		mg::net::BufferStream* myStream;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	class WriteMessage
	{
	public:
		WriteMessage();
		WriteMessage(
			WriteMessage&& aObj);

		void FromRef(
			mg::net::Buffer* aHead);
		void FromRef(
			mg::net::Buffer* aHead,
			mg::net::Buffer* aTail);
		void FromRef(
			mg::net::BufferList&& aData);

		mg::net::BufferList TakeData();

		void Open();
		void Close();

		void WriteUInt8(
			uint8_t aValue);
		void WriteUInt16(
			uint16_t aValue);
		void WriteUInt32(
			uint32_t aValue);
		void WriteUInt64(
			uint64_t aValue);

		void WriteInt8(
			uint8_t aValue);
		void WriteInt16(
			uint16_t aValue);
		void WriteInt32(
			uint32_t aValue);
		void WriteInt64(
			uint64_t aValue);

		void WriteBool(
			bool aValue);
		void WriteString(
			const std::string& aStr);
		void WriteString(
			const char* aStr);
		void WriteString(
			const char* aStr,
			uint64_t aLen);
		void WriteData(
			const void* aData,
			uint64_t aSize);

	private:
		mg::net::BufferList myData;
		uint64_t mySize;
		mg::net::Buffer* myBegin;
		mg::net::Buffer* myPos;
	};

}
}
