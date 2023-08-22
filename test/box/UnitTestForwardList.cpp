#include "mg/box/ForwardList.h"

#include "UnitTest.h"

#include <utility>

namespace mg {
namespace unittests {
namespace box {

	struct UTFLValue
	{
		UTFLValue()
			: myValue(-1)
			// Garbage pointer to ensure it is nullified when the
			// element is added to a list.
			, myNext((UTFLValue*)0x123)
		{
		}

		UTFLValue(
			int aValue)
			: myValue(aValue)
			, myNext((UTFLValue*)0x123)
		{
		}

		int myValue;
		UTFLValue* myNext;
	};

	using UTFLList = mg::box::ForwardList<UTFLValue>;

	static void
	UnitTestForwardListBasic()
	{
		// Empty list.
		{
			UTFLList list;
			const UTFLList* clist = &list;
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
			UTFLList list;
			const UTFLList* clist = &list;
			UTFLValue v(1);

			list.Append(&v);
			TEST_CHECK(v.myNext == nullptr);
			TEST_CHECK(list.GetFirst() == &v);
			TEST_CHECK(clist->GetFirst() == &v);
			TEST_CHECK(list.GetLast() == &v);
			TEST_CHECK(clist->GetLast() == &v);
			TEST_CHECK(!list.IsEmpty());

			list.Reverse();
			TEST_CHECK(list.GetFirst() == &v);

			list.Clear();
			TEST_CHECK(list.IsEmpty());
			TEST_CHECK(list.GetFirst() == nullptr);

			list.Prepend(&v);
			TEST_CHECK(list.GetFirst() == &v);
			TEST_CHECK(list.PopFirst() == &v);
		}
		// Many elements.
		{
			UTFLList list;
			const UTFLList* clist = &list;
			UTFLValue v1(1);
			UTFLValue v2(2);
			// Fill with garbage to check the links are nullified.
			v1.myNext = (UTFLValue*) 0x123;
			v2.myNext = v1.myNext;

			list.Append(&v1);
			TEST_CHECK(v1.myNext == nullptr);
			list.Append(&v2);

			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(clist->GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v2);
			TEST_CHECK(clist->GetLast() == &v2);
			TEST_CHECK(!list.IsEmpty());
			TEST_CHECK(v1.myNext == &v2);
			TEST_CHECK(v2.myNext == nullptr);

			UTFLValue v0(0);
			list.Prepend(&v0);
			TEST_CHECK(list.GetFirst() == &v0);
			TEST_CHECK(list.GetLast() == &v2);
			TEST_CHECK(v0.myNext == &v1);
			TEST_CHECK(v1.myNext == &v2);
			TEST_CHECK(v2.myNext == nullptr);

			list.Reverse();
			TEST_CHECK(list.GetFirst() == &v2);
			TEST_CHECK(list.GetLast() == &v0);
			TEST_CHECK(v2.myNext == &v1);
			TEST_CHECK(v1.myNext == &v0);
			TEST_CHECK(v0.myNext == nullptr);

			list.Clear();
			TEST_CHECK(list.IsEmpty());
		}
		// Construct from a plain empty list.
		{
			UTFLList list(nullptr, nullptr);
			UTFLValue v1(1);
			TEST_CHECK(list.IsEmpty());
			list.Append(&v1);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v1);
		}
		// Construct from a plain one-element list.
		{
			UTFLValue v1(1);
			v1.myNext = &v1;
			UTFLList list(&v1, &v1);
			TEST_CHECK(v1.myNext == nullptr);
			TEST_CHECK(!list.IsEmpty());
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v1);
		}
		// Construct from a plain two-element list.
		{
			UTFLValue v1(1);
			UTFLValue v2(2);
			v1.myNext = &v2;
			UTFLList list(&v1, &v2);
			TEST_CHECK(v1.myNext == &v2);
			TEST_CHECK(v2.myNext == nullptr);
			TEST_CHECK(!list.IsEmpty());
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v2);
		}
		// Construct from a list object.
		{
			UTFLValue v1(1);
			UTFLValue v2(2);
			v1.myNext = &v2;
			UTFLList list1(&v1, &v2);
			UTFLList list2(std::move(list1));

			TEST_CHECK(list1.IsEmpty());
			TEST_CHECK(v1.myNext == &v2);
			TEST_CHECK(v2.myNext == nullptr);
			TEST_CHECK(!list2.IsEmpty());
			TEST_CHECK(list2.GetFirst() == &v1);
			TEST_CHECK(list2.GetLast() == &v2);
		}
		// Pop with tail.
		{
			UTFLList list;
			UTFLValue v1(1);
			UTFLValue v2(2);
			UTFLValue v3(3);
			list.Append(&v1);
			list.Append(&v2);
			list.Append(&v3);
			UTFLValue* tail = &v1;
			UTFLValue* head = list.PopAll(tail);
			TEST_CHECK(list.IsEmpty());
			TEST_CHECK(tail == &v3);
			TEST_CHECK(head == &v1);
			TEST_CHECK(list.GetFirst() == nullptr);
			TEST_CHECK(list.GetLast() == nullptr);
		}
	}

	static void
	UnitTestForwardListAppend()
	{
		// Append plain list.
		{
			UTFLList list;
			UTFLValue v1(1);
			UTFLValue v2(2);
			v1.myNext = &v2;

			list.Append(&v1, &v2);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v2);
			TEST_CHECK(v1.myNext == &v2);

			list.Clear();
			list.Append(&v1, &v1);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v1);
			TEST_CHECK(v1.myNext == nullptr);

			list.Clear();
			UTFLValue v3(3);

			v1.myNext = &v2;
			list.Append(&v1, &v2);
			list.Append(&v3, &v3);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v3);
			TEST_CHECK(v1.myNext == &v2);
			TEST_CHECK(v2.myNext == &v3);
			TEST_CHECK(v3.myNext == nullptr);

			list.Clear();
			UTFLValue v4(4);
			v1.myNext = &v2;
			v3.myNext = &v4;
			// Set to a bad value to see that it is nullified.
			v4.myNext = &v1;
			list.Append(&v1, &v2);
			list.Append(&v3, &v4);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v4);
			TEST_CHECK(v1.myNext == &v2);
			TEST_CHECK(v2.myNext == &v3);
			TEST_CHECK(v3.myNext == &v4);
			TEST_CHECK(v4.myNext == nullptr);

			list.Append(nullptr, nullptr);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v4);
		}
		// Append a list object.
		{
			UTFLList list1;
			UTFLList list2;
			UTFLValue v1(1);

			list1.Append(std::move(list2));
			TEST_CHECK(list1.IsEmpty());
			TEST_CHECK(list2.IsEmpty());

			list2.Append(&v1);
			list1.Append(std::move(list2));
			TEST_CHECK(list1.GetFirst() == &v1);
			TEST_CHECK(list1.GetLast() == &v1);
			TEST_CHECK(list2.IsEmpty());

			UTFLValue v2(2);
			UTFLValue v3(2);
			list2.Append(&v2);
			list2.Append(&v3);
			list1.Append(std::move(list2));
			TEST_CHECK(list1.GetFirst() == &v1);
			TEST_CHECK(list1.GetLast() == &v3);
			TEST_CHECK(list2.IsEmpty());
		}
	}

	static void
	UnitTestForwardListPrepend()
	{
		// Prepend multiple.
		{
			UTFLList list;
			UTFLValue v1(1);
			UTFLValue v2(2);

			list.Prepend(&v2);
			list.Prepend(&v1);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v2);
			TEST_CHECK(v1.myNext == &v2);
			TEST_CHECK(v2.myNext == nullptr);
		}
		// Prepend a plain list.
		{
			UTFLList list;
			UTFLValue v1(1);
			UTFLValue v2(2);

			list.Prepend(nullptr, nullptr);
			TEST_CHECK(list.IsEmpty());
			list.Append(&v1);
			TEST_CHECK(list.PopFirst() == &v1);

			v1.myNext = &v1;
			list.Prepend(&v1, &v1);
			TEST_CHECK(!list.IsEmpty());
			TEST_CHECK(v1.myNext == nullptr);
			TEST_CHECK(list.PopFirst() == &v1);
			TEST_CHECK(list.IsEmpty());

			v1.myNext = &v2;
			list.Prepend(&v1, &v2);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v2);
			TEST_CHECK(list.PopFirst() == &v1);
			TEST_CHECK(list.PopFirst() == &v2);
			TEST_CHECK(list.GetFirst() == nullptr);
			TEST_CHECK(list.GetLast() == nullptr);
		}
		// Prepend a list object.
		{
			UTFLList list1;
			UTFLList list2;

			list1.Prepend(std::move(list2));
			TEST_CHECK(list1.IsEmpty());
			TEST_CHECK(list2.IsEmpty());

			UTFLValue v1(1);
			list2.Append(&v1);
			list1.Prepend(std::move(list2));
			TEST_CHECK(list1.GetFirst() == &v1);
			TEST_CHECK(list1.GetLast() == &v1);
			TEST_CHECK(list2.IsEmpty());

			UTFLValue v2(2);
			UTFLValue v3(3);
			list2.Append(&v2);
			list2.Append(&v3);
			list1.Prepend(std::move(list2));
			TEST_CHECK(list2.IsEmpty());
			TEST_CHECK(list1.GetFirst() == &v2);
			TEST_CHECK(list1.GetLast() == &v1);
			TEST_CHECK(v2.myNext == &v3);
			TEST_CHECK(v3.myNext == &v1);
			TEST_CHECK(v1.myNext == nullptr);
		}
	}

	static void
	UnitTestForwardListInsert()
	{
		// Insert into empty.
		{
			UTFLList list;
			UTFLValue v1(1);

			list.Insert(nullptr, &v1);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v1);
			TEST_CHECK(v1.myNext == nullptr);
		}
		// Insert before first.
		{
			UTFLList list;
			UTFLValue v1(1);
			UTFLValue v2(2);
			list.Insert(nullptr, &v2);
			list.Insert(nullptr, &v1);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v2);
			TEST_CHECK(v2.myNext == nullptr);
		}
		// Insert in the middle first.
		{
			UTFLList list;
			UTFLValue v1(1);
			UTFLValue v2(2);
			UTFLValue v3(3);
			list.Append(&v1);
			list.Append(&v3);
			list.Insert(&v1, &v2);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v3);
			TEST_CHECK(v1.myNext == &v2);
			TEST_CHECK(v2.myNext == &v3);
			TEST_CHECK(v3.myNext == nullptr);
		}
		// Insert in the end.
		{
			UTFLList list;
			UTFLValue v1(1);
			UTFLValue v2(2);
			UTFLValue v3(3);
			list.Append(&v1);
			list.Append(&v2);
			list.Insert(&v2, &v3);
			TEST_CHECK(list.GetFirst() == &v1);
			TEST_CHECK(list.GetLast() == &v3);
			TEST_CHECK(v1.myNext == &v2);
			TEST_CHECK(v2.myNext == &v3);
			TEST_CHECK(v3.myNext == nullptr);
		}
	}

	static void
	UnitTestForwardListAssign()
	{
		// Assign from a list object.
		{
			UTFLList list1;
			UTFLList list2;
			UTFLValue v1(1);

			list1 = std::move(list2);
			TEST_CHECK(list1.IsEmpty());
			TEST_CHECK(list2.IsEmpty());

			list1.Append(&v1);
			list1 = std::move(list2);
			TEST_CHECK(list1.IsEmpty());
			TEST_CHECK(list2.IsEmpty());

			UTFLValue v2(2);
			list1.Append(&v1);
			list2.Append(&v2);
			list1 = std::move(list2);
			TEST_CHECK(list1.GetFirst() == &v2);
			TEST_CHECK(list1.GetLast() == &v2);
			TEST_CHECK(list2.IsEmpty());

			UTFLValue v3(3);
			list1.Clear();
			list1.Append(&v1);
			list2.Append(&v2);
			list2.Append(&v3);
			list1 = std::move(list2);
			TEST_CHECK(list2.IsEmpty());
			TEST_CHECK(list1.GetFirst() == &v2);
			TEST_CHECK(list1.GetLast() == &v3);
			TEST_CHECK(v2.myNext == &v3);
			TEST_CHECK(v3.myNext == nullptr);

			list1.Clear();
			list1.Append(&v1);
			list1.Append(&v2);
			list2.Append(&v3);
			list1 = std::move(list2);
			TEST_CHECK(list2.IsEmpty());
			TEST_CHECK(list1.GetFirst() == &v3);
			TEST_CHECK(list1.GetLast() == &v3);
			TEST_CHECK(v3.myNext == nullptr);
		}
	}

	struct UTFLValue2
	{
		UTFLValue2()
			: myValue(-1)
			, myNext2((UTFLValue2*)0x123)
		{
		}

		UTFLValue2(
			int aValue)
			: myValue(aValue)
			, myNext2((UTFLValue2*)0x123)
		{
		}

		int myValue;
		UTFLValue2* myNext2;
	};

	using UTFLList2 = mg::box::ForwardList<UTFLValue2, &UTFLValue2::myNext2>;

	static void
	UnitTestForwardListDifferentLink()
	{
		UTFLList2 list2;
		UTFLValue2 v1(1);
		UTFLValue2 v2(2);
		list2.Append(&v1);
		list2.Append(&v2);
		TEST_CHECK(list2.GetFirst() == &v1);
		TEST_CHECK(list2.GetLast() == &v2);
		TEST_CHECK(v1.myNext2 == &v2);
		TEST_CHECK(v2.myNext2 == nullptr);
	}

	static void
	UnitTestForwardListIterator()
	{
		UTFLList list;
		for (UTFLValue* val : list)
		{
			MG_UNUSED(val);
			TEST_CHECK(false);
		}

		UTFLValue v1(1);
		list.Append(&v1);
		int count = 0;
		for (UTFLValue* val : list)
		{
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 1);

		count = 0;
		for (UTFLValue* val : list)
		{
			TEST_CHECK(val == list.PopFirst());
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 1);

		UTFLValue v2(2);
		list.Append(&v1);
		list.Append(&v2);
		count = 0;
		for (UTFLValue* val : list)
		{
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 2);

		count = 0;
		for (UTFLValue* val : list)
		{
			TEST_CHECK(val == list.PopFirst());
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 2);

		UTFLValue v3(3);
		list.Append(&v1);
		list.Append(&v2);
		list.Append(&v3);
		count = 0;
		for (UTFLValue* val : list)
		{
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 3);

		count = 0;
		for (UTFLValue* val : list)
		{
			TEST_CHECK(val == list.PopFirst());
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 3);
	}

	static void
	UnitTestForwardListConstIterator()
	{
		UTFLList list;
		const UTFLList& clist = list;
		for (const UTFLValue* val : clist)
		{
			MG_UNUSED(val);
			TEST_CHECK(false);
		}

		UTFLValue v1(1);
		list.Append(&v1);
		int count = 0;
		for (const UTFLValue* val : clist)
		{
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 1);

		UTFLValue v2(2);
		list.Append(&v1);
		list.Append(&v2);
		count = 0;
		for (const UTFLValue* val : clist)
		{
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 2);

		UTFLValue v3(3);
		list.Append(&v1);
		list.Append(&v2);
		list.Append(&v3);
		count = 0;
		for (const UTFLValue* val : clist)
		{
			++count;
			TEST_CHECK(val->myValue == count);
		}
		TEST_CHECK(count == 3);
	}

	void
	UnitTestForwardList()
	{
		TestSuiteGuard suite("ForwardList");

		UnitTestForwardListBasic();
		UnitTestForwardListAppend();
		UnitTestForwardListPrepend();
		UnitTestForwardListInsert();
		UnitTestForwardListDifferentLink();
		UnitTestForwardListAssign();
		UnitTestForwardListIterator();
		UnitTestForwardListConstIterator();
	}

}
}
}
