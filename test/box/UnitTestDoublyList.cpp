#include "mg/box/DoublyList.h"

#include "UnitTest.h"

#include <utility>

namespace mg {
namespace unittests {
namespace box {
namespace doublylist {

	struct Value
	{
		Value()
			: myValue(-1)
			// Garbage pointer to ensure it won't break anything.
			, myPrev((Value*)0x123)
			, myNext((Value*)0x123)
		{
		}

		Value(
			int aValue)
			: myValue(aValue)
			, myPrev((Value*)0x123)
			, myNext((Value*)0x123)
		{
		}

		int myValue;
		Value* myPrev;
		Value* myNext;
	};

	using ValueList = mg::box::DoublyList<Value>;

	//////////////////////////////////////////////////////////////////////////////////////

	static void
	CheckLinksRecursive(
		Value* aFirst,
		Value* aSecond)
	{
		TEST_CHECK(aFirst->myNext == aSecond);
		TEST_CHECK(aSecond->myPrev == aFirst);
		TEST_CHECK(aSecond->myNext == nullptr);
	}

	template<typename... Args>
	static void
	CheckLinksRecursive(
		Value* aFirst,
		Value* aSecond,
		Args... aArgs)
	{
		TEST_CHECK(aFirst->myNext == aSecond);
		TEST_CHECK(aSecond->myPrev == aFirst);
		CheckLinksRecursive(aSecond, aArgs...);
	}

	template<typename... Args>
	static void
	CheckLinks(
		Value* aFirst,
		Args... aArgs)
	{
		TEST_CHECK(aFirst->myPrev == nullptr);
		CheckLinksRecursive(aFirst, aArgs...);
	}

	static void
	CheckLinks(
		Value* aFirst)
	{
		TEST_CHECK(aFirst->myPrev == nullptr);
		TEST_CHECK(aFirst->myNext == nullptr);
	}

	//////////////////////////////////////////////////////////////////////////////////////

	static void
	MakeLinksRecursive(
		Value* aFirst,
		Value* aSecond)
	{
		aFirst->myNext = aSecond;
		aSecond->myPrev = aFirst;
		aSecond->myNext = nullptr;
	}

	template<typename... Args>
	static void
	MakeLinksRecursive(
		Value* aFirst,
		Value* aSecond,
		Args... aArgs)
	{
		aFirst->myNext = aSecond;
		aSecond->myPrev = aFirst;
		MakeLinksRecursive(aSecond, aArgs...);
	}

	template<typename... Args>
	static void
	MakeLinks(
		Value* aFirst,
		Args... aArgs)
	{
		aFirst->myPrev = nullptr;
		MakeLinksRecursive(aFirst, aArgs...);
	}

	//////////////////////////////////////////////////////////////////////////////////////

