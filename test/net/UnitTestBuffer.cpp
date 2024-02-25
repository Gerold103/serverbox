#include "mg/net/Buffer.h"

#include "mg/box/IOVec.h"
#include "mg/box/StringFunctions.h"

#include "UnitTest.h"

#if IS_COMPILER_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

namespace mg {
namespace unittests {
namespace net {
namespace buffer {

	static void BufferFill(
		uint64_t aSalt,
		uint8_t* aData,
		uint64_t aOffset,
		uint64_t aSize);

	static void BufferCheck(
		uint64_t aSalt,
		const uint8_t* aData,
		uint64_t aOffset,
		uint64_t aSize);

	//////////////////////////////////////////////////////////////////////////////////////

	static void
	UnitTestBufferCopy()
	{
		TestCaseGuard guard("BufferCopy");

		mg::net::Buffer::Ptr b = mg::net::BufferCopy::NewShared();
		TEST_CHECK(&b->myWData == &b->myRData);
		TEST_CHECK(b->myPos == 0);
		TEST_CHECK(b->myCapacity == mg::net::theBufferCopySize);
		memcpy(b->myWData, "defg", 5);
		b->myPos = 5;
		TEST_CHECK(!b->myNext.IsSet());

		// Can link with a buffer of another type.
		b->myNext = mg::net::BufferRaw::NewShared("1234", 4);
		b.Clear();
	}

	static void
	UnitTestBufferRaw()
	{
		TestCaseGuard guard("BufferRaw");

		mg::net::Buffer::Ptr b = mg::net::BufferRaw::NewShared();
		TEST_CHECK(&b->myWData == &b->myRData);
		TEST_CHECK(b->myWData == nullptr);
		TEST_CHECK(b->myPos == 0);
		TEST_CHECK(b->myCapacity == 0);
		TEST_CHECK(!b->myNext.IsSet());

		b = mg::net::BufferRaw::NewShared("1234", 4, 10);
		TEST_CHECK(b->myPos == 4);
		TEST_CHECK(b->myCapacity == 10);
		TEST_CHECK(memcmp(b->myRData, "1234", 4) == 0);
		TEST_CHECK(!b->myNext.IsSet());

		// Can link with a buffer of another type.
		b->myNext = mg::net::BufferCopy::NewShared();
		b.Clear();
	}

	static void
	UnitTestBufferPropagate()
	{
		TestCaseGuard guard("Buffer::Propagate()");

		const uint8_t* data = (const uint8_t*)"1234";
		uint32_t size = 4;
		uint32_t cap = 10;
		mg::net::Buffer::Ptr b = mg::net::BufferRaw::NewShared(data, size, cap);

		b->Propagate(0);
		TEST_CHECK(b->myRData == data);
		TEST_CHECK(b->myPos == size);
		TEST_CHECK(b->myCapacity == cap);

		b->Propagate(3);
		TEST_CHECK(b->myRData == data + 3);
		TEST_CHECK(b->myPos == size - 3);
		TEST_CHECK(b->myCapacity == cap - 3);

		b->Propagate(1);
		TEST_CHECK(b->myRData == data + size);
		TEST_CHECK(b->myPos == 0);
		TEST_CHECK(b->myCapacity == cap - size);
	}

	static void
	UnitTestBufferLink()
	{
		TestCaseGuard guard("BufferLink");

		mg::net::BufferLink* link = new mg::net::BufferLink();
		TEST_CHECK(!link->myHead.IsSet());
		TEST_CHECK(link->myNext == nullptr);
		delete link;

		mg::net::Buffer::Ptr buf = mg::net::BufferRaw::NewShared("123", 3);
		buf->myNext = mg::net::BufferRaw::NewShared("45", 2);
		mg::net::Buffer* nextBuf = buf->myNext.GetPointer();
		link = new mg::net::BufferLink(buf.GetPointer());
		TEST_CHECK(link->myNext == nullptr);
		TEST_CHECK(link->myHead == buf);
		TEST_CHECK(buf->myNext == nextBuf);
		delete link;

		link = new mg::net::BufferLink(buf);
		TEST_CHECK(link->myNext == nullptr);
		TEST_CHECK(link->myHead == buf);
		TEST_CHECK(buf->myNext == nextBuf);
		delete link;

		mg::net::Buffer* firstBuf = buf.GetPointer();
		link = new mg::net::BufferLink(std::move(buf));
		TEST_CHECK(link->myNext == nullptr);
		TEST_CHECK(link->myHead == firstBuf);
		TEST_CHECK(firstBuf->myNext == nextBuf);
		// Ensure the object wasn't deleted, can be dereferenced.
		TEST_CHECK(firstBuf->myPos == 3);
		TEST_CHECK(nextBuf->myPos == 2);

		// Link deletion doesn't cause deletion of all the next links. They are not
		// reference counted and don't own each other.
		link->myNext = new mg::net::BufferLink();
		mg::net::BufferLink* nextLink = link->myNext;
		nextLink->myHead = mg::net::BufferRaw::NewShared("789", 3);
		delete link;
		TEST_CHECK(nextLink->myHead->myPos == 3);
		TEST_CHECK(nextLink->myNext == nullptr);
	}

	static void
	UnitTestBufferLinkListConstructor()
	{
		TestCaseGuard guard("BufferLinkList constructor");

		mg::net::BufferLinkList list;
		TEST_CHECK(list.IsEmpty());
	}

	static void
	UnitTestBufferLinkListAppendRefRawPtr()
	{
		TestCaseGuard guard("BufferLinkList::AppendRef(const Buffer*)");

		mg::net::BufferLinkList list;
		const uint8_t* data1 = (const uint8_t*)"123";
		uint32_t size1 = 3;
		const uint8_t* data2 = (const uint8_t*)"45";
		uint32_t size2 = 2;

		mg::net::Buffer::Ptr b1 = mg::net::BufferRaw::NewShared(data1, size1);
		mg::net::Buffer::Ptr b2 = mg::net::BufferRaw::NewShared(data2, size2);
		b1->myNext = b2;
		list.AppendRef(b1.GetPointer());

		const mg::net::BufferLink* link = list.GetFirst();
		TEST_CHECK(link->myHead == b1);
		TEST_CHECK(link->myHead->myNext == b2);

		// Null buffers are skipped. Makes no sense to store them, can't be neither read
		// from nor written to.
		list.Clear();
		list.AppendRef((const mg::net::Buffer*)nullptr);
		TEST_CHECK(list.IsEmpty());
	}

	static void
	UnitTestBufferLinkListAppendRefPtr()
	{
		TestCaseGuard guard("BufferLinkList::AppendRef(const Buffer::Ptr&)");

		mg::net::BufferLinkList list;
		const uint8_t* data1 = (const uint8_t*)"123";
		uint32_t size1 = 3;
		const uint8_t* data2 = (const uint8_t*)"45";
		uint32_t size2 = 2;

		mg::net::Buffer::Ptr b1 = mg::net::BufferRaw::NewShared(data1, size1);
		mg::net::Buffer::Ptr b2 = mg::net::BufferRaw::NewShared(data2, size2);
		b1->myNext = b2;
		list.AppendRef(b1);

		const mg::net::BufferLink* link = list.GetFirst();
		TEST_CHECK(link->myHead == b1);
		TEST_CHECK(link->myHead->myNext == b2);

		list.Clear();
		b1.Clear();
		list.AppendRef(b1);
		TEST_CHECK(list.IsEmpty());
	}

	static void
	UnitTestBufferLinkListAppendRefMovePtr()
	{
		TestCaseGuard guard("BufferLinkList::AppendRef(Buffer::Ptr&&)");

		mg::net::BufferLinkList list;
		const uint8_t* data1 = (const uint8_t*)"123";
		uint32_t size1 = 3;
		const uint8_t* data2 = (const uint8_t*)"45";
		uint32_t size2 = 2;

		mg::net::Buffer::Ptr b1 = mg::net::BufferRaw::NewShared(data1, size1);
		mg::net::Buffer::Ptr b2 = mg::net::BufferRaw::NewShared(data2, size2);
		b1->myNext = b2;

		mg::net::Buffer::Ptr ref = b1;
		list.AppendRef(std::move(ref));
		TEST_CHECK(!ref.IsSet());
		const mg::net::BufferLink* link = list.GetFirst();
		TEST_CHECK(link->myHead == b1);
		TEST_CHECK(link->myHead->myNext == b2);

		list.Clear();
		list.AppendRef(mg::net::Buffer::Ptr());
		TEST_CHECK(list.IsEmpty());
	}

	static void
	UnitTestBufferLinkListAppendRefData()
	{
		TestCaseGuard guard("BufferLinkList::AppendRef(const void*, uint64_t)");

		const uint8_t* data = (const uint8_t*)"123";
		uint32_t size = 3;
		mg::net::BufferLinkList list;
		// 32 bits.
		list.AppendRef(data, size);
		mg::net::Buffer* b1 = list.GetFirst()->myHead.GetPointer();
		TEST_CHECK(b1->myRData == data);
		TEST_CHECK(b1->myPos == 3);
		TEST_CHECK(b1->myCapacity == 3);
		// 64 bits.
		list.Clear();
		list.AppendRef(data, (uint64_t)UINT32_MAX * 2 + 100);
		b1 = list.GetFirst()->myHead.GetPointer();
		TEST_CHECK(b1->myRData == data);
		TEST_CHECK(b1->myPos == UINT32_MAX);
		TEST_CHECK(b1->myCapacity == UINT32_MAX);
		mg::net::Buffer* b2 = b1->myNext.GetPointer();
		TEST_CHECK(b2->myRData == data + UINT32_MAX);
		TEST_CHECK(b2->myPos == UINT32_MAX);
		TEST_CHECK(b2->myCapacity == UINT32_MAX);
		mg::net::Buffer* b3 = b2->myNext.GetPointer();
		TEST_CHECK(b3->myRData == data + (uint64_t)UINT32_MAX * 2);
		TEST_CHECK(b3->myPos == 100);
		TEST_CHECK(b3->myCapacity == 100);

		list.Clear();
		list.AppendRef("123", 0);
		TEST_CHECK(list.IsEmpty());
	}

