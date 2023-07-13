#pragma once

#include "mg/box/RefCount.h"

#include <utility>

namespace mg {
namespace box {

	#define SHARED_PTR_ENABLE_FOR_BASE(ChildT)												\
		typename std::enable_if<std::is_base_of<T, ChildT>::value>::type

	template <typename T>
	class SharedPtrIntrusive {
	public:
		SharedPtrIntrusive();
		SharedPtrIntrusive(
			const SharedPtrIntrusive& aOther);
		SharedPtrIntrusive(
			SharedPtrIntrusive&& aOther);
		explicit SharedPtrIntrusive(
			T* aObject);
		~SharedPtrIntrusive() { PrivUnref(myObject); }

		template<typename ChildT, typename = SHARED_PTR_ENABLE_FOR_BASE(ChildT)>
		SharedPtrIntrusive(
			const SharedPtrIntrusive<ChildT>& aOther);
		template<typename ChildT, typename = SHARED_PTR_ENABLE_FOR_BASE(ChildT)>
		SharedPtrIntrusive(
			SharedPtrIntrusive<ChildT>&& aOther);

		// Take the pointer without incrementing its reference
		// counter. It is useful when it is just created with a
		// predefined reference count. Allows to avoid at least
		// one atomic increment then.
		static SharedPtrIntrusive Wrap(
			T* aObject);
		T* Unwrap();

		SharedPtrIntrusive& operator=(
			const SharedPtrIntrusive& aOther) { return Set(aOther.myObject); }
		SharedPtrIntrusive& operator=(
			SharedPtrIntrusive&& aOther);

		template<typename ChildT, typename = SHARED_PTR_ENABLE_FOR_BASE(ChildT)>
		SharedPtrIntrusive& operator=(
			const SharedPtrIntrusive<ChildT>& aOther) { return Set(aOther.myObject); }
		template<typename ChildT, typename = SHARED_PTR_ENABLE_FOR_BASE(ChildT)>
		SharedPtrIntrusive& operator=(
			SharedPtrIntrusive<ChildT>&& aOther);

		SharedPtrIntrusive<T>& Set(
			T* aPointer);

		void Clear();

		T* GetPointer() { return myObject; }
		T* operator->() { return myObject; }
		T& operator*() { return *myObject; }

		const T* GetPointer() const { return myObject; }
		const T* operator->() const { return myObject; }
		const T& operator*() const { return *myObject; }

		bool IsSet() const { return myObject != nullptr; }
#if !IS_COMPILER_MSVC
		// The MS compilers do not yet seem to support this c++ 11 feature.
		// This method is being deleted to make it clear that implicit casting to bool has
		// been considered and rejected due to the potential for bugs caused by unintended
		// implicit upcasting to integers of objects held by shared pointers.
		operator bool() = delete;
#endif
		bool operator==(const SharedPtrIntrusive& aOther) const { return myObject == aOther.myObject; }
		template<typename OtherT>
		bool operator==(const OtherT* aOther) const { return myObject == aOther; }
		bool operator==(std::nullptr_t aOther) const { return myObject == aOther; }
		bool operator!=(const SharedPtrIntrusive& aOther) const { return myObject != aOther.myObject; }
		template<typename OtherT>
		bool operator!=(const OtherT* aOther) const { return myObject != aOther; }
		bool operator!=(std::nullptr_t aOther) const { return myObject != aOther; }

		template<typename OtherT>
		bool operator==(const SharedPtrIntrusive<OtherT>& aOther) const { return myObject == aOther.myObject; }
		template<typename OtherT>
		bool operator!=(const SharedPtrIntrusive<OtherT>& aOther) const { return myObject != aOther.myObject; }

	private:
		void PrivUnref(T* aPointer) { if (aPointer != nullptr) aPointer->PrivUnref(); }
		void PrivRef() { if (myObject != nullptr) myObject->PrivRef(); }

		T* myObject;

		template<typename OtherT>
		friend class SharedPtrIntrusive;
	};

