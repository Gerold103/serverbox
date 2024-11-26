#pragma once

#include "mg/box/Assert.h"

namespace mg {
namespace box {

	template<typename T, T* T::*myLink>
	struct DoublyListIterator
	{
		DoublyListIterator(
			T* aHead);
		DoublyListIterator& operator++();

		T* operator*() const { return myPos; }
		bool operator!=(
			const DoublyListIterator& aOther) const { return myPos != aOther.myPos; }

		T* myPos;
		T* myNext;
	};

	template<typename T, T* T::*myLink>
	struct DoublyListConstIterator
	{
		DoublyListConstIterator(
			const T* aHead);
		DoublyListConstIterator& operator++();

		const T* operator*() const { return myPos; }
		bool operator!=(
			const DoublyListConstIterator& aOther) const { return myPos != aOther.myPos; }

		const T* myPos;
	};

	// An intrusive bidirectional (doubly-linked) list targeted at API simplicity and
	// memory compactness. Not the most efficient implementation, but the easiest one.
	//
	template<typename T, T* T::*myPrev = &T::myPrev, T* T::*myNext = &T::myNext>
	struct DoublyList
	{
		DoublyList() : myFirst(nullptr), myLast(nullptr) {}
		// Can only move, no copy. Elements can't be in 2 lists at once with the same
		// link members.
		DoublyList(
			DoublyList&& aList);
		DoublyList(
			T* aFirst,
			T* aLast);

		void Prepend(
			T* aItem);
		void Prepend(
			DoublyList&& aSrc);
		void Prepend(
			T* aFirst,
			T* aLast);

		void Append(
			T* aItem);
		void Append(
			DoublyList&& aSrc);
		void Append(
			T* aFirst,
			T* aLast);

		void Reverse();
		void Clear();

		T* PopAll();
		T* PopAll(
			T*& aOutLast);

		void Remove(
			T* aItem);

		T* PopFirst();
		T* PopLast();

		DoublyList& operator=(
			DoublyList&& aOther);

		T* GetFirst() { return myFirst; }
		const T* GetFirst() const { return myFirst; }
		T* GetLast() { return myLast; }
		const T* GetLast() const { return myLast; }

		bool IsEmpty() const { return myFirst == nullptr; }

		DoublyListIterator<T, myNext> begin() { return DoublyListIterator<T, myNext>(myFirst); }
		DoublyListIterator<T, myNext> end() { return DoublyListIterator<T, myNext>(nullptr); }
		DoublyListConstIterator<T, myNext> begin() const { return DoublyListConstIterator<T, myNext>(myFirst); }
		DoublyListConstIterator<T, myNext> end() const { return DoublyListConstIterator<T, myNext>(nullptr); }

	private:
		T* myFirst;
		T* myLast;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	template<typename T, T* T::*myLink>
	inline
	DoublyListIterator<T, myLink>::DoublyListIterator(
		T* aHead)
		: myPos(aHead)
	{
		if (aHead != nullptr)
			myNext = aHead->*myLink;
		else
			myNext = nullptr;
	}

