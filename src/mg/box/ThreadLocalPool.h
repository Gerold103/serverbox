#pragma once

#include "mg/box/Definitions.h"
#include "mg/box/ForwardList.h"
#include "mg/box/Mutex.h"
#include "mg/box/Thread.h"

namespace mg {
namespace box {

static constexpr uint32_t theThreadLocalBatchSize = 32;

template <typename T>
struct ThreadLocalPoolObject
{
	ThreadLocalPoolObject() {}
	~ThreadLocalPoolObject() {}

	// Union allows to keep proper size and alignment while
	// not to invoke any constructors.
	union
	{
		T myBytes;
	};
	ThreadLocalPoolObject* myNext;
};

template <typename T>
struct ThreadLocalPoolObjectChain
{
	using Object = ThreadLocalPoolObject<T>;
	using ObjectList = ForwardList<Object>;

	ThreadLocalPoolObjectChain();

	ObjectList myList;
	uint32_t mySize;
	ThreadLocalPoolObjectChain* myNext;
};

template <typename T>
struct ThreadLocalPoolUnit
{
	using Object = ThreadLocalPoolObject<T>;
	using Chain = ThreadLocalPoolObjectChain<T>;

	ThreadLocalPoolUnit() {}
	~ThreadLocalPoolUnit() {}

	// Each pooled unit is either an object or a list of objects (head and tail pointer).
	// This gives flexibility when a pool is empty and an object is freed - no need to
	// allocate any list-containers. The first freed object simply becomes the container.
	union
	{
		Object myObject;
		Chain myChain;
	};
};

template <typename T, uint32_t BatchSize>
class ThreadGlobalPool;

template <typename T, uint32_t BatchSize>
class ThreadLocalPool
{
public:
	using Chain = ThreadLocalPoolObjectChain<T>;
	using Object = typename Chain::Object;
	using ObjectList = typename Chain::ObjectList;
	using GlobalPool = ThreadGlobalPool<T, BatchSize>;
	using Unit = ThreadLocalPoolUnit<T>;

	ThreadLocalPool();
	~ThreadLocalPool();

	static ThreadLocalPool& GetInstance();
	void* AllocateT();
	void FreeT(
		void* aObject);

	Chain* myChain;
	bool myIsDestroyed;

	static_assert(BatchSize > 0, "BatchSize must be > 0");
};

template <typename T, uint32_t BatchSize>
class ThreadGlobalPool
{
public:
	using LocalPool = ThreadLocalPool<T, BatchSize>;
	using ChainList = ForwardList<typename LocalPool::Chain>;

	static ThreadGlobalPool& GetInstance();

	Mutex myMutex;
	// Thread-local pools return excess objects to the global pool. It allows to keep
	// reusing the objects even if some treads allocate more than free, or vice versa.
	ChainList myFreeChains;
};

template <typename T, size_t BatchSize = theThreadLocalBatchSize>
class ThreadPooled
{
public:
	using LocalPool = ThreadLocalPool<T, BatchSize>;

	void* operator new(
		size_t) { return LocalPool::GetInstance().AllocateT(); }
	void operator delete(
		void* aPtr) { LocalPool::GetInstance().FreeT(aPtr); }
};

//////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
inline
ThreadLocalPoolObjectChain<T>::ThreadLocalPoolObjectChain()
	: mySize(0)
{
}

//////////////////////////////////////////////////////////////////////////////////////////

template <typename T, uint32_t BatchSize>
inline
ThreadLocalPool<T, BatchSize>::ThreadLocalPool()
	: myChain(nullptr)
	, myIsDestroyed(false)
{
}

template <typename T, uint32_t BatchSize>
ThreadLocalPool<T, BatchSize>::~ThreadLocalPool()
{
	MG_DEV_ASSERT(!myIsDestroyed);
	myIsDestroyed = true;
	if (myChain != nullptr)
	{
		GlobalPool& glob = GlobalPool::GetInstance();

		glob.myMutex.Lock();
		glob.myFreeChains.Prepend(myChain);
		glob.myMutex.Unlock();

		myChain = nullptr;
	}
}

template <typename T, uint32_t BatchSize>
inline ThreadLocalPool<T, BatchSize>&
ThreadLocalPool<T, BatchSize>::GetInstance()
{
	static MG_THREADLOCAL ThreadLocalPool ourInstance;
	MG_DEV_ASSERT(!ourInstance.myIsDestroyed);
	return ourInstance;
}

template <typename T, uint32_t BatchSize>
void*
ThreadLocalPool<T, BatchSize>::AllocateT()
{
	MG_DEV_ASSERT(!myIsDestroyed);
	if (myChain == nullptr)
	{
		GlobalPool& glob = GlobalPool::GetInstance();

		glob.myMutex.Lock();
		if (!glob.myFreeChains.IsEmpty())
			myChain = glob.myFreeChains.PopFirst();
		glob.myMutex.Unlock();

		if (myChain == nullptr)
		{
			Unit* units = new Unit[BatchSize + 1];
			// First unit -> container. Other units - objects.
			Chain* chain = new (&units[0]) Chain();
			for (uint32_t i = 1; i < BatchSize + 1; ++i)
				chain->myList.Append(new (&units[i]) Object());
			chain->mySize = BatchSize;
			myChain = chain;
		}
	}
	ObjectList* list = &myChain->myList;
	if (!list->IsEmpty())
	{
		MG_DEV_ASSERT(myChain->mySize > 0);
		--myChain->mySize;
		return (void*)list->PopFirst();
	}
	MG_DEV_ASSERT(myChain->mySize == 0);
	// Can use the container itself as an object too.
	void* obj = (void*)myChain;
	myChain = nullptr;
	return obj;
}

template <typename T, uint32_t BatchSize>
void
ThreadLocalPool<T, BatchSize>::FreeT(
	void* aObject)
{
	MG_DEV_ASSERT(!myIsDestroyed);
	if (myChain == nullptr)
	{
		// First object becomes the container for next objects.
		myChain = new (aObject) Chain();
		return;
	}
	if (myChain->mySize == BatchSize)
	{
		GlobalPool& glob = GlobalPool::GetInstance();

		glob.myMutex.Lock();
		glob.myFreeChains.Prepend(myChain);
		glob.myMutex.Unlock();

		myChain = new (aObject) Chain();;
		return;
	}
	myChain->myList.Prepend((Object*)aObject);
	++myChain->mySize;
}

//////////////////////////////////////////////////////////////////////////////////////////

template <typename T, uint32_t BatchSize>
inline ThreadGlobalPool<T, BatchSize>&
ThreadGlobalPool<T, BatchSize>::GetInstance()
{
	static ThreadGlobalPool ourInstance;
	return ourInstance;
}

}
}
