#pragma once

#include "mg/box/RefCount.h"

#include <utility>

#define SHARED_PTR_ENABLE_FOR_CHILD(ChildT)												\
	typename std::enable_if<std::is_base_of<T, ChildT>::value>::type

namespace mg {
namespace box {

	template <typename T>
	class SharedPtrIntrusive {
	public:
		SharedPtrIntrusive() : myObject(nullptr) {}
		SharedPtrIntrusive(
			const SharedPtrIntrusive& aOther);
		SharedPtrIntrusive(
			SharedPtrIntrusive&& aOther);
		explicit SharedPtrIntrusive(
			T* aObject);
		~SharedPtrIntrusive() { PrivUnref(myObject); }

		template<typename ChildT, typename = SHARED_PTR_ENABLE_FOR_CHILD(ChildT)>
		SharedPtrIntrusive(
			const SharedPtrIntrusive<ChildT>& aOther);
		template<typename ChildT, typename = SHARED_PTR_ENABLE_FOR_CHILD(ChildT)>
		SharedPtrIntrusive(
			SharedPtrIntrusive<ChildT>&& aOther);

		// Set the pointer without incrementing its reference counter. It is useful when
		// it is just created with a predefined reference count. Allows to avoid at least
		// one atomic increment then.
		static SharedPtrIntrusive Wrap(
			T* aObject);
		T* Unwrap();

		SharedPtrIntrusive& operator=(
			const SharedPtrIntrusive& aOther) { Set(aOther.myObject); return *this; }
		SharedPtrIntrusive& operator=(
			SharedPtrIntrusive&& aOther);

		template<typename ChildT, typename = SHARED_PTR_ENABLE_FOR_CHILD(ChildT)>
		SharedPtrIntrusive& operator=(
			const SharedPtrIntrusive<ChildT>& aOther) { Set(aOther.myObject); return *this; }
		template<typename ChildT, typename = SHARED_PTR_ENABLE_FOR_CHILD(ChildT)>
		SharedPtrIntrusive& operator=(
			SharedPtrIntrusive<ChildT>&& aOther);

		void Set(
			T* aPointer);
		void Clear();

		T* GetPointer() { return myObject; }
		T* operator->() { return myObject; }
		T& operator*() { return *myObject; }

		const T* GetPointer() const { return myObject; }
		const T* operator->() const { return myObject; }
		const T& operator*() const { return *myObject; }

		bool IsSet() const { return myObject != nullptr; }

		bool operator==(
			const SharedPtrIntrusive& aOther) const { return myObject == aOther.myObject; }
		template<typename OtherT>
		bool operator==(
			const OtherT* aOther) const { return myObject == aOther; }
		bool operator==(
			std::nullptr_t) const { return myObject == (T*)nullptr; }
		bool operator!=(
			const SharedPtrIntrusive& aOther) const { return myObject != aOther.myObject; }
		template<typename OtherT>
		bool operator!=(
			const OtherT* aOther) const { return myObject != aOther; }
		bool operator!=(
			std::nullptr_t) const { return myObject != (T*)nullptr; }

		template<typename OtherT>
		bool operator==(
			const SharedPtrIntrusive<OtherT>& aOther) const { return myObject == aOther.myObject; }
		template<typename OtherT>
		bool operator!=(
			const SharedPtrIntrusive<OtherT>& aOther) const { return myObject != aOther.myObject; }

	private:
		void PrivUnref(
			T* aPointer) { if (aPointer != nullptr) aPointer->PrivUnref(); }
		void PrivRef() { if (myObject != nullptr) myObject->PrivRef(); }

		T* myObject;

		template<typename>
		friend class SharedPtrIntrusive;
	};

	template <typename T1, typename T2>
	static inline bool operator==(
		const T1* aL,
		const SharedPtrIntrusive<T2>& aR) { return aR == aL; }
	template <typename T>
	static inline bool operator==(
		std::nullptr_t,
		const SharedPtrIntrusive<T>& aR) { return aR == (T*)nullptr; }
	template <typename T1, typename T2>
	static inline bool operator!=(
		const T1* aL,
		const SharedPtrIntrusive<T2>& aR) { return aR != aL; }
	template <typename T>
	static inline bool operator!=(
		std::nullptr_t,
		const SharedPtrIntrusive<T>& aR) { return aR != (T*)nullptr; }

	// Enable Type::Ptr type.
#define SHARED_PTR_TYPE(aClassName)															\
	using Ptr = mg::box::SharedPtrIntrusive<aClassName>;									\
	template<typename aClassName##All>														\
	friend class mg::box::SharedPtrIntrusive;

	// Enable Type::NewShared().
#define SHARED_PTR_NEW(aClassName)															\
	template<typename ...Args>																\
	static Ptr																				\
	NewShared(Args&&... aArgs)																\
	{																						\
		return Ptr::Wrap(new aClassName(std::forward<Args>(aArgs)...));						\
	}

	// Re-declare the shared ptr API assuming that the lower level is already implemented
	// by the parent class. Useful when inherit a shared class and want to use Ptr type
	// with the new class.
#define SHARED_PTR_RENEW_API(aClassName)													\
public:																						\
	SHARED_PTR_TYPE(aClassName)																\
	SHARED_PTR_NEW(aClassName)

	// Enable only the referencing without any types. Useful when need to make an abstract
	// class shared. Then the full API containing NewShared() won't compile.
#define SHARED_PTR_REF(myRef)																\
	void PrivRef() { myRef.Inc(); }															\
	void PrivUnref() { if (myRef.Dec()) delete this; }

	// Full API.
#define SHARED_PTR_API(aClassName, myRef)													\
	SHARED_PTR_RENEW_API(aClassName)														\
protected:																					\
	SHARED_PTR_REF(myRef)

	//////////////////////////////////////////////////////////////////////////////////////

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
		// Same as for the other move-assign.
		T* oldObj = myObject;
		T* newObj = aOther.myObject;
		aOther.myObject = nullptr;
		myObject = newObj;
		if (newObj != oldObj)
			PrivUnref(oldObj);
		return *this;
	}

	template <typename T>
	inline void
	SharedPtrIntrusive<T>::Set(
		T* aObject)
	{
		if (myObject == aObject)
			return;
		T* old = myObject;
		myObject = aObject;
		PrivRef();
		PrivUnref(old);
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

#undef SHARED_PTR_ENABLE_FOR_CHILD