	static void
	UnitTestBufferLinkListAppendMoveLink()
	{
		TestCaseGuard guard("BufferLinkList::AppendMove(BufferLink*)");

		const uint8_t* data1 = (const uint8_t*)"123";
		uint32_t size1 = 3;
		const uint8_t* data2 = (const uint8_t*)"45";
		uint32_t size2 = 2;

		mg::net::Buffer::Ptr b1 = mg::net::BufferRaw::NewShared(data1, size1);
		mg::net::Buffer::Ptr b2 = mg::net::BufferRaw::NewShared(data2, size2);
		b1->myNext = b2;

		mg::net::BufferLink* linkToMove = new mg::net::BufferLink(b1);
		mg::net::BufferLinkList list;
		list.AppendMove(linkToMove);

		const mg::net::BufferLink* link = list.GetFirst();
		TEST_CHECK(link == linkToMove);
		TEST_CHECK(link->myHead == b1);
		TEST_CHECK(link->myHead->myNext == b2);

		list.Clear();
		list.AppendMove((mg::net::BufferLink*)nullptr);
		TEST_CHECK(list.IsEmpty());
	}

	static void
	UnitTestBufferLinkListAppendMoveLinkList()
	{
		TestCaseGuard guard("BufferLinkList::Append(BufferLinkList&&)");

		const uint8_t* data1 = (const uint8_t*)"123";
		uint32_t size1 = 3;
		const uint8_t* data2 = (const uint8_t*)"45";
		uint32_t size2 = 2;

		mg::net::Buffer::Ptr b1 = mg::net::BufferRaw::NewShared(data1, size1);
		mg::net::BufferLinkList list1;
		list1.AppendRef(b1);

		mg::net::Buffer::Ptr b2 = mg::net::BufferRaw::NewShared(data2, size2);
		mg::net::BufferLinkList list2;
		list2.AppendRef(b2);

		list1.Append(std::move(list2));
		TEST_CHECK(list2.IsEmpty());
		mg::net::BufferLink* link = list1.GetFirst();
		TEST_CHECK(link->myHead == b1);
		TEST_CHECK(!b1->myNext.IsSet());

		link = link->myNext;
		TEST_CHECK(link->myHead == b2);
		TEST_CHECK(!b2->myNext.IsSet());
	}

	static void
	UnitTestBufferLinkListAppendCopyData()
	{
		TestCaseGuard guard("BufferLinkList::AppendCopy("
			"const void*, uint64_t)");

		const uint8_t* data = (const uint8_t*)"123";
		uint32_t size = 3;
		// Small buffer.
		mg::net::BufferLinkList list;
		list.AppendCopy(data, size);
		mg::net::BufferLink* link = list.GetFirst();
		TEST_CHECK(link->myNext == nullptr);
		mg::net::Buffer* buf = link->myHead.GetPointer();
		TEST_CHECK(buf->myRData != data);
		TEST_CHECK(buf->myPos == size);
		TEST_CHECK(buf->myCapacity == mg::net::theBufferCopySize);
		TEST_CHECK(memcmp(buf->myRData, data, size) == 0);
		TEST_CHECK(!buf->myNext.IsSet());
		// Big buffer.
		uint64_t bigSize = mg::net::theBufferCopySize * 2 + 5;
		uint8_t* big = new uint8_t[bigSize];
		list.Clear();
		list.AppendCopy(big, bigSize);
		link = list.GetFirst();
		TEST_CHECK(link->myNext == nullptr);
		buf = link->myHead.GetPointer();
		TEST_CHECK(buf->myRData != big);
		TEST_CHECK(buf->myPos == mg::net::theBufferCopySize);
		TEST_CHECK(buf->myCapacity == mg::net::theBufferCopySize);
		TEST_CHECK(memcmp(buf->myRData, big, mg::net::theBufferCopySize) == 0);
		buf = buf->myNext.GetPointer();
		TEST_CHECK(buf->myPos == mg::net::theBufferCopySize);
		TEST_CHECK(buf->myCapacity == mg::net::theBufferCopySize);
		TEST_CHECK(memcmp(buf->myRData,
			big + mg::net::theBufferCopySize, mg::net::theBufferCopySize) == 0);
		buf = buf->myNext.GetPointer();
		TEST_CHECK(buf->myPos == 5);
		TEST_CHECK(buf->myCapacity == mg::net::theBufferCopySize);
		TEST_CHECK(memcmp(buf->myRData,
			big + mg::net::theBufferCopySize * 2, 5) == 0);
		TEST_CHECK(!buf->myNext.IsSet());

		list.Clear();
		list.AppendCopy("123", 0);
		TEST_CHECK(list.IsEmpty());
	}

	static void
	UnitTestBufferLinkListAppendMany()
	{
		TestCaseGuard guard("BufferLinkList append many");

		const uint8_t* data1 = (const uint8_t*)"123";
		uint32_t size1 = 3;
		const uint8_t* data2 = (const uint8_t*)"45";
		uint32_t size2 = 2;
		const uint8_t* data3 = (const uint8_t*)"6789";
		uint32_t size3 = 4;

		mg::net::BufferLinkList list;
		mg::net::Buffer::Ptr b1 = mg::net::BufferRaw::NewShared(data1, size1);
		list.AppendRef(b1);

		mg::net::Buffer::Ptr b2 = mg::net::BufferRaw::NewShared(data2, size2);
		mg::net::Buffer::Ptr b3 = mg::net::BufferRaw::NewShared(data3, size3);
		b2->myNext = b3;
		list.AppendRef(b2);

		mg::net::BufferLink* link = list.GetFirst();
		TEST_CHECK(link->myHead == b1);
		TEST_CHECK(!b1->myNext.IsSet());
		TEST_CHECK(link->myNext->myHead == b2);
		TEST_CHECK(b2->myNext == b3);
		TEST_CHECK(!b3->myNext.IsSet());
	}

	static void
	UnitTestBufferLinkListSkipData()
	{
		TestCaseGuard guard("BufferLinkList::SkipData()");

		uint32_t offset;
		mg::net::BufferLinkList list;
		auto buildList = [&]() {
			offset = 0;
			list.Clear();

			mg::net::Buffer::Ptr b1 = mg::net::BufferRaw::NewShared("123", 3);
			list.AppendRef(b1);

			b1 = mg::net::BufferRaw::NewShared("45678", 5);
			b1->myNext = mg::net::BufferRaw::NewShared("9101112", 7);
			b1->myNext->myNext = mg::net::BufferRaw::NewShared("1314", 4);
			list.AppendRef(b1);

			b1 = mg::net::BufferRaw::NewShared("151617", 6);
			list.AppendRef(b1);
		};

		auto checkData = [&](const char* str) {
			uint32_t offsetLocal = offset;
			const mg::net::BufferLink* link = list.GetFirst();
			uint32_t len = mg::box::Strlen(str);
			while (link != nullptr)
			{
				const mg::net::Buffer* buf = link->myHead.GetPointer();
				do {
					TEST_CHECK(offsetLocal < buf->myPos);
					uint32_t bufSize = buf->myPos - offsetLocal;
					const uint8_t* bufData = buf->myRData + offsetLocal;
					offsetLocal = 0;

					TEST_CHECK(bufSize <= len);
					TEST_CHECK(memcmp(bufData, str, bufSize) == 0);
					str += bufSize;
					len -= bufSize;
					buf = buf->myNext.GetPointer();
				} while(buf != nullptr);
				link = link->myNext;
			}
			TEST_CHECK(len == 0);
		};

		// Zero.
		buildList();
		list.SkipData(offset, 0);
		TEST_CHECK(offset == 0);

		// Part of first link.
		buildList();
		list.SkipData(offset, 2);
		checkData("34567891011121314151617");
		TEST_CHECK(offset == 2);
		TEST_CHECK(list.GetFirst()->myHead->myPos == 3);

		// Whole first link.
		buildList();
		list.SkipData(offset, 3);
		checkData("4567891011121314151617");
		TEST_CHECK(offset == 0);
		TEST_CHECK(list.GetFirst()->myHead->myPos == 5);

		// First link + second link part of first buf.
		buildList();
		list.SkipData(offset, 5);
		checkData("67891011121314151617");
		TEST_CHECK(offset == 2);
		TEST_CHECK(list.GetFirst()->myHead->myPos == 5);

		// First link + second link whole first buf.
		buildList();
		list.SkipData(offset, 8);
		checkData("91011121314151617");
		TEST_CHECK(offset == 0);
		TEST_CHECK(list.GetFirst()->myHead->myPos == 7);

		// First link + second link part of second buf.
		buildList();
		list.SkipData(offset, 11);
		checkData("11121314151617");
		TEST_CHECK(offset == 3);
		TEST_CHECK(list.GetFirst()->myHead->myPos == 7);

		// First link + second link whole second buf.
		buildList();
		list.SkipData(offset, 15);
		checkData("1314151617");
		TEST_CHECK(offset == 0);
		TEST_CHECK(list.GetFirst()->myHead->myPos == 4);

		// First link + second link part of third buf.
		buildList();
		list.SkipData(offset, 16);
		checkData("314151617");
		TEST_CHECK(offset == 1);
		TEST_CHECK(list.GetFirst()->myHead->myPos == 4);

		// First link + second link whole third buf.
		buildList();
		list.SkipData(offset, 19);
		checkData("151617");
		TEST_CHECK(offset == 0);
		TEST_CHECK(list.GetFirst()->myHead->myPos == 6);

		// Second link + part of third link.
		buildList();
		list.SkipData(offset, 23);
		checkData("17");
		TEST_CHECK(offset == 4);
		TEST_CHECK(list.GetFirst()->myHead->myPos == 6);

		// All.
		buildList();
		list.SkipData(offset, 25);
		TEST_CHECK(offset == 0);
		TEST_CHECK(list.IsEmpty());
	}