	static void
	UnitTestDoublyListBasic()
	{
		TestCaseGuard guard("Basic");

		// Empty list.
		{
			ValueList list;
			const ValueList* clist = &list;
			TEST_CHECK(list.GetFirst() == nullptr);
			TEST_CHECK(clist->GetFirst() == nullptr);
			TEST_CHECK(list.GetLast() == nullptr);
			TEST_CHECK(clist->GetLast() == nullptr);
			TEST_CHECK(list.IsEmpty());
			list.Clear();
			list.Reverse();
		}
		// Has 1 element.
		{
			ValueList list;
			const ValueList* clist = &list;
			Value v(1);

			list.Append(&v);
			CheckLinks(&v);
			TEST_CHECK(list.GetFirst() == &v);
			TEST_CHECK(clist->GetFirst() == &v);
			TEST_CHECK(list.GetLast() == &v);
			TEST_CHECK(clist->GetLast() == &v);
			TEST_CHECK(!list.IsEmpty());

			list.Reverse();
			CheckLinks(&v);
			TEST_CHECK(list.GetFirst() == &v);

			list.Clear();
			CheckLinks(&v);
			TEST_CHECK(list.IsEmpty());
			TEST_CHECK(list.GetFirst() == nullptr);

			list.Prepend(&v);
			CheckLinks(&v);
			TEST_CHECK(list.GetFirst() == &v);
			TEST_CHECK(list.PopFirst() == &v);
		}
		// Many elements.
		{
			ValueList list;
			const ValueList* clist = &list;
			Value v1(1);
			Value v2(2);

			list.Append(&v1);
			CheckLinks(&v1);
			list.Append(&v2);
			CheckLinks(&v1, &v2);

			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(clist->GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v2);
			TEST_CHECK(clist->GetLast() == &v2);
			TEST_CHECK(!list.IsEmpty());

			Value v0(0);
			list.Prepend(&v0);
			CheckLinks(&v0, &v1, &v2);

			TEST_CHECK(list.GetFirst() == &v0);
			TEST_CHECK(list.GetLast() == &v2);

			list.Reverse();
			CheckLinks(&v2, &v1, &v0);

			TEST_CHECK(list.GetFirst() == &v2);
			TEST_CHECK(list.GetLast() == &v0);

			list.Clear();
			CheckLinks(&v2, &v1, &v0);
			TEST_CHECK(list.IsEmpty());
		}
		// Construct from a plain empty list.
		{
			ValueList list(nullptr, nullptr);
			Value v1(1);
			TEST_CHECK(list.IsEmpty());
			list.Append(&v1);
			CheckLinks(&v1);
			TEST_CHECK(!list.IsEmpty());
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v1);
		}
		// Construct from a plain one-element list.
		{
			Value v1(1);
			v1.myNext = &v1;
			ValueList list(&v1, &v1);
			CheckLinks(&v1);
			TEST_CHECK(!list.IsEmpty());
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v1);
		}
		// Construct from a plain two-element list.
		{
			Value v1(1);
			Value v2(2);
			MakeLinks(&v1, &v2);
			ValueList list(&v1, &v2);
			CheckLinks(&v1, &v2);
			TEST_CHECK(!list.IsEmpty());
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v2);
		}
		// Construct from a list object.
		{
			Value v1(1);
			Value v2(2);
			MakeLinks(&v1, &v2);
			ValueList list1(&v1, &v2);
			ValueList list2(std::move(list1));

			TEST_CHECK(list1.IsEmpty());
			CheckLinks(&v1, &v2);
			TEST_CHECK(!list2.IsEmpty());
			TEST_CHECK(list2.GetFirst() == &v1);
			TEST_CHECK(list2.GetLast() == &v2);
		}
		// Pop with tail.
		{
			ValueList list;
			Value v1(1);
			Value v2(2);
			Value v3(3);
			list.Append(&v1);
			list.Append(&v2);
			list.Append(&v3);
			Value* tail = &v1;
			Value* head = list.PopAll(tail);
			CheckLinks(&v1, &v2, &v3);
			TEST_CHECK(list.IsEmpty());
			TEST_CHECK(tail == &v3);
			TEST_CHECK(head == &v1);
			TEST_CHECK(list.GetFirst() == nullptr);
			TEST_CHECK(list.GetLast() == nullptr);
		}
	}

	static void
	UnitTestDoublyListPrepend()
	{
		TestCaseGuard guard("Prepend");

		// Prepend multiple.
		{
			ValueList list;
			Value v1(1);
			Value v2(2);
			Value v3(3);

			list.Prepend(&v3);
			CheckLinks(&v3);
			list.Prepend(&v2);
			CheckLinks(&v2, &v3);
			list.Prepend(&v1);
			CheckLinks(&v1, &v2, &v3);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v3);
		}
		// Prepend a plain list.
		{
			ValueList list;
			Value v1(1);
			Value v2(2);

			list.Prepend(nullptr, nullptr);
			TEST_CHECK(list.IsEmpty());
			list.Append(&v1);
			TEST_CHECK(list.PopFirst() == &v1);

			v1.myNext = &v1;
			list.Prepend(&v1, &v1);
			TEST_CHECK(!list.IsEmpty());
			CheckLinks(&v1);
			TEST_CHECK(list.PopFirst() == &v1);
			TEST_CHECK(list.IsEmpty());

			MakeLinks(&v1, &v2);
			list.Prepend(&v1, &v2);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v2);
			CheckLinks(&v1, &v2);
			TEST_CHECK(list.PopFirst() == &v1);
			TEST_CHECK(list.PopFirst() == &v2);
			TEST_CHECK(list.GetFirst() == nullptr);
			TEST_CHECK(list.GetLast() == nullptr);