	template <typename T1, typename T2>
	static inline bool operator==(const T1* aL, const SharedPtrIntrusive<T2>& aR) { return aR == aL; }
	template <typename T>
	static inline bool operator==(std::nullptr_t aL, const SharedPtrIntrusive<T>& aR) { return aR == aL; }
	template <typename T1, typename T2>
	static inline bool operator!=(const T1* aL, const SharedPtrIntrusive<T2>& aR) { return aR != aL; }
	template <typename T>
	static inline bool operator!=(std::nullptr_t aL, const SharedPtrIntrusive<T>& aR) { return aR != aL; }

#define SHARED_PTR_TYPE(aClassName)															\
	using Ptr = mg::box::SharedPtrIntrusive<aClassName>;									\
	template<typename aClassName##All>														\
	friend class mg::box::SharedPtrIntrusive;

#define SHARED_PTR_NEW(aClassName)															\
	template<typename ...Args>																\
	static Ptr																				\
	NewShared(Args&&... aArgs) { return Ptr::Wrap(new aClassName(std::forward<Args>(aArgs)...)); }

#define SHARED_PTR_RE_API(aClassName)														\
public:																						\
	SHARED_PTR_TYPE(aClassName)																\
	SHARED_PTR_NEW(aClassName)

#define SHARED_PTR_API(aClassName, myRef)													\
	SHARED_PTR_RE_API(aClassName)															\
private:																					\
	void PrivRef() { myRef.Inc(); }															\
	void PrivUnref() { if (myRef.Dec()) delete this; }

	//////////////////////////////////////////////////////////////////////////////////////

	template <typename T>
	inline
	SharedPtrIntrusive<T>::SharedPtrIntrusive() 
		: myObject(nullptr)
	{
	}

	template <typename T>
	inline
	SharedPtrIntrusive<T>::SharedPtrIntrusive(
		const SharedPtrIntrusive& aOther) 
		: myObject(aOther.myObject)
	{
		PrivRef();
	}

	template <typename T>
	inline
	SharedPtrIntrusive<T>::SharedPtrIntrusive(
		SharedPtrIntrusive&& aOther)
		: myObject(aOther.myObject)
	{
		aOther.myObject = nullptr;
	}

	template <typename T>
	inline
	SharedPtrIntrusive<T>::SharedPtrIntrusive(
		T* aObject)
		: myObject(aObject)
	{
		PrivRef();
	}

	template <typename T>
	template<typename ChildT, typename>
	inline
	SharedPtrIntrusive<T>::SharedPtrIntrusive(
		const SharedPtrIntrusive<ChildT>& aOther)
		: SharedPtrIntrusive(aOther.myObject)
	{
	}

	template <typename T>
	template<typename ChildT, typename>
	inline
	SharedPtrIntrusive<T>::SharedPtrIntrusive(
		SharedPtrIntrusive<ChildT>&& aOther)
		: myObject(aOther.myObject)
	{
		aOther.myObject = nullptr;
	}

	template <typename T>
	inline SharedPtrIntrusive<T>
	SharedPtrIntrusive<T>::Wrap(
		T* aObject)
	{
		SharedPtrIntrusive<T> res;
		res.myObject = aObject;
		return res;
	}

	template <typename T>
	inline T*
	SharedPtrIntrusive<T>::Unwrap()
	{
		T* res = myObject;
		myObject = nullptr;
		return res;
	}

	template <typename T>
	inline SharedPtrIntrusive<T>&
	SharedPtrIntrusive<T>::operator=(
		SharedPtrIntrusive&& aOther)
	{
		// Can't decrement the old pointer right away. Because it can happen that source
		// and destination pointers are related. For example, in a linked list.
		//
		//   ref = Move(ref->myNext);
		//
		// Assume 'ref' is obj1 and ref->myNext is obj2. Both have 1 ref count. If obj1
		// would be dereferenced before stealing the new pointer, it would lead to it
		// being deleted. This in turn will delete obj2, which was kept alive only by
		// obj1. The new pointer becomes garbage. Need to steal the pointer first. So obj1
		// is deleted, but its 'myNext' is already null.
		T* oldObj = myObject;
		T* newObj = aOther.myObject;
		aOther.myObject = nullptr;
		myObject = newObj;
		if (newObj != oldObj)
			PrivUnref(oldObj);
		return *this;
	}

	template <typename T>
	template<typename ChildT, typename>
	inline SharedPtrIntrusive<T>&
	SharedPtrIntrusive<T>::operator=(
		SharedPtrIntrusive<ChildT>&& aOther)
	{
		T* oldObj = myObject;
		T* newObj = aOther.myObject;
		aOther.myObject = nullptr;
		myObject = newObj;
		if (newObj != oldObj)
			PrivUnref(oldObj);
		return *this;
	}

	template <typename T>
	inline SharedPtrIntrusive<T>&
	SharedPtrIntrusive<T>::Set(
		T* aObject) 
	{
		if (myObject == aObject)
			return *this;
		T* old = myObject;
		myObject = aObject;
		PrivRef();
		PrivUnref(old);
		return *this;
	}

	template <typename T>
	inline void
	SharedPtrIntrusive<T>::Clear()
	{
		// Nullify the pointer before unref. The unref might lead to destruction of this
		// shared pointer which should not unref the raw pointer again.
		T* old = myObject;
		myObject = nullptr;
		PrivUnref(old);
	}

}
}