	static void
	UnitTestBufferLinkListSkipEmptyPrefix()
	{
		TestCaseGuard guard("BufferLinkList::SkipEmptyPrefix()");

		mg::net::BufferLinkList list;
		// Empty.
		uint32_t offset = 0;
		list.SkipEmptyPrefix(offset);
		TEST_CHECK(list.IsEmpty());
		TEST_CHECK(offset == 0);

		// Has empty buffers.
		list.AppendRef(mg::net::BufferRaw::NewShared());
		TEST_CHECK(!list.IsEmpty());
		list.SkipEmptyPrefix(offset);
		TEST_CHECK(list.IsEmpty());
		TEST_CHECK(offset == 0);

		// Non-empty offset in the first buffer.
		list.AppendRef(mg::net::BufferRaw::NewShared("123", 3));
		for (uint32_t i = 1; i < 3; ++i)
		{
			offset = i;
			list.SkipEmptyPrefix(offset);
			TEST_CHECK(!list.IsEmpty());
			TEST_CHECK(list.GetFirst()->myHead->myPos == 3);
			TEST_CHECK(memcmp(list.GetFirst()->myHead->myRData, "123", 3) == 0);
			TEST_CHECK(offset == i);
		}
		offset = 3;
		list.SkipEmptyPrefix(offset);
		TEST_CHECK(list.IsEmpty());
		TEST_CHECK(offset == 0);

		// Skip multiple links and stop in the middle of one.
		list.AppendRef(mg::net::BufferRaw::NewShared());
		list.AppendRef(mg::net::BufferRaw::NewShared());
		mg::net::Buffer::Ptr b = mg::net::BufferRaw::NewShared();
		mg::net::Buffer* tail = b.GetPointer();
		tail = (tail->myNext = mg::net::BufferRaw::NewShared()).GetPointer();
		tail = (tail->myNext = mg::net::BufferRaw::NewShared("abcd", 4)).GetPointer();
		tail = (tail->myNext = mg::net::BufferRaw::NewShared()).GetPointer();
		tail = (tail->myNext = mg::net::BufferRaw::NewShared("efg", 3)).GetPointer();
		list.AppendRef(std::move(b));

		TEST_CHECK(list.GetFirst()->myHead->myPos == 0);
		for (uint32_t i = 0; i < 4; ++i)
		{
			offset = i;
			list.SkipEmptyPrefix(offset);
			TEST_CHECK(list.GetFirst()->myHead->myPos == 4);
			TEST_CHECK(memcmp(list.GetFirst()->myHead->myRData, "abcd", 4) == 0);
			TEST_CHECK(offset == i);
		}
		offset = 4;
		list.SkipEmptyPrefix(offset);
		TEST_CHECK(list.GetFirst()->myHead->myPos == 3);
		TEST_CHECK(memcmp(list.GetFirst()->myHead->myRData, "efg", 3) == 0);
		TEST_CHECK(offset == 0);
		for (uint32_t i = 0; i < 3; ++i)
		{
			offset = i;
			list.SkipEmptyPrefix(offset);
			TEST_CHECK(list.GetFirst()->myHead->myPos == 3);
			TEST_CHECK(memcmp(list.GetFirst()->myHead->myRData, "efg", 3) == 0);
			TEST_CHECK(offset == i);
		}
		offset = 3;
		list.SkipEmptyPrefix(offset);
		TEST_CHECK(list.IsEmpty());
		TEST_CHECK(offset == 0);
	}

	static void
	UnitTestBufferLinkListMisc()
	{
		TestCaseGuard guard("BufferLinkList misc");

		// Clear().
		{
			mg::net::BufferLinkList list;
			list.Clear();
			TEST_CHECK(list.IsEmpty());

			list.AppendCopy("123", 3);
			TEST_CHECK(!list.IsEmpty());
			list.Clear();
			TEST_CHECK(list.IsEmpty());
		}
		// GetFirst().
		{
			mg::net::BufferLinkList list;
			TEST_CHECK(list.GetFirst() == nullptr);
			const mg::net::BufferLinkList& clist = list;
			TEST_CHECK(clist.GetFirst() == nullptr);

			list.AppendCopy("123", 3);
			TEST_CHECK(list.GetFirst()->myHead->myPos == 3);
			TEST_CHECK(clist.GetFirst()->myHead->myPos == 3);
		}
		// PopFirst().
		{
			mg::net::BufferLinkList list;
			list.AppendCopy("123", 3);
			list.AppendCopy("4567", 4);
			mg::net::BufferLink* link = list.PopFirst();
			TEST_CHECK(link->myHead->myPos == 3);
			TEST_CHECK(!link->myHead->myNext.IsSet());
			TEST_CHECK(link->myNext == list.GetFirst());
			TEST_CHECK(list.GetFirst()->myHead->myPos == 4);
			delete link;

			link = list.PopFirst();
			TEST_CHECK(list.IsEmpty());
			TEST_CHECK(link->myHead->myPos == 4);
			TEST_CHECK(!link->myHead->myNext.IsSet());
			TEST_CHECK(link->myNext == nullptr);
			delete link;
		}
		// IsEmpty().
		{
			mg::net::BufferLinkList list;
			TEST_CHECK(list.IsEmpty());
			list.AppendRef("789", 3);
			TEST_CHECK(!list.IsEmpty());
		}
	}

	static void
	UnitTestBufferStreamPropagateWritePos()
	{
		TestCaseGuard guard("BufferStream::PropagateWritePos()");

		mg::net::BufferStream stream;
		stream.PropagateWritePos(0);
		TEST_CHECK(stream.GetWriteSize() == 0);

		const uint8_t* data = (const uint8_t*)"12345";
		uint64_t size = 5;
		stream.EnsureWriteSize(size);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize);
		TEST_CHECK(stream.GetWritePos()->myPos == 0);
		memcpy(stream.GetWritePos()->myWData, data, size);
		stream.PropagateWritePos(3);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize - 3);
		TEST_CHECK(stream.GetReadPos()->myPos == 3);
		TEST_CHECK(memcmp(stream.GetReadPos()->myRData, data, size) == 0);

		stream.PropagateWritePos(2);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize - size);
		TEST_CHECK(stream.GetReadPos()->myPos == size);
		TEST_CHECK(memcmp(stream.GetReadPos()->myRData, data, size) == 0);

