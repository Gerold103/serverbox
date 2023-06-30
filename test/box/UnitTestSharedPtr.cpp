#include "mg/box/SharedPtr.h"

#include "mg/box/ThreadFunc.h"

#include "UnitTest.h"

#include <vector>

#if IS_COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#pragma clang diagnostic ignored "-Wself-move"
#endif

namespace mg {
namespace unittests {
namespace box {
namespace sharedptr {

	struct ValueOnStack
	{
		SHARED_PTR_TYPE(ValueOnStack)

		ValueOnStack()
			: myRef(0)
			, myIsDeleted(false)
		{

		}

		~ValueOnStack()
		{
			TEST_CHECK(myIsDeleted);
		}

	private:
		void PrivRef() { TEST_CHECK(!myIsDeleted); myRef.Inc(); }
		void PrivUnref() { TEST_CHECK(!myIsDeleted); if (myRef.Dec()) myIsDeleted = true; }

	public:
		mg::box::RefCount myRef;
		bool myIsDeleted;
		ValueOnStack::Ptr myNext;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	struct ValueOnHeap
	{
		SHARED_PTR_API(ValueOnHeap, myRef)
private:
		ValueOnHeap(
			int aValue)
			: myValue(aValue)
		{
		}

		~ValueOnHeap()
		{
 			++ourDeleteCount;
		}

public:
		mg::box::RefCount myRef;
		int myValue;

		static int ourDeleteCount;
	};

	int ValueOnHeap::ourDeleteCount = 0;

	//////////////////////////////////////////////////////////////////////////////////////

	struct ValueBase
	{
	public:
		SHARED_PTR_TYPE(ValueBase)
		SHARED_PTR_NEW(ValueBase)

		ValueBase(
			int aValue)
			: myValue(aValue)
		{
		}

		virtual ~ValueBase()
		{
			++ourDeleteCount;
		}

	private:
		void PrivRef() { myRef.Inc(); ++ourRefIncCount; }
		void PrivUnref() { if (myRef.Dec()) delete this; }

	public:
		mg::box::RefCount myRef;
		int myValue;

		static int ourDeleteCount;
		static int ourRefIncCount;
	};

	int ValueBase::ourDeleteCount = 0;
	int ValueBase::ourRefIncCount = 0;

	//////////////////////////////////////////////////////////////////////////////////////

	struct ValueChild
		: public ValueBase
	{
		SHARED_PTR_RENEW_API(ValueChild)

		ValueChild(
			int aValue)
			: ValueBase(aValue)
		{
		}

		~ValueChild() override
		{
			++ourDeleteCount;
		}

		static int ourDeleteCount;
	};

	int ValueChild::ourDeleteCount = 0;

	//////////////////////////////////////////////////////////////////////////////////////