	template<typename T, T* T::*myLink>
	inline DoublyListIterator<T, myLink>&
	DoublyListIterator<T, myLink>::operator++()
	{
		myPos = myNext;
		if (myNext != nullptr)
			myNext = myNext->*myLink;
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////////////////

	template<typename T, T* T::*myLink>
	inline
	DoublyListConstIterator<T, myLink>::DoublyListConstIterator(
		const T* aHead)
		: myPos(aHead)
	{
	}

	template<typename T, T* T::*myLink>
	inline DoublyListConstIterator<T, myLink>&
	DoublyListConstIterator<T, myLink>::operator++()
	{
		myPos = myPos->*myLink;
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////////////////

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline
	DoublyList<T, myPrev, myNext>::DoublyList(
		DoublyList&& aList)
		: myFirst(aList.myFirst)
		, myLast(aList.myLast)
	{
		aList.Clear();
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline
	DoublyList<T, myPrev, myNext>::DoublyList(
		T* aFirst,
		T* aLast)
		: myFirst(aFirst)
		, myLast(aLast)
	{
		if (aFirst != nullptr)
		{
			aFirst->*myPrev = nullptr;
			aLast->*myNext = nullptr;
		}
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline void
	DoublyList<T, myPrev, myNext>::Prepend(
		T* aItem)
	{
		T* first = myFirst;
		aItem->*myPrev = nullptr;
		aItem->*myNext = first;
		if (first != nullptr)
			first->*myPrev = aItem;
		else
			myLast = aItem;
		myFirst = aItem;
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline void
	DoublyList<T, myPrev, myNext>::Prepend(
		DoublyList&& aSrc)
	{
		T* srcFirst = aSrc.myFirst;
		if (srcFirst != nullptr)
		{
			T* srcLast = aSrc.myLast;
			T* first = myFirst;
			srcLast->*myNext = first;
			if (first != nullptr)
				first->*myPrev = srcLast;
			else
				myLast = srcLast;
			myFirst = srcFirst;
			aSrc.Clear();
		}
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline void
	DoublyList<T, myPrev, myNext>::Prepend(
		T* aFirst,
		T* aLast)
	{
		if (aFirst != nullptr)
		{
			T* first = myFirst;
			aFirst->*myPrev = nullptr;
			aLast->*myNext = first;
			if (first != nullptr)
				first->*myPrev = aLast;
			else
				myLast = aLast;
			myFirst = aFirst;
		}
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline T*
	DoublyList<T, myPrev, myNext>::PopFirst()
	{
		T* res = myFirst;
		T* next = res->*myNext;
		res->*myNext = nullptr;
		myFirst = next;
		if (next != nullptr)
			next->*myPrev = nullptr;
		else
			myLast = nullptr;
		return res;
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline T*
	DoublyList<T, myPrev, myNext>::PopLast()
	{
		T* res = myLast;
		T* prev = res->*myPrev;
		res->*myPrev = nullptr;
		myLast = prev;
		if (prev != nullptr)
			prev->*myNext = nullptr;
		else
			myFirst = nullptr;
		return res;
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline void
	DoublyList<T, myPrev, myNext>::Append(
		T* aItem)
	{
		T* last = myLast;
		aItem->*myNext = nullptr;
		aItem->*myPrev = last;
		if (last != nullptr)
			last->*myNext = aItem;
		else
			myFirst = aItem;
		myLast = aItem;
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline void
	DoublyList<T, myPrev, myNext>::Append(
		DoublyList&& aSrc)
	{
		T* srcFirst = aSrc.myFirst;
		if (srcFirst != nullptr)
		{
			T* last = myLast;
			srcFirst->*myPrev = last;
			if (last != nullptr)
				last->*myNext = srcFirst;
			else
				myFirst = srcFirst;
			myLast = aSrc.myLast;
			aSrc.Clear();
		}
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline void
	DoublyList<T, myPrev, myNext>::Append(
		T* aFirst,
		T* aLast)
	{
		if (aFirst != nullptr)
		{
			T* last = myLast;
			aFirst->*myPrev = last;
			aLast->*myNext = nullptr;
			if (last != nullptr)
				last->*myNext = aFirst;
			else
				myFirst = aFirst;
			myLast = aLast;
		}
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	void
	DoublyList<T, myPrev, myNext>::Reverse()
	{
		T* item = myFirst;
		T* next;
		Clear();
		while (item != nullptr)
		{
			next = item->*myNext;
			Prepend(item);
			item = next;
		}
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline void
	DoublyList<T, myPrev, myNext>::Clear()
	{
		myFirst = nullptr;
		myLast = nullptr;
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline T*
	DoublyList<T, myPrev, myNext>::PopAll()
	{
		T* res = myFirst;
		Clear();
		return res;
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline T*
	DoublyList<T, myPrev, myNext>::PopAll(
		T*& aOutLast)
	{
		T* res = myFirst;
		aOutLast = myLast;
		Clear();
		return res;
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline void
	DoublyList<T, myPrev, myNext>::Remove(
		T* aItem)
	{
		T* next = aItem->*myNext;
		T* prev = aItem->*myPrev;
		if (next != nullptr)
		{
			next->*myPrev = prev;
			aItem->*myNext = nullptr;
		}
		else
		{
			MG_DEV_ASSERT(aItem == myLast);
			myLast = prev;
		}
		if (prev != nullptr)
		{
			prev->*myNext = next;
			aItem->*myPrev = nullptr;
		}
		else
		{
			MG_DEV_ASSERT(aItem == myFirst);
			myFirst = next;
		}
	}

	template<typename T, T* T::*myPrev, T* T::*myNext>
	inline DoublyList<T, myPrev, myNext>&
	DoublyList<T, myPrev, myNext>::operator=(
		DoublyList&& aOther)
	{
		myFirst = aOther.myFirst;
		myLast = aOther.myLast;
		aOther.Clear();
		return *this;
	}

}
}