		stream.Clear();
		stream.EnsureWriteSize(mg::net::theBufferCopySize * 2);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize * 2);
		TEST_CHECK(!stream.GetWritePos()->myNext->myNext.IsSet());
		TEST_CHECK(stream.GetWritePos()->myPos == 0);
		TEST_CHECK(stream.GetWritePos()->myNext->myPos == 0);
		stream.PropagateWritePos(mg::net::theBufferCopySize * 3/2);
		TEST_CHECK(stream.GetReadPos()->myPos == mg::net::theBufferCopySize);
		TEST_CHECK(stream.GetReadPos()->myNext->myPos ==
			mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.GetReadPos()->myNext == stream.GetWritePos());
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize / 2);
	}

	static void
	UnitTestBufferStreamEnsureWriteSize()
	{
		TestCaseGuard guard("BufferStream::EnsureWriteSize()");

		mg::net::BufferStream stream;
		stream.EnsureWriteSize(0);
		TEST_CHECK(stream.GetWriteSize() == 0);

		stream.EnsureWriteSize(1);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize);
		mg::net::Buffer* head = stream.GetWritePos();
		stream.EnsureWriteSize(100);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize);
		stream.EnsureWriteSize(mg::net::theBufferCopySize);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize);
		stream.EnsureWriteSize(mg::net::theBufferCopySize + 1);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize * 2);
		TEST_CHECK(stream.GetWritePos() == head);
		TEST_CHECK(!head->myNext->myNext.IsSet());
		TEST_CHECK(head->myPos == 0);
		TEST_CHECK(head->myNext->myPos == 0);

		stream.PropagateWritePos(100);
		TEST_CHECK(head->myPos == 100);
		TEST_CHECK(stream.GetWriteSize() ==
			mg::net::theBufferCopySize * 2 - head->myPos);
		stream.EnsureWriteSize(10);
		TEST_CHECK(stream.GetWriteSize() ==
			mg::net::theBufferCopySize * 2 - head->myPos);
		stream.EnsureWriteSize(mg::net::theBufferCopySize * 2);
		TEST_CHECK(stream.GetWriteSize() ==
			mg::net::theBufferCopySize * 3 - head->myPos);
		TEST_CHECK(stream.GetWritePos() == head);
		TEST_CHECK(!head->myNext->myNext->myNext.IsSet());
		TEST_CHECK(head->myPos == 100);
		TEST_CHECK(head->myNext->myPos == 0);
		TEST_CHECK(head->myNext->myNext->myPos == 0);

		// Move wpos if after ensuring the size the position is fully used. First buffer
		// for writing shouldn't have zero capacity.
		stream.Clear();
		stream.EnsureWriteSize(1);
		head = stream.GetWritePos();
		stream.PropagateWritePos(head->myCapacity);
		stream.EnsureWriteSize(1);
		TEST_CHECK(stream.GetWritePos() == head->myNext);
		TEST_CHECK(stream.GetWritePos()->myPos == 0);
	}

	static void
	UnitTestBufferStreamSkipData()
	{
		TestCaseGuard guard("BufferStream::SkipData()");

		mg::net::BufferStream stream;
		stream.SkipData(0);
		TEST_CHECK(stream.GetReadSize() == 0);

		const uint8_t* data = (const uint8_t*)"123456789";
		uint64_t size = 9;
		stream.EnsureWriteSize(size);
		memcpy(stream.GetWritePos()->myWData, data, size);
		stream.PropagateWritePos(size);

		TEST_CHECK(stream.GetReadSize() == size);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize - size);
		stream.SkipData(4);
		TEST_CHECK(stream.GetReadSize() == size - 4);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize - size);
		const mg::net::Buffer* rpos = stream.GetReadPos();
		TEST_CHECK(rpos->myPos == size - 4);
		TEST_CHECK(rpos->myCapacity == mg::net::theBufferCopySize - 4);
		TEST_CHECK(memcmp(rpos->myRData, data + 4, size - 4) == 0);
		TEST_CHECK(rpos == stream.GetWritePos());

		stream.SkipData(size - 4);
		TEST_CHECK(stream.GetReadSize() == 0);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize - size);
		rpos = stream.GetReadPos();
		TEST_CHECK(rpos == stream.GetWritePos());
		TEST_CHECK(rpos->myPos == 0);
		TEST_CHECK(rpos->myCapacity == mg::net::theBufferCopySize - size);

		stream.Clear();
		stream.EnsureWriteSize(mg::net::theBufferCopySize * 2);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize * 2);
		stream.PropagateWritePos(mg::net::theBufferCopySize * 3/2);
		rpos = stream.GetReadPos();
		const mg::net::Buffer* rposNext = rpos->myNext.GetPointer();
		TEST_CHECK(rposNext == stream.GetWritePos());
		TEST_CHECK(rpos->myPos == mg::net::theBufferCopySize);
		stream.SkipData(mg::net::theBufferCopySize);
		rpos = stream.GetReadPos();
		TEST_CHECK(rpos == rposNext);
		TEST_CHECK(stream.GetReadSize() == mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize / 2);

		stream.SkipData(stream.GetReadSize());
		rpos = stream.GetReadPos();
		TEST_CHECK(rpos == stream.GetWritePos());
		TEST_CHECK(rpos->myPos == 0);
		TEST_CHECK(rpos->myCapacity == mg::net::theBufferCopySize / 2);

		stream.PropagateWritePos(mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.GetWriteSize() == 0);
		TEST_CHECK(stream.GetReadSize() == mg::net::theBufferCopySize / 2);
		stream.SkipData(stream.GetReadSize());
		TEST_CHECK(stream.IsEmpty());
		TEST_CHECK(stream.GetWritePos() == nullptr);
		TEST_CHECK(stream.GetReadPos() == nullptr);
		TEST_CHECK(stream.GetWriteSize() == 0);
		TEST_CHECK(stream.GetReadSize() == 0);
	}

	static void
	UnitTestBufferStreamReadData()
	{
		TestCaseGuard guard("BufferStream::ReadData()");

		mg::net::BufferStream stream;
		constexpr uint64_t bufSize = mg::net::theBufferCopySize * 2;
		uint8_t* buf = new uint8_t[bufSize];
		stream.ReadData(nullptr, 0);
		TEST_CHECK(stream.GetReadSize() == 0);

		const uint8_t* data = (const uint8_t*)"123456789";
		uint64_t size = 9;
		stream.EnsureWriteSize(size);
		memcpy(stream.GetWritePos()->myWData, data, size);
		stream.PropagateWritePos(size);

		TEST_CHECK(stream.GetReadSize() == size);
		stream.ReadData(buf, 4);
		TEST_CHECK(memcmp(buf, data, 4) == 0);
		TEST_CHECK(stream.GetReadSize() == size - 4);
		const mg::net::Buffer* rpos = stream.GetReadPos();
		TEST_CHECK(rpos->myPos == size - 4);
		TEST_CHECK(rpos->myCapacity == mg::net::theBufferCopySize - 4);
		TEST_CHECK(memcmp(rpos->myRData, data + 4, size - 4) == 0);
		TEST_CHECK(rpos == stream.GetWritePos());

		stream.ReadData(buf, size - 4);
		TEST_CHECK(memcmp(buf, data + 4, size - 4) == 0);
		TEST_CHECK(stream.GetReadSize() == 0);
		rpos = stream.GetReadPos();
		TEST_CHECK(rpos == stream.GetWritePos());
		TEST_CHECK(rpos->myPos == 0);
		TEST_CHECK(rpos->myCapacity == mg::net::theBufferCopySize - size);

		stream.Clear();
		stream.EnsureWriteSize(mg::net::theBufferCopySize * 2);
		mg::net::Buffer* wpos = stream.GetWritePos();
		BufferFill(1, wpos->myWData, 0, mg::net::theBufferCopySize);
		BufferFill(2, wpos->myNext->myWData, 0, mg::net::theBufferCopySize / 2);
		stream.PropagateWritePos(mg::net::theBufferCopySize * 3/2);
		stream.ReadData(buf, mg::net::theBufferCopySize);
		BufferCheck(1, buf, 0, mg::net::theBufferCopySize);
		TEST_CHECK(stream.GetReadSize() == mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize / 2);

		stream.ReadData(buf, stream.GetReadSize());
		BufferCheck(2, buf, 0, mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.GetWritePos() != nullptr);
		TEST_CHECK(stream.GetReadPos() != nullptr);

		BufferFill(2, stream.GetWritePos()->myWData, mg::net::theBufferCopySize / 2,
			mg::net::theBufferCopySize / 2);
		stream.PropagateWritePos(mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.GetWriteSize() == 0);
		TEST_CHECK(stream.GetReadSize() == mg::net::theBufferCopySize / 2);
		stream.ReadData(buf, stream.GetReadSize());
		BufferCheck(2, buf, mg::net::theBufferCopySize / 2,
			mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.IsEmpty());
		TEST_CHECK(stream.GetWritePos() == nullptr);
		TEST_CHECK(stream.GetReadPos() == nullptr);
		TEST_CHECK(stream.GetWriteSize() == 0);
		TEST_CHECK(stream.GetReadSize() == 0);

		stream.EnsureWriteSize(mg::net::theBufferCopySize * 2 + 100);
		wpos = stream.GetWritePos();
		BufferFill(1, wpos->myWData, 0, mg::net::theBufferCopySize);
		BufferFill(2, wpos->myNext->myWData, 0, mg::net::theBufferCopySize);
		BufferFill(3, wpos->myNext->myNext->myWData, 0, 100);
		stream.PropagateWritePos(mg::net::theBufferCopySize * 2 + 100);

		stream.ReadData(buf, mg::net::theBufferCopySize * 2);
		BufferCheck(1, buf, 0, mg::net::theBufferCopySize);
		BufferCheck(2, buf + mg::net::theBufferCopySize, 0, mg::net::theBufferCopySize);
		TEST_CHECK(stream.GetWritePos() == stream.GetReadPos());
		TEST_CHECK(stream.GetReadSize() == 100);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize - 100);
		stream.ReadData(buf, stream.GetReadSize());
		BufferCheck(3, buf, 0, 100);
		TEST_CHECK(!stream.IsEmpty());
		TEST_CHECK(stream.GetWritePos() != nullptr);
		TEST_CHECK(stream.GetReadPos() == stream.GetWritePos());
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize - 100);
		TEST_CHECK(stream.GetReadSize() == 0);

		delete[] buf;
	}

	static void
	UnitTestBufferStreamPeekData()
	{
		TestCaseGuard guard("BufferStream::PeekData()");

		mg::net::BufferStream stream;
		constexpr uint64_t bufSize = mg::net::theBufferCopySize * 2;
		uint8_t* buf = new uint8_t[bufSize];
		stream.PeekData(nullptr, 0);
		TEST_CHECK(stream.GetReadSize() == 0);

		const uint8_t* data = (const uint8_t*)"123456789";
		uint64_t size = 9;
		stream.EnsureWriteSize(size);
		memcpy(stream.GetWritePos()->myWData, data, size);
		stream.PropagateWritePos(size);

		TEST_CHECK(stream.GetReadSize() == size);
		stream.PeekData(buf, 4);
		TEST_CHECK(memcmp(buf, data, 4) == 0);
		TEST_CHECK(stream.GetReadSize() == size);
		const mg::net::Buffer* rpos = stream.GetReadPos();
		TEST_CHECK(rpos->myPos == size);
		TEST_CHECK(rpos->myCapacity == mg::net::theBufferCopySize);
		TEST_CHECK(memcmp(rpos->myRData, data, size) == 0);
		TEST_CHECK(rpos == stream.GetWritePos());
		stream.SkipData(4);
		TEST_CHECK(stream.GetReadSize() == size - 4);

		stream.PeekData(buf, size - 4);
		TEST_CHECK(memcmp(buf, data + 4, size - 4) == 0);
		TEST_CHECK(stream.GetReadSize() == size - 4);
		rpos = stream.GetReadPos();
		TEST_CHECK(rpos == stream.GetWritePos());
		TEST_CHECK(rpos->myPos == size - 4);
		TEST_CHECK(rpos->myCapacity == mg::net::theBufferCopySize - 4);
		stream.SkipData(size - 4);
		TEST_CHECK(stream.GetReadSize() == 0);

		stream.Clear();
		stream.EnsureWriteSize(mg::net::theBufferCopySize * 2);
		mg::net::Buffer* wpos = stream.GetWritePos();
		BufferFill(1, wpos->myWData, 0, mg::net::theBufferCopySize);
		BufferFill(2, wpos->myNext->myWData, 0, mg::net::theBufferCopySize / 2);
		stream.PropagateWritePos(mg::net::theBufferCopySize * 3/2);
		stream.PeekData(buf, mg::net::theBufferCopySize);
		BufferCheck(1, buf, 0, mg::net::theBufferCopySize);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.GetReadSize() == mg::net::theBufferCopySize * 3/2);
		stream.SkipData(mg::net::theBufferCopySize);
		TEST_CHECK(stream.GetReadSize() == mg::net::theBufferCopySize / 2);

		stream.PeekData(buf, stream.GetReadSize());
		BufferCheck(2, buf, 0, mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.GetReadSize() == mg::net::theBufferCopySize / 2);
		stream.SkipData(stream.GetReadSize());
		TEST_CHECK(stream.GetReadSize() == 0);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.GetWritePos() != nullptr);
		TEST_CHECK(stream.GetReadPos() != nullptr);

		BufferFill(2, stream.GetWritePos()->myWData, mg::net::theBufferCopySize / 2,
			mg::net::theBufferCopySize / 2);
		stream.PropagateWritePos(mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.GetWriteSize() == 0);
		TEST_CHECK(stream.GetReadSize() == mg::net::theBufferCopySize / 2);
		stream.PeekData(buf, stream.GetReadSize());
		BufferCheck(2, buf, mg::net::theBufferCopySize / 2,
			mg::net::theBufferCopySize / 2);
		TEST_CHECK(stream.GetReadSize() == mg::net::theBufferCopySize / 2);
		stream.SkipData(stream.GetReadSize());
		TEST_CHECK(stream.IsEmpty());
		TEST_CHECK(stream.GetWritePos() == nullptr);
		TEST_CHECK(stream.GetReadPos() == nullptr);
		TEST_CHECK(stream.GetWriteSize() == 0);
		TEST_CHECK(stream.GetReadSize() == 0);

		stream.EnsureWriteSize(mg::net::theBufferCopySize * 2 + 100);
		wpos = stream.GetWritePos();
		BufferFill(1, wpos->myWData, 0, mg::net::theBufferCopySize);
		BufferFill(2, wpos->myNext->myWData, 0, mg::net::theBufferCopySize);
		BufferFill(3, wpos->myNext->myNext->myWData, 0, 100);
		stream.PropagateWritePos(mg::net::theBufferCopySize * 2 + 100);

		stream.PeekData(buf, mg::net::theBufferCopySize * 2);
		BufferCheck(1, buf, 0, mg::net::theBufferCopySize);
		BufferCheck(2, buf + mg::net::theBufferCopySize, 0, mg::net::theBufferCopySize);
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize - 100);
		TEST_CHECK(stream.GetReadSize() == mg::net::theBufferCopySize * 2 + 100);
		stream.SkipData(mg::net::theBufferCopySize * 2);
		TEST_CHECK(stream.GetWritePos() == stream.GetReadPos());
		TEST_CHECK(stream.GetReadSize() == 100);

		stream.PeekData(buf, stream.GetReadSize());
		BufferCheck(3, buf, 0, 100);
		TEST_CHECK(stream.GetReadSize() == 100);
		stream.SkipData(100);
		TEST_CHECK(!stream.IsEmpty());
		TEST_CHECK(stream.GetWritePos() != nullptr);
		TEST_CHECK(stream.GetReadPos() == stream.GetWritePos());
		TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize - 100);
		TEST_CHECK(stream.GetReadSize() == 0);

		delete[] buf;
	}

	static void
	UnitTestBufferStreamMisc()
	{
		TestCaseGuard guard("BufferStream misc");

		// Constructor.
		{
			mg::net::BufferStream stream;
			TEST_CHECK(stream.IsEmpty());
			TEST_CHECK(stream.GetWritePos() == nullptr);
			TEST_CHECK(stream.GetReadPos() == nullptr);
			TEST_CHECK(stream.GetWriteSize() == 0);
			TEST_CHECK(stream.GetReadSize() == 0);
		}
		// GetWritePos().
		{
			mg::net::BufferStream stream;
			TEST_CHECK(stream.GetWritePos() == nullptr);
			stream.EnsureWriteSize(100);
			TEST_CHECK(stream.GetWritePos()->myCapacity == mg::net::theBufferCopySize);
			stream.Clear();
			TEST_CHECK(stream.GetWritePos() == nullptr);
		}
		// GetReadPos().
		{
			mg::net::BufferStream stream;
			TEST_CHECK(stream.GetReadPos() == nullptr);
			stream.EnsureWriteSize(100);
			TEST_CHECK(stream.GetReadPos()->myCapacity == mg::net::theBufferCopySize);
			stream.Clear();
			TEST_CHECK(stream.GetReadPos() == nullptr);
		}
		// GetReadSize().
		{
			mg::net::BufferStream stream;
			TEST_CHECK(stream.GetReadSize() == 0);
			stream.EnsureWriteSize(100);
			TEST_CHECK(stream.GetReadSize() == 0);
			stream.PropagateWritePos(50);
			TEST_CHECK(stream.GetReadSize() == 50);
			stream.SkipData(20);
			TEST_CHECK(stream.GetReadSize() == 30);
			stream.Clear();
			TEST_CHECK(stream.GetReadSize() == 0);
		}
		// GetWriteSize().
		{
			mg::net::BufferStream stream;
			TEST_CHECK(stream.GetWriteSize() == 0);
			stream.EnsureWriteSize(100);
			TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize);
			stream.PropagateWritePos(50);
			TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize - 50);
			stream.Clear();
			TEST_CHECK(stream.GetWriteSize() == 0);
		}
		// Clear().
		{
			mg::net::BufferStream stream;
			stream.EnsureWriteSize(100);
			stream.PropagateWritePos(50);
			TEST_CHECK(!stream.IsEmpty());
			TEST_CHECK(stream.GetWritePos() != nullptr);
			TEST_CHECK(stream.GetReadPos() != nullptr);
			TEST_CHECK(stream.GetWriteSize() == mg::net::theBufferCopySize - 50);
			TEST_CHECK(stream.GetReadSize() == 50);
			stream.Clear();
			TEST_CHECK(stream.IsEmpty());
			TEST_CHECK(stream.GetWritePos() == nullptr);
			TEST_CHECK(stream.GetReadPos() == nullptr);
			TEST_CHECK(stream.GetWriteSize() == 0);
			TEST_CHECK(stream.GetReadSize() == 0);
		}
		// IsEmpty().
		{
			mg::net::BufferStream stream;
			TEST_CHECK(stream.IsEmpty());
			stream.EnsureWriteSize(100);
			TEST_CHECK(!stream.IsEmpty());
			stream.PropagateWritePos(50);
			stream.SkipData(50);
			TEST_CHECK(!stream.IsEmpty());
			stream.Clear();
			TEST_CHECK(stream.IsEmpty());
		}
	}

	static void
	UnitTestBufferReadStreamSkipData()
	{
		TestCaseGuard guard("BufferReadStream::SkipData()");

		mg::net::BufferStream base;
		mg::net::BufferReadStream stream(base);
		TEST_CHECK(stream.IsEmpty());
		TEST_CHECK(stream.GetReadSize() == 0);
		stream.SkipData(0);

		base.EnsureWriteSize(100);
		TEST_CHECK(stream.GetReadSize() == 0);
		base.PropagateWritePos(50);
		TEST_CHECK(stream.GetReadSize() == 50);
		stream.SkipData(20);
		TEST_CHECK(stream.GetReadSize() == 30);
		TEST_CHECK(base.GetReadSize() == 30);
	}

	static void
	UnitTestBufferReadStreamReadData()
	{
		TestCaseGuard guard("BufferReadStream::ReadData()");

		mg::net::BufferStream base;
		mg::net::BufferReadStream stream(base);
		stream.ReadData(nullptr, 0);

		constexpr uint64_t bufSize = 100;
		uint8_t buf[bufSize];
		base.EnsureWriteSize(1);
		mg::net::Buffer* wpos = base.GetWritePos();
		BufferFill(1, wpos->myWData, 0, bufSize);
		base.PropagateWritePos(bufSize);
		TEST_CHECK(stream.GetReadSize() == bufSize);

		uint64_t toRead = bufSize / 3;
		stream.ReadData(buf, toRead);
		TEST_CHECK(stream.GetReadSize() == bufSize - toRead);
		BufferCheck(1, buf, 0, toRead);
		toRead = bufSize - toRead;
		stream.ReadData(buf, toRead);
		TEST_CHECK(stream.GetReadSize() == 0);
		BufferCheck(1, buf, bufSize / 3, toRead);
	}

	static void
	UnitTestBufferReadStreamPeekData()
	{
		TestCaseGuard guard("BufferReadStream::PeekData()");

		mg::net::BufferStream base;
		mg::net::BufferReadStream stream(base);
		stream.PeekData(nullptr, 0);

		constexpr uint64_t bufSize = 100;
		uint8_t buf[bufSize];
		base.EnsureWriteSize(1);
		mg::net::Buffer* wpos = base.GetWritePos();
		BufferFill(1, wpos->myWData, 0, bufSize);
		base.PropagateWritePos(bufSize);
		TEST_CHECK(stream.GetReadSize() == bufSize);

		uint64_t toRead = bufSize / 3;
		stream.PeekData(buf, toRead);
		TEST_CHECK(stream.GetReadSize() == bufSize);
		BufferCheck(1, buf, 0, toRead);
		stream.SkipData(toRead);
		TEST_CHECK(stream.GetReadSize() == bufSize - toRead);

		toRead = bufSize - toRead;
		stream.PeekData(buf, toRead);
		BufferCheck(1, buf, bufSize / 3, toRead);
		TEST_CHECK(stream.GetReadSize() != 0);
		stream.SkipData(toRead);
		TEST_CHECK(stream.GetReadSize() == 0);
	}

	static void
	UnitTestBufferReadStreamMisc()
	{
		TestCaseGuard guard("BufferReadStream misc");

		// Constructor.
		{
			mg::net::BufferStream base;
			{
				mg::net::BufferReadStream stream(base);
				TEST_CHECK(stream.GetReadPos() == nullptr);
				TEST_CHECK(stream.GetReadSize() == 0);
				TEST_CHECK(stream.IsEmpty());
			}
			base.EnsureWriteSize(100);
			base.PropagateWritePos(50);
			{
				mg::net::BufferReadStream stream(base);
				TEST_CHECK(stream.GetReadPos() != nullptr);
				TEST_CHECK(stream.GetReadSize() == 50);
				TEST_CHECK(!stream.IsEmpty());
			}
		}
		// GetReadPos().
		{
			mg::net::BufferStream base;
			mg::net::BufferReadStream stream(base);
			TEST_CHECK(stream.GetReadPos() == nullptr);
			base.EnsureWriteSize(100);
			TEST_CHECK(stream.GetReadPos() != nullptr);
		}
		// GetReadSize().
		{
			mg::net::BufferStream base;
			mg::net::BufferReadStream stream(base);
			base.EnsureWriteSize(100);
			TEST_CHECK(stream.GetReadSize() == 0);
			base.PropagateWritePos(50);
			TEST_CHECK(stream.GetReadSize() == 50);
		}
	}

	static void
	UnitTestBuffersCopyData()
	{
		TestCaseGuard guard("BuffersCopy(const void*)");

		// Empty.
		mg::net::Buffer::Ptr head = mg::net::BuffersCopy(nullptr, 0);
		TEST_CHECK(!head.IsSet());

		// Size < one buf.
		head = mg::net::BuffersCopy("12345", 5);
		TEST_CHECK(head->myPos == 5);
		TEST_CHECK(head->myCapacity == mg::net::theBufferCopySize);
		TEST_CHECK(memcmp(head->myRData, "12345", 5) == 0);
		TEST_CHECK(!head->myNext.IsSet());

		// Size exactly one buf.
		uint64_t size = mg::net::theBufferCopySize;
		uint8_t* buf = new uint8_t[size];
		BufferFill(1, buf, 0, size);
		head = mg::net::BuffersCopy(buf, size);
		TEST_CHECK(head->myPos == mg::net::theBufferCopySize);
		TEST_CHECK(head->myCapacity == mg::net::theBufferCopySize);
		BufferCheck(1, head->myRData, 0, head->myPos);
		TEST_CHECK(!head->myNext.IsSet());

		// Size > buf, but < 2 bufs.
		delete[] buf;
		size = mg::net::theBufferCopySize * 4 / 3;
		buf = new uint8_t[size];
		BufferFill(1, buf, 0, size);
		head = mg::net::BuffersCopy(buf, size);
		const mg::net::Buffer* pos = head.GetPointer();
		TEST_CHECK(pos->myPos == mg::net::theBufferCopySize);
		TEST_CHECK(pos->myCapacity == mg::net::theBufferCopySize);
		BufferCheck(1, pos->myRData, 0, pos->myPos);
		pos = pos->myNext.GetPointer();
		TEST_CHECK(pos->myPos == size - mg::net::theBufferCopySize);
		TEST_CHECK(pos->myCapacity == mg::net::theBufferCopySize);
		BufferCheck(1, pos->myRData, mg::net::theBufferCopySize, pos->myPos);
		TEST_CHECK(!pos->myNext.IsSet());

		// Size exactly 2 bufs.
		delete[] buf;
		size = mg::net::theBufferCopySize * 2;
		buf = new uint8_t[size];
		BufferFill(1, buf, 0, size);
		head = mg::net::BuffersCopy(buf, size);
		pos = head.GetPointer();
		TEST_CHECK(pos->myPos == mg::net::theBufferCopySize);
		TEST_CHECK(pos->myCapacity == mg::net::theBufferCopySize);
		BufferCheck(1, pos->myRData, 0, pos->myPos);
		pos = pos->myNext.GetPointer();
		TEST_CHECK(pos->myPos == mg::net::theBufferCopySize);
		TEST_CHECK(pos->myCapacity == mg::net::theBufferCopySize);
		BufferCheck(1, pos->myRData, mg::net::theBufferCopySize, pos->myPos);
		TEST_CHECK(!pos->myNext.IsSet());

		// Size > 2 bufs.
		delete[] buf;
		size = mg::net::theBufferCopySize * 8 / 3;
		buf = new uint8_t[size];
		BufferFill(1, buf, 0, size);
		head = mg::net::BuffersCopy(buf, size);
		pos = head.GetPointer();
		TEST_CHECK(pos->myPos == mg::net::theBufferCopySize);
		TEST_CHECK(pos->myCapacity == mg::net::theBufferCopySize);
		BufferCheck(1, pos->myRData, 0, pos->myPos);
		pos = pos->myNext.GetPointer();
		TEST_CHECK(pos->myPos == mg::net::theBufferCopySize);
		TEST_CHECK(pos->myCapacity == mg::net::theBufferCopySize);
		BufferCheck(1, pos->myRData, mg::net::theBufferCopySize, pos->myPos);
		pos = pos->myNext.GetPointer();
		TEST_CHECK(pos->myPos == size - mg::net::theBufferCopySize * 2);
		TEST_CHECK(pos->myCapacity == mg::net::theBufferCopySize);
		BufferCheck(1, pos->myRData, mg::net::theBufferCopySize * 2, pos->myPos);
		TEST_CHECK(!pos->myNext.IsSet());
	}

	static void
	UnitTestBuffersCopyBufferList()
	{
		TestCaseGuard guard("BuffersCopy(const Buffer*)");

		// Empty.
		mg::net::Buffer::Ptr head = mg::net::BuffersCopy((mg::net::Buffer*)nullptr);
		TEST_CHECK(!head.IsSet());
		// Empty prefix is skipped.
		mg::net::Buffer::Ptr srcHead = mg::net::BufferRaw::NewShared(nullptr, 0);
		mg::net::Buffer* tail = srcHead.GetPointer();
		tail->myNext = mg::net::BufferRaw::NewShared(nullptr, 0);
		head = mg::net::BuffersCopy(srcHead.GetPointer());
		TEST_CHECK(!head.IsSet());
		// Same with a smart ptr.
		head = mg::net::BuffersCopy(srcHead);
		TEST_CHECK(!head.IsSet());

		// Scattered data with holes.
		srcHead = mg::net::BufferRaw::NewShared(nullptr, 0);
		tail = srcHead.GetPointer();
		tail = (tail->myNext = mg::net::BufferRaw::NewShared("123", 3)).GetPointer();
		tail = (tail->myNext = mg::net::BufferRaw::NewShared(nullptr, 0)).GetPointer();
		tail = (tail->myNext = mg::net::BufferRaw::NewShared(nullptr, 0)).GetPointer();
		tail = (tail->myNext = mg::net::BufferRaw::NewShared("4567", 4)).GetPointer();
		tail = (tail->myNext = mg::net::BufferRaw::NewShared(nullptr, 0)).GetPointer();
		tail = (tail->myNext = mg::net::BufferRaw::NewShared("89", 2)).GetPointer();
		head = mg::net::BuffersCopy(srcHead.GetPointer());
		TEST_CHECK(head->myPos == 9);
		TEST_CHECK(head->myCapacity == mg::net::theBufferCopySize);
		TEST_CHECK(memcmp(head->myRData, "123456789", 9) == 0);
		TEST_CHECK(!head->myNext.IsSet());
		// Same with a smart ptr.
		head = mg::net::BuffersCopy(srcHead);
		TEST_CHECK(head->myPos == 9);

		// Size > one buf.
		srcHead = mg::net::BufferCopy::NewShared();
		tail = srcHead.GetPointer();
		tail->myPos = tail->myCapacity;
		BufferFill(1, tail->myWData, 0, tail->myPos);
		tail = (tail->myNext = mg::net::BufferCopy::NewShared()).GetPointer();
		tail->myPos = tail->myCapacity / 3;
		BufferFill(2, tail->myWData, 0, tail->myPos);
		head = mg::net::BuffersCopy(srcHead.GetPointer());
		TEST_CHECK(head->myPos == mg::net::theBufferCopySize);
		BufferCheck(1, head->myRData, 0, head->myPos);
		head = std::move(head->myNext);
		TEST_CHECK(head->myPos == mg::net::theBufferCopySize / 3);
		BufferCheck(2, head->myRData, 0, head->myPos);
		TEST_CHECK(!head->myNext.IsSet());
		// Same with a smart ptr.
		head = mg::net::BuffersCopy(srcHead);
		TEST_CHECK(head->myPos == mg::net::theBufferCopySize);
		TEST_CHECK(head->myNext->myPos == mg::net::theBufferCopySize / 3);
	}

	static void
	UnitTestBuffersCopyBufferLinkList()
	{
		TestCaseGuard guard("BuffersCopy(const BufferLink*)");

		// Empty.
		mg::net::Buffer::Ptr head = mg::net::BuffersCopy((mg::net::BufferLink*)nullptr);
		TEST_CHECK(!head.IsSet());
		// Empty prefix is skipped.
		// [{}] -> [{}->{}] -> [] -> [{}].
		mg::net::Buffer::Ptr bufHead = mg::net::BufferRaw::NewShared();
		mg::net::BufferLink* srcHead = new mg::net::BufferLink(std::move(bufHead));
		mg::net::BufferLink* srcTail = srcHead;

		bufHead = mg::net::BufferRaw::NewShared();
		bufHead->myNext = mg::net::BufferRaw::NewShared();
		srcTail = (srcTail->myNext = new mg::net::BufferLink(std::move(bufHead)));

		srcTail = (srcTail->myNext = new mg::net::BufferLink());

		bufHead = mg::net::BufferRaw::NewShared();
		srcTail = (srcTail->myNext = new mg::net::BufferLink(std::move(bufHead)));

		head = mg::net::BuffersCopy(srcHead);
		TEST_CHECK(!head.IsSet());
		while (srcHead != nullptr)
		{
			mg::net::BufferLink* next = srcHead->myNext;
			delete srcHead;
			srcHead = next;
		}

		// Scattered data with holes.
		// [] -> [{} -> {123} -> {}] -> [{4567}] -> [{} -> {89}]
		srcHead = new mg::net::BufferLink();
		srcTail = srcHead;

		bufHead = mg::net::BufferRaw::NewShared();
		mg::net::Buffer* bufTail = bufHead.GetPointer();
		bufTail = (bufTail->myNext =
			mg::net::BufferRaw::NewShared("123", 3)).GetPointer();
		bufTail = (bufTail->myNext =
			mg::net::BufferRaw::NewShared()).GetPointer();
		srcTail = (srcTail->myNext = new mg::net::BufferLink(std::move(bufHead)));

		bufHead = mg::net::BufferRaw::NewShared("4567", 4);
		srcTail = (srcTail->myNext = new mg::net::BufferLink(std::move(bufHead)));

		srcTail = (srcTail->myNext = new mg::net::BufferLink());

		bufHead = mg::net::BufferRaw::NewShared();
		bufHead->myNext = mg::net::BufferRaw::NewShared("89", 2);
		srcTail = (srcTail->myNext = new mg::net::BufferLink(std::move(bufHead)));

		head = mg::net::BuffersCopy(srcHead);
		TEST_CHECK(head->myPos == 9);
		TEST_CHECK(head->myCapacity == mg::net::theBufferCopySize);
		TEST_CHECK(memcmp(head->myRData, "123456789", 9) == 0);
		TEST_CHECK(!head->myNext.IsSet());
		while (srcHead != nullptr)
		{
			mg::net::BufferLink* next = srcHead->myNext;
			delete srcHead;
			srcHead = next;
		}

		// Size > one buf.
		// [{size / 3}] -> [{size}] -> [{size 2/3}] -> [{123456}]
		bufHead = mg::net::BufferCopy::NewShared();
		bufHead->myPos = mg::net::theBufferCopySize / 3;
		BufferFill(1, bufHead->myWData, 0, bufHead->myPos);
		srcHead = new mg::net::BufferLink(std::move(bufHead));
		srcTail = srcHead;

		bufHead = mg::net::BufferCopy::NewShared();
		bufHead->myPos = mg::net::theBufferCopySize;
		BufferFill(2, bufHead->myWData, 0, bufHead->myPos);
		srcTail = (srcTail->myNext = new mg::net::BufferLink(std::move(bufHead)));

		bufHead = mg::net::BufferCopy::NewShared();
		bufHead->myPos = mg::net::theBufferCopySize - mg::net::theBufferCopySize / 3;
		BufferFill(3, bufHead->myWData, 0, bufHead->myPos);
		srcTail = (srcTail->myNext = new mg::net::BufferLink(std::move(bufHead)));

		bufHead = mg::net::BufferRaw::NewShared("123456", 6);
		srcTail->myNext = new mg::net::BufferLink(std::move(bufHead));

		head = mg::net::BuffersCopy(srcHead);
		TEST_CHECK(head->myPos == mg::net::theBufferCopySize);
		BufferCheck(1, head->myRData, 0, mg::net::theBufferCopySize / 3);
		BufferCheck(2, head->myRData + mg::net::theBufferCopySize / 3, 0,
			mg::net::theBufferCopySize - mg::net::theBufferCopySize / 3);

		head = std::move(head->myNext);
		TEST_CHECK(head->myPos == mg::net::theBufferCopySize);
		BufferCheck(2, head->myRData,
			mg::net::theBufferCopySize - mg::net::theBufferCopySize / 3,
			mg::net::theBufferCopySize / 3);
		BufferCheck(3, head->myRData + mg::net::theBufferCopySize / 3, 0,
			mg::net::theBufferCopySize - mg::net::theBufferCopySize / 3);

		head = std::move(head->myNext);
		TEST_CHECK(head->myPos == 6);
		TEST_CHECK(memcmp(head->myRData, "123456", 6) == 0);
		TEST_CHECK(!head->myNext.IsSet());
		while (srcHead != nullptr)
		{
			mg::net::BufferLink* next = srcHead->myNext;
			delete srcHead;
			srcHead = next;
		}
	}

	static void
	UnitTestBuffersRefData()
	{
		TestCaseGuard guard("BuffersRef()");

		// Empty.
		mg::net::Buffer::Ptr head = mg::net::BuffersRef(nullptr, 0);
		TEST_CHECK(!head.IsSet());

		// Small.
		const void* data = "1";
		head = mg::net::BuffersRef(data, 1000);
		TEST_CHECK(head->myRData == data);
		TEST_CHECK(head->myPos == 1000);
		TEST_CHECK(head->myCapacity == 1000);
		TEST_CHECK(!head->myNext.IsSet());

		// Max buf size.
		head = mg::net::BuffersRef(data, UINT32_MAX);
		TEST_CHECK(head->myRData == data);
		TEST_CHECK(head->myPos == UINT32_MAX);
		TEST_CHECK(head->myCapacity == UINT32_MAX);
		TEST_CHECK(!head->myNext.IsSet());

		// > max buf size, < x2 max buf size.
		head = mg::net::BuffersRef(data, UINT32_MAX + 100LL);
		TEST_CHECK(head->myRData == data);
		TEST_CHECK(head->myPos == UINT32_MAX);
		TEST_CHECK(head->myCapacity == UINT32_MAX);
		head = std::move(head->myNext);
		TEST_CHECK(head->myRData == (const uint8_t*)data + UINT32_MAX);
		TEST_CHECK(head->myPos == 100);
		TEST_CHECK(head->myCapacity == 100);
		TEST_CHECK(!head->myNext.IsSet());

		// x2 max buf size.
		head = mg::net::BuffersRef(data, UINT32_MAX * 2LL);
		TEST_CHECK(head->myRData == data);
		TEST_CHECK(head->myPos == UINT32_MAX);
		TEST_CHECK(head->myCapacity == UINT32_MAX);
		head = std::move(head->myNext);
		TEST_CHECK(head->myRData == (const uint8_t*)data + UINT32_MAX);
		TEST_CHECK(head->myPos == UINT32_MAX);
		TEST_CHECK(head->myCapacity == UINT32_MAX);
		TEST_CHECK(!head->myNext.IsSet());

		// > x2 max buf size.
		head = mg::net::BuffersRef(data, UINT32_MAX * 2LL + 100);
		TEST_CHECK(head->myRData == data);
		TEST_CHECK(head->myPos == UINT32_MAX);
		TEST_CHECK(head->myCapacity == UINT32_MAX);
		head = std::move(head->myNext);
		TEST_CHECK(head->myRData == (const uint8_t*)data + UINT32_MAX);
		TEST_CHECK(head->myPos == UINT32_MAX);
		TEST_CHECK(head->myCapacity == UINT32_MAX);
		head = std::move(head->myNext);
		TEST_CHECK(head->myRData == (const uint8_t*)data + UINT32_MAX * 2LL);
		TEST_CHECK(head->myPos == 100);
		TEST_CHECK(head->myCapacity == 100);
		TEST_CHECK(!head->myNext.IsSet());
	}

	static void
	UnitTestBuffersPropagateOnWrite()
	{
		TestCaseGuard guard("BuffersPropagateOnWrite()");

		// Empty.
		TEST_CHECK(mg::net::BuffersPropagateOnWrite(nullptr, 0) == nullptr);

		// In first buf.
		const uint8_t* data = (const uint8_t*)"1";
		mg::net::Buffer::Ptr head = mg::net::BufferRaw::NewShared(data, 0, 100);
		head->myNext = mg::net::BufferRaw::NewShared(data + 100, 0, 50);
		mg::net::Buffer* res = mg::net::BuffersPropagateOnWrite(
			head.GetPointer(), 70);
		TEST_CHECK(res == head);
		TEST_CHECK(head->myPos == 70);
		TEST_CHECK(head->myCapacity == 100);
		TEST_CHECK(head->myRData == data);
		TEST_CHECK(head->myNext->myPos == 0);
		TEST_CHECK(head->myNext->myCapacity == 50);
		TEST_CHECK(head->myNext->myRData == data + 100);

		// Continue the first buf.
		res = mg::net::BuffersPropagateOnWrite(head.GetPointer(), 20);
		TEST_CHECK(res == head);
		TEST_CHECK(head->myPos == 90);
		TEST_CHECK(head->myCapacity == 100);
		TEST_CHECK(head->myRData == data);
		TEST_CHECK(head->myNext->myPos == 0);
		TEST_CHECK(head->myNext->myCapacity == 50);
		TEST_CHECK(head->myNext->myRData == data + 100);

		// Go after the first buf.
		res = mg::net::BuffersPropagateOnWrite(head.GetPointer(), 40);
		TEST_CHECK(res == head->myNext);
		TEST_CHECK(head->myPos == 100);
		TEST_CHECK(head->myCapacity == 100);
		TEST_CHECK(head->myRData == data);
		TEST_CHECK(head->myNext->myPos == 30);
		TEST_CHECK(head->myNext->myCapacity == 50);
		TEST_CHECK(head->myNext->myRData == data + 100);

		// Finish the second buf.
		res = mg::net::BuffersPropagateOnWrite(head.GetPointer(), 20);
		TEST_CHECK(res == nullptr);
		TEST_CHECK(head->myPos == 100);
		TEST_CHECK(head->myCapacity == 100);
		TEST_CHECK(head->myRData == data);
		TEST_CHECK(head->myNext->myPos == 50);
		TEST_CHECK(head->myNext->myCapacity == 50);
		TEST_CHECK(head->myNext->myRData == data + 100);
	}

	static void
	UnitTestBuffersPropagateOnRead()
	{
		TestCaseGuard guard("BuffersPropagateOnRead()");

		// Empty.
		mg::net::Buffer::Ptr head;
		mg::net::BuffersPropagateOnRead(head, 0);

		// In first buf.
		const uint8_t* data = (const uint8_t*)"1";
		head = mg::net::BufferRaw::NewShared(data, 30, 100);
		head->myNext = mg::net::BufferRaw::NewShared(data + 30, 40, 50);
		mg::net::Buffer::Ptr next = head->myNext;
		mg::net::Buffer::Ptr pos = head;
		mg::net::BuffersPropagateOnRead(pos, 20);
		TEST_CHECK(pos == head);
		TEST_CHECK(pos->myPos == 10);
		TEST_CHECK(pos->myCapacity == 80);
		TEST_CHECK(pos->myRData == data + 20);
		TEST_CHECK(pos->myNext->myPos == 40);
		TEST_CHECK(pos->myNext->myCapacity == 50);
		TEST_CHECK(pos->myNext->myRData == data + 30);

		// Continue the first buf.
		mg::net::BuffersPropagateOnRead(pos, 8);
		TEST_CHECK(pos == head);
		TEST_CHECK(pos->myPos == 2);
		TEST_CHECK(pos->myCapacity == 72);
		TEST_CHECK(pos->myRData == data + 28);
		TEST_CHECK(pos->myNext->myPos == 40);
		TEST_CHECK(pos->myNext->myCapacity == 50);
		TEST_CHECK(pos->myNext->myRData == data + 30);

		// Exactly the first buf.
		mg::net::BuffersPropagateOnRead(pos, 2);
		// Not moved because the buffer still has capacity. Could be used for writing,
		// potentially.
		TEST_CHECK(pos == head);
		TEST_CHECK(pos->myPos == 0);
		TEST_CHECK(pos->myCapacity == 70);
		TEST_CHECK(pos->myRData == data + 30);
		TEST_CHECK(pos->myNext->myPos == 40);
		TEST_CHECK(pos->myNext->myCapacity == 50);
		TEST_CHECK(pos->myNext->myRData == data + 30);

		// When consumed without any capacity left, then switch happens.
		head->myPos = 10;
		head->myCapacity = 10;
		mg::net::BuffersPropagateOnRead(pos, 10);
		TEST_CHECK(pos == next);
		TEST_CHECK(!head->myNext.IsSet());
		TEST_CHECK(head->myPos == 0);
		TEST_CHECK(head->myCapacity == 0);
		TEST_CHECK(pos->myPos == 40);
		TEST_CHECK(pos->myCapacity == 50);
		TEST_CHECK(pos->myRData == data + 30);

		// Consume over the border.
		head->myPos = 10;
		head->myCapacity = 10;
		head->myRData = data;
		head->myNext = next;
		pos = head;
		mg::net::BuffersPropagateOnRead(pos, 25);
		TEST_CHECK(pos == next);
		TEST_CHECK(!head->myNext.IsSet());
		TEST_CHECK(pos->myPos == 25);
		TEST_CHECK(pos->myCapacity == 35);
		TEST_CHECK(pos->myRData == data + 45);

		// Finish the second buf.
		pos->myCapacity = pos->myPos;
		mg::net::BuffersPropagateOnRead(pos, 25);
		TEST_CHECK(!pos.IsSet());
	}

	static void
	UnitTestBuffersToIOVecsForWriteBufferList()
	{
		TestCaseGuard guard("BuffersToIOVecsForWrite(const Buffer*)");

		// Single buffer, no offset.
		const uint8_t* data = (const uint8_t*)"1";
		mg::net::Buffer::Ptr head = mg::net::BufferRaw::NewShared(data, 10, 30);
		constexpr uint32_t vecCount = 10;
		mg::box::IOVec vecs[vecCount];
		memset(vecs, 0, sizeof(vecs));
		TEST_CHECK(mg::net::BuffersToIOVecsForWrite(head, 0, vecs, vecCount) == 1);
		TEST_CHECK(vecs[0].myData == data);
		TEST_CHECK(vecs[0].mySize == 10);

		// Single buffer with offset.
		memset(vecs, 0, sizeof(vecs));
		TEST_CHECK(mg::net::BuffersToIOVecsForWrite(head, 3, vecs, vecCount) == 1);
		TEST_CHECK(vecs[0].myData == data + 3);
		TEST_CHECK(vecs[0].mySize == 7);

		// More buffers.
		mg::net::Buffer* pos = head.GetPointer();
		pos = (pos->myNext = mg::net::BufferRaw::NewShared(
			data + 10, 20, 100)).GetPointer();
		pos->myNext = mg::net::BufferRaw::NewShared(
			data + 500, 300, 400);
		memset(vecs, 0, sizeof(vecs));
		TEST_CHECK(mg::net::BuffersToIOVecsForWrite(head, 3, vecs, vecCount) == 3);
		TEST_CHECK(vecs[0].myData == data + 3);
		TEST_CHECK(vecs[0].mySize == 7);
		TEST_CHECK(vecs[1].myData == data + 10);
		TEST_CHECK(vecs[1].mySize == 20);
		TEST_CHECK(vecs[2].myData == data + 500);
		TEST_CHECK(vecs[2].mySize == 300);

		// More buffers than vectors.
		memset(vecs, 0, sizeof(vecs));
		TEST_CHECK(mg::net::BuffersToIOVecsForWrite(head, 3, vecs, 2) == 2);
		TEST_CHECK(vecs[0].myData == data + 3);
		TEST_CHECK(vecs[0].mySize == 7);
		TEST_CHECK(vecs[1].myData == data + 10);
		TEST_CHECK(vecs[1].mySize == 20);
	}

	static void
	UnitTestBuffersToIOVecsForWriteBufferLinkList()
	{
		TestCaseGuard guard("BuffersToIOVecsForWrite(const BufferLink*)");

		// Single link, single buffer, no offset.
		const uint8_t* data = (const uint8_t*)"1";
		mg::net::BufferLinkList list;
		list.AppendRef(mg::net::BufferRaw::NewShared(data, 10, 30));
		constexpr uint32_t vecCount = 10;
		mg::box::IOVec vecs[vecCount];
		memset(vecs, 0, sizeof(vecs));
		TEST_CHECK(mg::net::BuffersToIOVecsForWrite(list, 0, vecs, vecCount) == 1);
		TEST_CHECK(vecs[0].myData == data);
		TEST_CHECK(vecs[0].mySize == 10);

		// Multiple links and buffers and offset.
		mg::net::BufferLink* link = new mg::net::BufferLink();
		link->myHead = mg::net::BufferRaw::NewShared(data + 10, 100, 200);
		link->myHead->myNext = mg::net::BufferRaw::NewShared(data + 300, 20, 100);
		list.AppendMove(link);

		link = new mg::net::BufferLink();
		link->myHead = mg::net::BufferRaw::NewShared(data + 400, 15, 15);
		link->myHead->myNext = mg::net::BufferRaw::NewShared(data + 500, 1, 1);
		list.AppendMove(link);
		memset(vecs, 0, sizeof(vecs));
		TEST_CHECK(mg::net::BuffersToIOVecsForWrite(list, 7, vecs, vecCount) == 5);
		TEST_CHECK(vecs[0].myData == data + 7);
		TEST_CHECK(vecs[0].mySize == 3);
		TEST_CHECK(vecs[1].myData == data + 10);
		TEST_CHECK(vecs[1].mySize == 100);
		TEST_CHECK(vecs[2].myData == data + 300);
		TEST_CHECK(vecs[2].mySize == 20);
		TEST_CHECK(vecs[3].myData == data + 400);
		TEST_CHECK(vecs[3].mySize == 15);
		TEST_CHECK(vecs[4].myData == data + 500);
		TEST_CHECK(vecs[4].mySize == 1);

		// Vecs end right on first link.
		memset(vecs, 0, sizeof(vecs));
		TEST_CHECK(mg::net::BuffersToIOVecsForWrite(list, 0, vecs, 1) == 1);
		TEST_CHECK(vecs[0].myData == data);
		TEST_CHECK(vecs[0].mySize == 10);

		// Vecs end in the middle.
		memset(vecs, 0, sizeof(vecs));
		TEST_CHECK(mg::net::BuffersToIOVecsForWrite(list, 7, vecs, 2) == 2);
		TEST_CHECK(vecs[0].myData == data + 7);
		TEST_CHECK(vecs[0].mySize == 3);
		TEST_CHECK(vecs[1].myData == data + 10);
		TEST_CHECK(vecs[1].mySize == 100);
	}

	static void
	UnitTestBuffersToIOVecsForRead()
	{
		TestCaseGuard guard("BuffersToIOVecsForRead(Buffer*)");

		// Single buffer, no offset.
		const uint8_t* data = (const uint8_t*)"1";
		mg::net::Buffer::Ptr head = mg::net::BufferRaw::NewShared(data, 0, 30);
		constexpr uint32_t vecCount = 10;
		mg::box::IOVec vecs[vecCount];
		memset(vecs, 0, sizeof(vecs));
		TEST_CHECK(mg::net::BuffersToIOVecsForRead(head, vecs, vecCount) == 1);
		TEST_CHECK(vecs[0].myData == data);
		TEST_CHECK(vecs[0].mySize == 30);

		// Single buffer with offset.
		memset(vecs, 0, sizeof(vecs));
		head->myPos = 10;
		TEST_CHECK(mg::net::BuffersToIOVecsForRead(head, vecs, vecCount) == 1);
		TEST_CHECK(vecs[0].myData == data + 10);
		TEST_CHECK(vecs[0].mySize == 20);

		// More buffers.
		mg::net::Buffer* pos = head.GetPointer();
		pos = (pos->myNext = mg::net::BufferRaw::NewShared(
			data + 10, 0, 100)).GetPointer();
		pos->myNext = mg::net::BufferRaw::NewShared(
			data + 500, 0, 400);
		memset(vecs, 0, sizeof(vecs));
		TEST_CHECK(mg::net::BuffersToIOVecsForRead(head, vecs, vecCount) == 3);
		TEST_CHECK(vecs[0].myData == data + 10);
		TEST_CHECK(vecs[0].mySize == 20);
		TEST_CHECK(vecs[1].myData == data + 10);
		TEST_CHECK(vecs[1].mySize == 100);
		TEST_CHECK(vecs[2].myData == data + 500);
		TEST_CHECK(vecs[2].mySize == 400);

		// More buffers than vectors.
		memset(vecs, 0, sizeof(vecs));
		TEST_CHECK(mg::net::BuffersToIOVecsForRead(head, vecs, 2) == 2);
		TEST_CHECK(vecs[0].myData == data + 10);
		TEST_CHECK(vecs[0].mySize == 20);
		TEST_CHECK(vecs[1].myData == data + 10);
		TEST_CHECK(vecs[1].mySize == 100);
	}
}

	void
	UnitTestBuffer()
	{
		using namespace buffer;
		TestSuiteGuard suite("Buffer");

		UnitTestBufferCopy();
		UnitTestBufferRaw();
		UnitTestBufferPropagate();
		UnitTestBufferLink();

		UnitTestBufferLinkListConstructor();
		UnitTestBufferLinkListAppendRefRawPtr();
		UnitTestBufferLinkListAppendRefPtr();
		UnitTestBufferLinkListAppendRefMovePtr();
		UnitTestBufferLinkListAppendRefData();
		UnitTestBufferLinkListAppendMoveLink();
		UnitTestBufferLinkListAppendMoveLinkList();
		UnitTestBufferLinkListAppendCopyData();
		UnitTestBufferLinkListAppendMany();
		UnitTestBufferLinkListSkipData();
		UnitTestBufferLinkListSkipEmptyPrefix();
		UnitTestBufferLinkListMisc();

		UnitTestBufferStreamPropagateWritePos();
		UnitTestBufferStreamEnsureWriteSize();
		UnitTestBufferStreamSkipData();
		UnitTestBufferStreamReadData();
		UnitTestBufferStreamPeekData();
		UnitTestBufferStreamMisc();

		UnitTestBufferReadStreamSkipData();
		UnitTestBufferReadStreamReadData();
		UnitTestBufferReadStreamPeekData();
		UnitTestBufferReadStreamMisc();

		UnitTestBuffersCopyData();
		UnitTestBuffersCopyBufferList();
		UnitTestBuffersCopyBufferLinkList();
		UnitTestBuffersRefData();
		UnitTestBuffersPropagateOnWrite();
		UnitTestBuffersPropagateOnRead();
		UnitTestBuffersToIOVecsForWriteBufferList();
		UnitTestBuffersToIOVecsForWriteBufferLinkList();
		UnitTestBuffersToIOVecsForRead();
	}

	//////////////////////////////////////////////////////////////////////////////////////
namespace buffer {

	static void
	BufferFill(
		uint64_t aSalt,
		uint8_t* aData,
		uint64_t aOffset,
		uint64_t aSize)
	{
		for (uint64_t i = 0; i < aSize; ++i)
		{
			uint64_t toAdd = (i + aSalt + aOffset) % ('z' - 'a' + 1);
			aData[i] = 'a' + (uint8_t)toAdd;
		}
	}

	static void
	BufferCheck(
		uint64_t aSalt,
		const uint8_t* aData,
		uint64_t aOffset,
		uint64_t aSize)
	{
		for (uint64_t i = 0; i < aSize; ++i)
		{
			uint64_t toAdd = (i + aSalt + aOffset) % ('z' - 'a' + 1);
			TEST_CHECK(aData[i] == 'a' + (uint8_t)toAdd);
		}
	}

}
}
}
}

#if IS_COMPILER_GCC
#pragma GCC diagnostic pop
#endif