	static void
	UnitTestSharedPtrBasic()
	{
		TestCaseGuard guard("Basic");
		using Value = ValueOnStack;
		{
			Value v;
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(!v.myIsDeleted);
			{
				Value::Ptr ptr;
				ptr.Set(&v);
				TEST_CHECK(v.myRef.Get() == 1);
				TEST_CHECK(!v.myIsDeleted);
			}
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
		{
			Value v;
			Value::Ptr ptr1;
			ptr1.Set(&v);
			TEST_CHECK(v.myRef.Get() == 1);
			TEST_CHECK(!v.myIsDeleted);

			Value::Ptr ptr2;
			ptr2.Set(&v);
			TEST_CHECK(v.myRef.Get() == 2);
			TEST_CHECK(!v.myIsDeleted);

			ptr2.Clear();
			TEST_CHECK(v.myRef.Get() == 1);
			TEST_CHECK(!v.myIsDeleted);

			ptr1.Clear();
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
		{
			Value v;
			Value::Ptr ptr;
			ptr.Set(&v);

			constexpr int threadCount = 10;
			mg::box::AtomicBool isStopRequested(false);
			std::vector<mg::box::ThreadFunc*> threads;
			for (int i = 0; i < threadCount; ++i)
			{
				threads.push_back(new mg::box::ThreadFunc([&]() {
					Value::Ptr localPtr = ptr;
					uint64_t yield = 0;
					while (!isStopRequested.LoadRelaxed())
					{
						Value::Ptr ptr = localPtr;
						TEST_CHECK(!ptr->myIsDeleted);
						if (++yield % 10000 == 0)
							mg::box::Sleep(1);
					}
				}));
				threads.back()->Start();
			}
			mg::box::Sleep(100);
			isStopRequested.StoreRelaxed(true);
			for (mg::box::ThreadFunc* t : threads)
				delete t;

			TEST_CHECK(v.myRef.Get() == 1);
			TEST_CHECK(!v.myIsDeleted);
			ptr.Clear();
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
	}

	static void
	UnitTestSharedPtrConstructor()
	{
		TestCaseGuard guard("Constructor");
		using Value = ValueOnStack;
		// Default.
		{
			Value::Ptr ptr;
			TEST_CHECK(!ptr.IsSet());
		}
		// Copy from empty.
		{
			Value::Ptr ptr1;
			Value::Ptr ptr2(ptr1);
			TEST_CHECK(!ptr1.IsSet() && !ptr2.IsSet());
		}
		// Copy from non-empty.
		{
			Value v;
			{
				Value::Ptr ptr1(&v);
				Value::Ptr ptr2(ptr1);
				TEST_CHECK(v.myRef.Get() == 2);
				TEST_CHECK(ptr1 == &v && ptr2 == &v);
			}
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
		// Move from empty.
		{
			Value::Ptr ptr1;
			Value::Ptr ptr2(std::move(ptr1));
			TEST_CHECK(!ptr1.IsSet() && !ptr2.IsSet());
		}
		// Move from non-empty.
		{
			Value v;
			{
				Value::Ptr ptr1(&v);
				Value::Ptr ptr2(std::move(ptr1));
				TEST_CHECK(v.myRef.Get() == 1);
				TEST_CHECK(ptr1 == nullptr && ptr2 == &v);
			}
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
		// From raw empty.
		{
			Value::Ptr ptr(nullptr);
			TEST_CHECK(!ptr.IsSet());
		}
		// From raw non-empty.
		{
			Value v;
			{
				Value::Ptr ptr(&v);
				TEST_CHECK(v.myRef.Get() == 1);
				TEST_CHECK(ptr == &v);
			}
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
	}

	static void
	UnitTestSharedPtrDestructor()
	{
		TestCaseGuard guard("Destructor");
		using Value = ValueOnStack;
		// Empty.
		{
			Value::Ptr ptr;
		}
		// Non-empty.
		Value v;
		{
			Value::Ptr ptr(&v);
			TEST_CHECK(ptr == &v);
		}
		TEST_CHECK(v.myRef.Get() == 0);
		TEST_CHECK(v.myIsDeleted);
	}

	static void
	UnitTestSharedPtrWrap()
	{
		TestCaseGuard guard("Wrap");
		using Value = ValueOnStack;

		// From empty.
		Value::Ptr p = Value::Ptr::Wrap(nullptr);
		TEST_CHECK(!p.IsSet());

		// From non-empty.
		Value v;
		v.myRef.Inc();
		TEST_CHECK(v.myRef.Get() == 1);
		p = Value::Ptr::Wrap(&v);
		TEST_CHECK(v.myRef.Get() == 1);
		p.Clear();
		TEST_CHECK(v.myRef.Get() == 0);
		TEST_CHECK(v.myIsDeleted);
	}

	static void
	UnitTestSharedPtrUnwrap()
	{
		TestCaseGuard guard("Unwrap");
		using Value = ValueOnStack;

		// From empty.
		Value::Ptr p;
		TEST_CHECK(p.Unwrap() == nullptr);
		TEST_CHECK(!p.IsSet());

		// From non-empty.
		Value v;
		p.Set(&v);
		TEST_CHECK(v.myRef.Get() == 1);
		TEST_CHECK(p.Unwrap() == &v);
		TEST_CHECK(!p.IsSet());
		TEST_CHECK(v.myRef.Get() == 1);
		TEST_CHECK(!v.myIsDeleted);
		p.Wrap(&v);
	}

	static void
	UnitTestSharedPtrAssignCopy()
	{
		TestCaseGuard guard("Assign copy");
		using Value = ValueOnStack;

		// Empty = empty.
		{
			Value::Ptr p1;
			Value::Ptr p2;
			p1 = p2;
			TEST_CHECK(!p1.IsSet() && !p2.IsSet());
		}
		// Empty = non-empty.
		{
			Value v;
			{
				Value::Ptr p1;
				Value::Ptr p2(&v);
				TEST_CHECK(v.myRef.Get() == 1);
				p1 = p2;
				TEST_CHECK(v.myRef.Get() == 2);
				TEST_CHECK(p1 == &v && p2 == &v);
			}
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
		// Non-empty = empty.
		{
			Value v;
			Value::Ptr p1;
			Value::Ptr p2(&v);
			TEST_CHECK(v.myRef.Get() == 1);
			p2 = p1;
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
			TEST_CHECK(!p1.IsSet() && !p2.IsSet());
		}
		// Non-empty = non-empty.
		{
			Value v1;
			Value v2;
			{
				Value::Ptr p1(&v1);
				Value::Ptr p2(&v2);
				TEST_CHECK(v1.myRef.Get() == 1);
				TEST_CHECK(v2.myRef.Get() == 1);
				p1 = p2;
				TEST_CHECK(v2.myRef.Get() == 2);
				TEST_CHECK(p1 == &v2 && p2 == &v2);

				TEST_CHECK(v1.myRef.Get() == 0);
				TEST_CHECK(v1.myIsDeleted);
			}
			TEST_CHECK(v2.myRef.Get() == 0);
			TEST_CHECK(v2.myIsDeleted);
		}
		// Self = self empty.
		{
			Value::Ptr p;
			p = p;
			TEST_CHECK(!p.IsSet());
		}
		// Self = self non-empty.
		{
			Value v;
			{
				Value::Ptr p(&v);
				TEST_CHECK(v.myRef.Get() == 1);
				p = p;
				TEST_CHECK(v.myRef.Get() == 1);
				TEST_CHECK(p == &v);
			}
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
	}

	static void
	UnitTestSharedPtrAssignMove()
	{
		TestCaseGuard guard("Assign move");
		using Value = ValueOnStack;

		// Empty = empty.
		{
			Value::Ptr p1;
			Value::Ptr p2;
			p1 = std::move(p2);
			TEST_CHECK(!p1.IsSet() && !p2.IsSet());
		}
		// Empty = non-empty.
		{
			Value v;
			{
				Value::Ptr p1;
				Value::Ptr p2(&v);
				TEST_CHECK(v.myRef.Get() == 1);
				p1 = std::move(p2);
				TEST_CHECK(v.myRef.Get() == 1);
				TEST_CHECK(p1 == &v && !p2.IsSet());
			}
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
		// Non-empty = empty.
		{
			Value v;
			Value::Ptr p1;
			Value::Ptr p2(&v);
			TEST_CHECK(v.myRef.Get() == 1);
			p2 = std::move(p1);
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
			TEST_CHECK(!p1.IsSet() && !p2.IsSet());
		}
		// Non-empty = non-empty.
		{
			Value v1;
			Value v2;
			{
				Value::Ptr p1(&v1);
				Value::Ptr p2(&v2);
				TEST_CHECK(v1.myRef.Get() == 1);
				TEST_CHECK(v2.myRef.Get() == 1);
				p1 = std::move(p2);
				TEST_CHECK(v2.myRef.Get() == 1);
				TEST_CHECK(p1 == &v2 && !p2.IsSet());

				TEST_CHECK(v1.myRef.Get() == 0);
				TEST_CHECK(v1.myIsDeleted);
			}
			TEST_CHECK(v2.myRef.Get() == 0);
			TEST_CHECK(v2.myIsDeleted);
		}
		// Self = self empty.
		{
			Value::Ptr p;
			p = std::move(p);
			TEST_CHECK(!p.IsSet());
		}
		// Self = self non-empty.
		{
			Value v;
			{
				Value::Ptr p(&v);
				TEST_CHECK(v.myRef.Get() == 1);
				p = std::move(p);
				TEST_CHECK(v.myRef.Get() == 1);
				TEST_CHECK(p == &v);
			}
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
	}

	static void
	UnitTestSharedPtrSet()
	{
		TestCaseGuard guard("Set");
		using Value = ValueOnStack;

		// Empty = empty.
		{
			Value::Ptr p;
			p.Set(nullptr);
			TEST_CHECK(!p.IsSet());
		}
		// Empty = non-empty.
		{
			Value v;
			{
				Value::Ptr p;
				TEST_CHECK(v.myRef.Get() == 0);
				p.Set(&v);
				TEST_CHECK(v.myRef.Get() == 1);
				TEST_CHECK(p == &v);
			}
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
		// Non-empty = empty.
		{
			Value v;
			Value::Ptr p(&v);
			TEST_CHECK(v.myRef.Get() == 1);
			TEST_CHECK(p == &v);
			p.Set(nullptr);
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
			TEST_CHECK(!p.IsSet());
		}
		// Non-empty = non-empty.
		{
			Value v1;
			Value v2;
			{
				Value::Ptr p(&v1);
				TEST_CHECK(v1.myRef.Get() == 1);
				TEST_CHECK(v2.myRef.Get() == 0);
				TEST_CHECK(p == &v1);
				p.Set(&v2);
				TEST_CHECK(v1.myRef.Get() == 0);
				TEST_CHECK(v1.myIsDeleted);
				TEST_CHECK(v2.myRef.Get() == 1);
				TEST_CHECK(p == &v2);
			}
			TEST_CHECK(v2.myRef.Get() == 0);
			TEST_CHECK(v2.myIsDeleted);
		}
		// Self = self non-empty.
		{
			Value v;
			{
				Value::Ptr p(&v);
				TEST_CHECK(v.myRef.Get() == 1);
				p.Set(&v);
				TEST_CHECK(v.myRef.Get() == 1);
				TEST_CHECK(p == &v);
			}
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
	}

	static void
	UnitTestSharedPtrClear()
	{
		TestCaseGuard guard("Clear");
		using Value = ValueOnStack;
		// Empty.
		{
			Value::Ptr p;
			TEST_CHECK(!p.IsSet());
			p.Clear();
			TEST_CHECK(!p.IsSet());
		}
		// Non-empty.
		{
			Value v;
			Value::Ptr p;
			p.Set(&v);
			TEST_CHECK(v.myRef.Get() == 1);
			TEST_CHECK(!v.myIsDeleted);

			p.Clear();
			TEST_CHECK(!p.IsSet());
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
		// Loop link.
		{
			Value v;
			v.myNext.Set(&v);
			TEST_CHECK(v.myRef.Get() == 1);
			v.myNext.Clear();
			TEST_CHECK(!v.myNext.IsSet());
			TEST_CHECK(v.myRef.Get() == 0);
			TEST_CHECK(v.myIsDeleted);
		}
	}

	static void
	UnitTestSharedPtrGetPointer()
	{
		TestCaseGuard guard("GetPointer");
		using Value = ValueOnStack;
		// Empty.
		{
			Value::Ptr p;
			TEST_CHECK(p.GetPointer() == nullptr);
		}
		// Non-empty.
		{
			Value v;
			Value::Ptr p;
			p.Set(&v);
			TEST_CHECK(p.GetPointer() == &v);
			p.Clear();
			TEST_CHECK(p.GetPointer() == nullptr);
		}
	}

	static void
	UnitTestSharedPtrDereference()
	{
		TestCaseGuard guard("Operators '->' and '*'");
		using Value = ValueOnStack;
		Value v;
		Value::Ptr p(&v);
		TEST_CHECK(&p->myNext == &v.myNext);
		TEST_CHECK(&(*p) == &v);

		const Value::Ptr& pc = p;
		TEST_CHECK(&pc->myNext == &v.myNext);
		TEST_CHECK(&(*pc) == &v);
	}

	static void
	UnitTestSharedPtrIsSet()
	{
		TestCaseGuard guard("IsSet()");
		using Value = ValueOnStack;
		Value v;
		Value::Ptr p;
		const Value::Ptr& pc = p;
		TEST_CHECK(!p.IsSet() && p == nullptr);
		TEST_CHECK(!pc.IsSet() && pc == nullptr);
		p.Set(&v);
		TEST_CHECK(p.IsSet() && p == &v);
		TEST_CHECK(pc.IsSet() && pc == &v);
	}

	static void
	UnitTestSharedPtrOperatorEqNe()
	{
		TestCaseGuard guard("Operators '==' and '!='");
		using Value = ValueOnStack;
		Value v1;
		Value v2;
		// Empty = empty.
		Value::Ptr p1;
		Value::Ptr p2;
		TEST_CHECK(p1 == p2 && !(p1 != p2));
		TEST_CHECK(p2 == p1 && !(p2 != p1));
		TEST_CHECK(p1 == nullptr && !(p1 != nullptr));
		TEST_CHECK(nullptr == p1 && !(nullptr != p1));
		// Non-empty == non-empty.
		p1.Set(&v1);
		p2 = p1;
		TEST_CHECK(p1 == p2 && !(p1 != p2));
		TEST_CHECK(p2 == p1 && !(p2 != p1));
		TEST_CHECK(p1 == &v1 && !(p1 != &v1));
		TEST_CHECK(&v1 == p1 && !(&v1 != p1));
		// Non-empty != non-empty.
		p2.Set(&v2);
		TEST_CHECK(p1 != p2 && !(p1 == p2));
		TEST_CHECK(p2 != p1 && !(p2 == p1));
		TEST_CHECK(p1 == &v1 && !(p1 != &v1));
		TEST_CHECK(&v1 == p1 && !(&v1 != p1));
		TEST_CHECK(p2 == &v2 && !(p2 != &v2));
		TEST_CHECK(&v2 == p2 && !(&v2 != p2));
		// Empty != non-empty.
		p2.Clear();
		TEST_CHECK(p1 != p2 && !(p1 == p2));
		TEST_CHECK(p2 != p1 && !(p2 == p1));
		TEST_CHECK(p2 == nullptr && !(p2 != nullptr));
		TEST_CHECK(nullptr == p2 && !(nullptr != p2));
	}

	static void
	UnitTestSharedPtrNewShared()
	{
		TestCaseGuard guard("NewShared");
		using Value = ValueOnHeap;
		Value::ourDeleteCount = 0;

		Value::Ptr p = Value::NewShared(10);
		TEST_CHECK(p->myValue == 10);
		TEST_CHECK(p->myRef.Get() == 1);
		p.Clear();
		TEST_CHECK(Value::ourDeleteCount == 1);
	}

	static void
	UnitTestSharedPtrInheritance()
	{
		TestCaseGuard guard("Inheritance");
		ValueBase::ourDeleteCount = 0;
		ValueBase::ourRefIncCount = 0;
		ValueChild::ourDeleteCount = 0;
		//
		// Construct from child.
		//
		{
			ValueBase::Ptr basePtr(new ValueChild(1));
			TEST_CHECK(basePtr->myRef.Get() == 2);
			basePtr->myRef.Dec();
			basePtr.Clear();
		}
		TEST_CHECK(ValueBase::ourDeleteCount == 1);
		ValueBase::ourDeleteCount = 0;
		TEST_CHECK(ValueBase::ourRefIncCount == 1);
		ValueBase::ourRefIncCount = 0;
		TEST_CHECK(ValueChild::ourDeleteCount == 1);
		ValueChild::ourDeleteCount = 0;
		//
		// Wrap child.
		//
		{
			ValueBase::Ptr basePtr = ValueBase::Ptr::Wrap(new ValueChild(1));
			TEST_CHECK(basePtr->myRef.Get() == 1);
			basePtr.Clear();
		}
		TEST_CHECK(ValueBase::ourDeleteCount == 1);
		ValueBase::ourDeleteCount = 0;
		TEST_CHECK(ValueBase::ourRefIncCount == 0);
		TEST_CHECK(ValueChild::ourDeleteCount == 1);
		ValueChild::ourDeleteCount = 0;
		//
		// Set child.
		//
		{
			ValueBase::Ptr basePtr;
			basePtr.Set(new ValueChild(1));
			TEST_CHECK(basePtr->myRef.Get() == 2);
			basePtr->myRef.Dec();
			basePtr.Clear();
		}
		TEST_CHECK(ValueBase::ourDeleteCount == 1);
		ValueBase::ourDeleteCount = 0;
		TEST_CHECK(ValueBase::ourRefIncCount == 1);
		ValueBase::ourRefIncCount = 0;
		TEST_CHECK(ValueChild::ourDeleteCount == 1);
		ValueChild::ourDeleteCount = 0;
		//
		// Copy-construct from child ptr.
		//
		{
			ValueChild::Ptr childPtr = ValueChild::NewShared(1);
			ValueBase::Ptr basePtr(childPtr);
			TEST_CHECK(basePtr->myRef.Get() == 2);
			TEST_CHECK(childPtr.GetPointer() == basePtr.GetPointer());
		}
		TEST_CHECK(ValueBase::ourDeleteCount == 1);
		ValueBase::ourDeleteCount = 0;
		TEST_CHECK(ValueBase::ourRefIncCount == 1);
		ValueBase::ourRefIncCount = 0;
		TEST_CHECK(ValueChild::ourDeleteCount == 1);
		ValueChild::ourDeleteCount = 0;
		//
		// Move-construct from child ptr.
		//
		{
			ValueChild::Ptr childPtr = ValueChild::NewShared(1);
			ValueBase::Ptr basePtr(std::move(childPtr));
			TEST_CHECK(basePtr->myRef.Get() == 1);
			TEST_CHECK(!childPtr.IsSet());
			TEST_CHECK(basePtr.IsSet());
		}
		TEST_CHECK(ValueBase::ourDeleteCount == 1);
		ValueBase::ourDeleteCount = 0;
		TEST_CHECK(ValueBase::ourRefIncCount == 0);
		TEST_CHECK(ValueChild::ourDeleteCount == 1);
		ValueChild::ourDeleteCount = 0;
		//
		// Copy-assign from child ptr.
		//
		{
			ValueChild::Ptr childPtr = ValueChild::NewShared(1);
			ValueBase::Ptr basePtr;
			basePtr = childPtr;
			TEST_CHECK(basePtr->myRef.Get() == 2);
			TEST_CHECK(childPtr.GetPointer() == basePtr.GetPointer());
		}
		TEST_CHECK(ValueBase::ourDeleteCount == 1);
		ValueBase::ourDeleteCount = 0;
		TEST_CHECK(ValueBase::ourRefIncCount == 1);
		ValueBase::ourRefIncCount = 0;
		TEST_CHECK(ValueChild::ourDeleteCount == 1);
		ValueChild::ourDeleteCount = 0;
		//
		// Move-assign from child ptr.
		//
		{
			ValueChild::Ptr childPtr = ValueChild::NewShared(1);
			ValueBase::Ptr basePtr;
			basePtr = std::move(childPtr);
			TEST_CHECK(basePtr->myRef.Get() == 1);
			TEST_CHECK(!childPtr.IsSet());
			TEST_CHECK(basePtr.IsSet());
		}
		TEST_CHECK(ValueBase::ourDeleteCount == 1);
		ValueBase::ourDeleteCount = 0;
		TEST_CHECK(ValueBase::ourRefIncCount == 0);
		TEST_CHECK(ValueChild::ourDeleteCount == 1);
		ValueChild::ourDeleteCount = 0;
		//
		// Compare.
		//
		{
			ValueChild::Ptr childPtr = ValueChild::NewShared(1);
			ValueBase::Ptr basePtr = ValueBase::NewShared(1);
			TEST_CHECK(!(basePtr == childPtr) && basePtr != childPtr);
			TEST_CHECK(!(childPtr == basePtr) && childPtr != basePtr);
			TEST_CHECK(ValueBase::ourDeleteCount == 0);
			TEST_CHECK(ValueBase::ourRefIncCount == 0);
			TEST_CHECK(ValueChild::ourDeleteCount == 0);
			void* rawPtr = nullptr;
			TEST_CHECK(!(basePtr == rawPtr) && basePtr != rawPtr);
			TEST_CHECK(!(rawPtr == basePtr) && rawPtr != basePtr);
		}
	}
}

	void
	UnitTestSharedPtr()
	{
		using namespace sharedptr;
		TestSuiteGuard suite("SharedPtr");

		UnitTestSharedPtrBasic();
		UnitTestSharedPtrConstructor();
		UnitTestSharedPtrDestructor();
		UnitTestSharedPtrWrap();
		UnitTestSharedPtrUnwrap();
		UnitTestSharedPtrAssignCopy();
		UnitTestSharedPtrAssignMove();
		UnitTestSharedPtrSet();
		UnitTestSharedPtrClear();
		UnitTestSharedPtrGetPointer();
		UnitTestSharedPtrDereference();
		UnitTestSharedPtrIsSet();
		UnitTestSharedPtrOperatorEqNe();
		UnitTestSharedPtrNewShared();
		UnitTestSharedPtrInheritance();
	}

}
}
}

#if IS_COMPILER_CLANG
#pragma clang diagnostic pop
#endif