			Value v3(3);
			Value v4(4);
			MakeLinks(&v3, &v4);
			list.Prepend(&v3, &v4);
			CheckLinks(&v3, &v4);

			MakeLinks(&v1, &v2);
			list.Prepend(&v1, &v2);
			CheckLinks(&v1, &v2, &v3, &v4);

			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v4);
		}
		// Prepend a list object.
		{
			ValueList list1;
			ValueList list2;

			list1.Prepend(std::move(list2));
			TEST_CHECK(list1.IsEmpty());
			TEST_CHECK(list2.IsEmpty());

			Value v1(1);
			list2.Append(&v1);
			list1.Prepend(std::move(list2));
			TEST_CHECK(list1.GetFirst() == &v1);
			TEST_CHECK(list1.GetLast() == &v1);
			TEST_CHECK(list2.IsEmpty());
			CheckLinks(&v1);

			Value v2(2);
			Value v3(3);
			list2.Append(&v2);
			list2.Append(&v3);
			list1.Prepend(std::move(list2));
			TEST_CHECK(list2.IsEmpty());
			TEST_CHECK(list1.GetFirst() == &v2);
			TEST_CHECK(list1.GetLast() == &v1);
			CheckLinks(&v2, &v3, &v1);

			Value v4(4);
			Value v5(5);
			list2.Append(&v4);
			list2.Append(&v5);
			list1.Prepend(std::move(list2));
			TEST_CHECK(list2.IsEmpty());
			TEST_CHECK(list1.GetFirst() == &v4);
			TEST_CHECK(list1.GetLast() == &v1);
			CheckLinks(&v4, &v5, &v2, &v3, &v1);

			list1.Clear();
			MakeLinks(&v1, &v2);
			list2.Append(&v1, &v2);
			list1.Prepend(std::move(list2));
			TEST_CHECK(list1.GetFirst() == &v1);
			TEST_CHECK(list1.GetLast() == &v2);
			CheckLinks(&v1, &v2);
		}
	}

	static void
	UnitTestDoublyListAppend()
	{
		TestCaseGuard guard("Append");

		// Append plain list.
		{
			ValueList list;
			Value v1(1);
			Value v2(2);
			MakeLinks(&v1, &v2);

			list.Append(&v1, &v2);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v2);
			CheckLinks(&v1, &v2);

			list.Clear();
			list.Append(&v1, &v1);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v1);
			CheckLinks(&v1);

			list.Clear();
			Value v3(3);
			MakeLinks(&v1, &v2);
			list.Append(&v1, &v2);
			list.Append(&v3, &v3);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v3);
			CheckLinks(&v1, &v2, &v3);

			list.Clear();
			Value v4(4);
			MakeLinks(&v1, &v2);
			MakeLinks(&v3, &v4);
			// Set to a bad value to see that it is nullified.
			v4.myNext = &v1;
			list.Append(&v1, &v2);
			list.Append(&v3, &v4);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v4);
			CheckLinks(&v1, &v2, &v3, &v4);

			list.Append(nullptr, nullptr);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v4);
		}
		// Append a list object.
		{
			ValueList list1;
			ValueList list2;
			Value v1(1);

			list1.Append(std::move(list2));
			TEST_CHECK(list1.IsEmpty());
			TEST_CHECK(list2.IsEmpty());

			list2.Append(&v1);
			list1.Append(std::move(list2));
			TEST_CHECK(list1.GetFirst() == &v1);
			TEST_CHECK(list1.GetLast() == &v1);
			TEST_CHECK(list2.IsEmpty());

			Value v2(2);
			Value v3(2);
			list2.Append(&v2);
			list2.Append(&v3);
			list1.Append(std::move(list2));
			TEST_CHECK(list1.GetFirst() == &v1);
			TEST_CHECK(list1.GetLast() == &v3);
			TEST_CHECK(list2.IsEmpty());
			CheckLinks(&v1, &v2, &v3);

			list2.Append(std::move(list1));
			TEST_CHECK(list1.IsEmpty());
			TEST_CHECK(list2.GetFirst() == &v1);
			TEST_CHECK(list2.GetLast() == &v3);
			CheckLinks(&v1, &v2, &v3);
		}
	}

	static void
	UnitTestDoublyListRemove()
	{
		TestCaseGuard guard("Remove");

		// Remove from single element list.
		ValueList list;
		Value v1(1);
		list.Append(&v1);
		list.Remove(&v1);
		TEST_CHECK(list.IsEmpty());
		TEST_CHECK(list.GetFirst() == nullptr);
		TEST_CHECK(list.GetLast() == nullptr);
		CheckLinks(&v1);

		// Remove first from 2 elements.
		Value v2(2);
		list.Append(&v1);
		list.Append(&v2);
		list.Remove(&v1);
		TEST_CHECK(list.GetFirst() == &v2);
		TEST_CHECK(list.GetLast() == &v2);
		CheckLinks(&v1);
		CheckLinks(&v2);

		// Remove first from 3 elements.
		list.Clear();
		Value v3(3);
		MakeLinks(&v1, &v2, &v3);
		list.Append(&v1, &v3);
		list.Remove(&v1);
		TEST_CHECK(list.GetFirst() == &v2);
		TEST_CHECK(list.GetLast() == &v3);
		CheckLinks(&v1);
		CheckLinks(&v2, &v3);

		// Remove last from 2 elements.
		list.Clear();
		list.Append(&v1);
		list.Append(&v2);
		list.Remove(&v2);
		TEST_CHECK(list.GetFirst() == &v1);
		TEST_CHECK(list.GetLast() == &v1);
		CheckLinks(&v1);
		CheckLinks(&v2);

		// Remove middle from 3 elements.
		list.Clear();
		MakeLinks(&v1, &v2, &v3);
		list.Append(&v1, &v3);
		list.Remove(&v2);
		TEST_CHECK(list.GetFirst() == &v1);
		TEST_CHECK(list.GetLast() == &v3);
		CheckLinks(&v1, &v3);
		CheckLinks(&v2);

		// Remove last from 3 elements.
		list.Clear();
		MakeLinks(&v1, &v2, &v3);
		list.Append(&v1, &v3);
		list.Remove(&v3);
		TEST_CHECK(list.GetFirst() == &v1);
		TEST_CHECK(list.GetLast() == &v2);
		CheckLinks(&v1, &v2);
		CheckLinks(&v3);
	}

	static void
	UnitTestDoublyListPopFirst()
	{
		TestCaseGuard guard("PopFirst");

		// Pop from 1 size list.
		ValueList list;
		Value v1(1);
		list.Append(&v1);
		CheckLinks(&v1);
		Value* v = list.PopFirst();
		TEST_CHECK(v == &v1);
		TEST_CHECK(list.GetFirst() == nullptr);
		TEST_CHECK(list.GetLast() == nullptr);
		CheckLinks(&v1);

		// Pop from 2 size list.
		Value v2(2);
		list.Clear();
		list.Append(&v1);
		list.Append(&v2);
		CheckLinks(&v1, &v2);
		v = list.PopFirst();
		TEST_CHECK(v == &v1);
		TEST_CHECK(list.GetFirst() == &v2);
		TEST_CHECK(list.GetLast() == &v2);
		CheckLinks(&v1);
		CheckLinks(&v2);

		Value v3(3);
		list.Clear();
		MakeLinks(&v1, &v2, &v3);
		list.Append(&v1, &v3);
		v = list.PopFirst();
		TEST_CHECK(v == &v1);
		TEST_CHECK(list.GetFirst() == &v2);
		TEST_CHECK(list.GetLast() == &v3);
		CheckLinks(&v1);
		CheckLinks(&v2, &v3);
	}

	static void
	UnitTestDoublyListPopLast()
	{
		TestCaseGuard guard("PopLast");

		// Pop from 1 size list.
		ValueList list;
		Value v1(1);
		list.Append(&v1);
		CheckLinks(&v1);
		Value* v = list.PopLast();
		TEST_CHECK(v == &v1);
		TEST_CHECK(list.GetFirst() == nullptr);
		TEST_CHECK(list.GetLast() == nullptr);
		CheckLinks(&v1);

		// Pop from 2 size list.
		Value v2(2);
		list.Clear();
		list.Append(&v1);
		list.Append(&v2);
		CheckLinks(&v1, &v2);
		v = list.PopLast();
		TEST_CHECK(v == &v2);
		TEST_CHECK(list.GetFirst() == &v1);
		TEST_CHECK(list.GetLast() == &v1);
		CheckLinks(&v1);
		CheckLinks(&v2);

		Value v3(3);
		list.Clear();
		MakeLinks(&v1, &v2, &v3);
		list.Append(&v1, &v3);
		v = list.PopLast();
		TEST_CHECK(v == &v3);
		TEST_CHECK(list.GetFirst() == &v1);
		TEST_CHECK(list.GetLast() == &v2);
		CheckLinks(&v1, &v2);
		CheckLinks(&v3);
	}

	static void
	UnitTestDoublyListAssign()
	{
		TestCaseGuard guard("Assign");

		// Assign from a list object.
		ValueList list1;
		ValueList list2;
		Value v1(1);

		list1 = std::move(list2);
		TEST_CHECK(list1.IsEmpty());
		TEST_CHECK(list2.IsEmpty());

		list1.Append(&v1);
		list1 = std::move(list2);
		TEST_CHECK(list1.IsEmpty());
		TEST_CHECK(list2.IsEmpty());
		CheckLinks(&v1);

		Value v2(2);
		list1.Append(&v1);
		list2.Append(&v2);
		list1 = std::move(list2);
		TEST_CHECK(list1.GetFirst() == &v2);
		TEST_CHECK(list1.GetLast() == &v2);
		TEST_CHECK(list2.IsEmpty());
		CheckLinks(&v1);
		CheckLinks(&v2);

		Value v3(3);
		list1.Clear();
		list1.Append(&v1);
		list2.Append(&v2);
		list2.Append(&v3);
		list1 = std::move(list2);
		TEST_CHECK(list2.IsEmpty());
		TEST_CHECK(list1.GetFirst() == &v2);
		TEST_CHECK(list1.GetLast() == &v3);
		CheckLinks(&v1);
		CheckLinks(&v2, &v3);

		list1.Clear();
		list1.Append(&v1);
		list1.Append(&v2);
		list2.Append(&v3);
		list1 = std::move(list2);
		TEST_CHECK(list2.IsEmpty());
		TEST_CHECK(list1.GetFirst() == &v3);
		TEST_CHECK(list1.GetLast() == &v3);
		CheckLinks(&v1, &v2);
		CheckLinks(&v3);
	}

	struct Value2
	{
		Value2()
			: myValue(-1)
			, myPrev2((Value2*)0x123)
			, myNext2((Value2*)0x123)
		{
		}

		Value2(
			int aValue)
			: myValue(aValue)
			, myPrev2((Value2*)0x123)
			, myNext2((Value2*)0x123)
		{
		}

		int myValue;
		Value2* myPrev2;
		Value2* myNext2;
	};

	using ValueList2 = mg::box::DoublyList<Value2, &Value2::myPrev2, &Value2::myNext2>;

	static void
	UnitTestDoublyListDifferentLink()
	{
		TestCaseGuard guard("Different link name");

		ValueList2 list;
		Value2 v1(1);
		Value2 v2(2);
		list.Append(&v1);
		list.Append(&v2);
		TEST_CHECK(list.GetFirst() == &v1);
		TEST_CHECK(list.GetLast() == &v2);
		TEST_CHECK(v1.myPrev2 == nullptr);
		TEST_CHECK(v1.myNext2 == &v2);
		TEST_CHECK(v2.myPrev2 == &v1);
		TEST_CHECK(v2.myNext2 == nullptr);
		TEST_CHECK(list.PopFirst() == &v1);
		TEST_CHECK(list.PopLast() == &v2);
	}

	static void
	UnitTestDoublyListIterator()
	{
		TestCaseGuard guard("DoublyListIterator");

		ValueList list;
		for (Value* val : list)
		{
			MG_UNUSED(val);
			TEST_CHECK(false);
		}

		Value v1(1);
		list.Append(&v1);
		int count = 0;
		for (Value* val : list)
		{
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 1);

		count = 0;
		for (Value* val : list)
		{
			TEST_CHECK(val == list.PopFirst());
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 1);

		Value v2(2);
		list.Append(&v1);
		list.Append(&v2);
		count = 0;
		for (Value* val : list)
		{
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 2);

		count = 0;
		for (Value* val : list)
		{
			TEST_CHECK(val == list.PopFirst());
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 2);

		Value v3(3);
		list.Append(&v1);
		list.Append(&v2);
		list.Append(&v3);
		count = 0;
		for (Value* val : list)
		{
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 3);

		count = 0;
		for (Value* val : list)
		{
			TEST_CHECK(val == list.PopFirst());
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 3);

		Value v4(4);
		MakeLinks(&v1, &v2, &v3, &v4);
		list.Append(&v1, &v4);
		count = 0;
		for (Value* val : list)
		{
			if (val->myValue % 2 == 0)
				list.Remove(val);
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 4);
		CheckLinks(&v1, &v3);
		CheckLinks(&v2);
		CheckLinks(&v4);
		TEST_CHECK(list.GetFirst() == &v1);
		TEST_CHECK(list.GetLast() == &v3);
	}

	static void
	UnitTestDoublyListConstIterator()
	{
		TestCaseGuard guard("DoublyListConstIterator");

		ValueList list;
		const ValueList& clist = list;
		for (const Value* val : clist)
		{
			MG_UNUSED(val);
			TEST_CHECK(false);
		}

		Value v1(1);
		list.Append(&v1);
		int count = 0;
		for (const Value* val : clist)
		{
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 1);

		Value v2(2);
		list.Append(&v1);
		list.Append(&v2);
		count = 0;
		for (const Value* val : clist)
		{
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 2);

		Value v3(3);
		list.Append(&v1);
		list.Append(&v2);
		list.Append(&v3);
		count = 0;
		for (const Value* val : clist)
		{
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 3);
	}
}

	void
	UnitTestDoublyList()
	{
		using namespace doublylist;
		TestSuiteGuard suite("DoublyList");

		UnitTestDoublyListBasic();
		UnitTestDoublyListPrepend();
		UnitTestDoublyListAppend();
		UnitTestDoublyListRemove();
		UnitTestDoublyListPopFirst();
		UnitTestDoublyListPopLast();
		UnitTestDoublyListAssign();
		UnitTestDoublyListDifferentLink();
		UnitTestDoublyListIterator();
		UnitTestDoublyListConstIterator();
	}

}
}
}
