#include "DomainToIP.h"

#include "mg/box/BinaryHeap.h"
#include "mg/box/DoublyList.h"
#include "mg/box/ForwardList.h"
#include "mg/box/Thread.h"

#if !IS_PLATFORM_WIN
#include <netdb.h>
#endif

namespace mg {
namespace net {

#if IS_BUILD_DEBUG
	static uint64_t theDebugProcessLatency = 0;
	static mg::box::ErrorCode theDebugErrorCode = mg::box::ERR_BOX_NONE;
#endif

	class DomainToIPContext
	{
	public:
		DomainToIPContext(
			const char* aDomain,
			mg::box::TimeLimit aTimeLimit,
			const DomainToIPCallback& aCallback);

		SHARED_PTR_REF(myRef)
		void ReturnCancel();
		void ReturnTimeout();
		void ReturnError(
			mg::box::Error* aError);
		void ReturnEndpoints(
			const std::vector<DomainEndpoint>& aEndpoints);
		void Process();

		bool operator<=(
			const DomainToIPContext& aCtx) const { return myDeadline <= aCtx.myDeadline; }

		DomainToIPContext* myNext;
		DomainToIPContext* myPrev;
		DomainToIPContext* myNextCancel;
		uint64_t myDeadline;
		int32_t myIndex;
		bool myIsCancelled;
		mg::box::RefCount myRef;
		std::string myDomain;
		DomainToIPCallback myCallback;

		friend DomainToIPRequest;
		friend DomainToIPWorker;
	};

	using DomainToIPHeap = mg::box::BinaryHeapMinIntrusive<DomainToIPContext>;
	using DomainToIPForwardList = mg::box::ForwardList<DomainToIPContext>;
	using DomainToIPDoublyList = mg::box::DoublyList<DomainToIPContext>;
	using DomainToIPCancelList = mg::box::ForwardList<DomainToIPContext,
		&DomainToIPContext::myNextCancel>;

	class DomainToIPWorker
		: private mg::box::Thread
	{
	public:
		DomainToIPWorker();
		~DomainToIPWorker();

		void Post(
			DomainToIPRequest& aOutRequest,
			const char* aDomain,
			mg::box::TimeLimit aTimeLimit,
			const DomainToIPCallback& aCallback);

		void Cancel(
			DomainToIPRequest& aRequest);

	private:
		void Run() override;
		void PrivProcessMain();
		void PrivProcessFront();

		// The queue of accepted requests to execute, accessed only by this
		// worker thread.
		DomainToIPDoublyList myMain;
		DomainToIPHeap myMainHeap;

		mg::box::Mutex myMutex;
		mg::box::ConditionVariable myCond;
		// Not yet processed requests potentially received from multiple
		// external threads.
		DomainToIPForwardList myFront;
		DomainToIPCancelList myFrontCancel;
	};

	static DomainToIPWorker& DomainToIPGetInstance();

	//////////////////////////////////////////////////////////////////////////////////////

	DomainToIPRequest::DomainToIPRequest()
		: myCtx(nullptr)
	{
	}

	DomainToIPRequest::DomainToIPRequest(
		DomainToIPContext* aContext)
		: myCtx(aContext)
	{
	}

	DomainToIPRequest::DomainToIPRequest(
		DomainToIPRequest&& aOther)
		: myCtx(aOther.myCtx)
	{
		aOther.myCtx = nullptr;
	}

	DomainToIPRequest::~DomainToIPRequest()
	{
		if (myCtx != nullptr)
			myCtx->PrivUnref();
	}

	DomainToIPRequest&
	DomainToIPRequest::operator=(
		DomainToIPRequest&& aOther)
	{
		if (this == &aOther)
			return *this;
		MG_DEV_ASSERT(myCtx != aOther.myCtx);
		if (myCtx != nullptr)
			myCtx->PrivUnref();
		myCtx = aOther.myCtx;
		aOther.myCtx = nullptr;
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////////////////

	void
	DomainToIPAsync(
		DomainToIPRequest& aOutRequest,
		const char* aDomain,
		mg::box::TimeLimit aTimeLimit,
		const DomainToIPCallback& aCallback)
	{
		DomainToIPGetInstance().Post(aOutRequest, aDomain, aTimeLimit, aCallback);
	}

	void
	DomainToIPCancel(
		DomainToIPRequest& aRequest)
	{
		DomainToIPGetInstance().Cancel(aRequest);
	}

	bool
	DomainToIPBlocking(
		const char* aDomain,
		mg::box::TimeLimit aTimeLimit,
		std::vector<DomainEndpoint>& aOutEndpoints,
		mg::box::Error::Ptr& aOutError)
	{
		mg::box::Mutex mutex;
		mg::box::ConditionVariable cond;
		bool isFinished = false;
		DomainToIPRequest req;
		DomainToIPAsync(req, aDomain, aTimeLimit, [&](
			const char* /* aDomain */,
			const std::vector<DomainEndpoint>& aEndpoints,
			mg::box::Error* aError) {

			aOutEndpoints = aEndpoints;
			aOutError.Set(aError);

			mutex.Lock();
			isFinished = true;
			cond.Signal();
			mutex.Unlock();
		});

		mutex.Lock();
		while (!isFinished)
			cond.Wait(mutex);
		mutex.Unlock();

		return !aOutError.IsSet();
	}

	//////////////////////////////////////////////////////////////////////////////////////

	inline
	DomainToIPWorker::DomainToIPWorker() : Thread("mgnet.domtoip")
	{
#if IS_PLATFORM_WIN
		WSADATA data;
		MG_BOX_ASSERT(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
		Start();
	}

	inline
	DomainToIPWorker::~DomainToIPWorker()
	{
		MG_BOX_ASSERT(!"The worker can't be deleted");
#if IS_PLATFORM_WIN
		MG_BOX_ASSERT(WSACleanup() == 0);
#endif
	}

	void
	DomainToIPWorker::Post(
		DomainToIPRequest& aOutRequest,
		const char* aDomain,
		mg::box::TimeLimit aTimeLimit,
		const DomainToIPCallback& aCallback)
	{
		DomainToIPContext* ctx = new DomainToIPContext(aDomain, aTimeLimit, aCallback);
		aOutRequest = DomainToIPRequest(ctx);
		// +1 for being in the worker's queues.
		ctx->PrivRef();

		myMutex.Lock();
		bool wasEmpty = myFront.IsEmpty();
		myFront.Append(ctx);
		if (wasEmpty)
			myCond.Signal();
		myMutex.Unlock();
	}

	void
	DomainToIPWorker::Cancel(
		DomainToIPRequest& aRequest)
	{
		DomainToIPContext* ctx = aRequest.myCtx;
		aRequest.myCtx = nullptr;
		MG_DEV_ASSERT(!ctx->myIsCancelled);
		ctx->myIsCancelled = true;

		myMutex.Lock();
		bool wasEmpty = myFrontCancel.IsEmpty();
		myFrontCancel.Append(ctx);
		if (wasEmpty)
			myCond.Signal();
		myMutex.Unlock();
	}

	void
	DomainToIPWorker::Run()
	{
		while (true)
		{
			MG_DEV_ASSERT(!StopRequested());
#if IS_BUILD_DEBUG
			uint64_t value = theDebugProcessLatency;
			if (value != 0)
				mg::box::Sleep(value);
#endif
			PrivProcessMain();
			PrivProcessFront();
		}
	}

	void
	DomainToIPWorker::PrivProcessMain()
	{
		if (myMain.IsEmpty())
			return;
		DomainToIPContext* ctx;
		uint64_t ts = mg::box::GetMilliseconds();
		while ((ctx = myMainHeap.GetTop())->myDeadline <= ts)
		{
			myMainHeap.RemoveTop();
			myMain.Remove(ctx);
			ctx->ReturnTimeout();
			ctx->PrivUnref();
			if (myMain.IsEmpty())
				return;
		}
		int batchSize = 10;
		while (!myMain.IsEmpty() && --batchSize >= 0)
		{
			ctx = myMain.PopFirst();
			myMainHeap.Remove(ctx);
			ctx->Process();
			ctx->PrivUnref();
		}
	}

	void
	DomainToIPWorker::PrivProcessFront()
	{
		uint64_t deadline = MG_TIME_INFINITE;
		if (myMainHeap.Count() != 0)
			deadline = myMainHeap.GetTop()->myDeadline;

		DomainToIPForwardList front;
		DomainToIPCancelList cancel;

		myMutex.Lock();
		if (myMain.IsEmpty() && myFront.IsEmpty() && myFrontCancel.IsEmpty())
		{
			if (deadline == MG_TIME_INFINITE)
			{
				myCond.Wait(myMutex);
			}
			else
			{
				uint64_t ts = mg::box::GetMilliseconds();
				if (ts < deadline)
				{
					myCond.TimedWait(myMutex,
						mg::box::TimeDuration(deadline - ts));
				}
			}
		}
		front = std::move(myFront);
		cancel = std::move(myFrontCancel);
		myMutex.Unlock();

		while (!front.IsEmpty())
		{
			DomainToIPContext* ctx = front.PopFirst();
			myMain.Append(ctx);
			myMainHeap.Push(ctx);
		}
		while (!cancel.IsEmpty())
		{
			DomainToIPContext* ctx = cancel.PopFirst();
			if (ctx->myIndex >= 0)
			{
				myMain.Remove(ctx);
				myMainHeap.Remove(ctx);
				ctx->ReturnCancel();
				ctx->PrivUnref();
			}
			ctx->PrivUnref();
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////

	DomainToIPContext::DomainToIPContext(
		const char* aDomain,
		mg::box::TimeLimit aTimeLimit,
		const DomainToIPCallback& aCallback)
		: myNext(nullptr)
		, myPrev(nullptr)
		, myNextCancel(nullptr)
		, myDeadline(aTimeLimit.ToPointFromNow().myValue)
		, myIndex(-1)
		, myIsCancelled(false)
		, myDomain(aDomain)
		, myCallback(aCallback)
	{
	}

	inline void
	DomainToIPContext::ReturnCancel()
	{
		ReturnError(mg::box::ErrorRaise(mg::box::ERR_BOX_CANCEL));
	}

	inline void
	DomainToIPContext::ReturnTimeout()
	{
		ReturnError(mg::box::ErrorRaise(mg::box::ERR_BOX_TIMEOUT));
	}

	inline void
	DomainToIPContext::ReturnError(
		mg::box::Error* aError)
	{
		MG_DEV_ASSERT(myIndex < 0);
		myCallback(myDomain.c_str(), {}, aError);
	}

	inline void
	DomainToIPContext::ReturnEndpoints(
		const std::vector<DomainEndpoint>& aEndpoints)
	{
		MG_DEV_ASSERT(myIndex < 0);
		myCallback(myDomain.c_str(), aEndpoints, nullptr);
	}

	void
	DomainToIPContext::Process()
	{
		MG_DEV_ASSERT(myIndex < 0);
#if IS_BUILD_DEBUG
		if (theDebugErrorCode != mg::box::ERR_BOX_NONE)
		{
			ReturnError(mg::box::ErrorRaise(theDebugErrorCode, "debug error"));
			return;
		}
#endif
		addrinfo* res = nullptr;
		int rc = getaddrinfo(myDomain.c_str(), nullptr, nullptr, &res);
		if (rc != 0)
		{
#if IS_PLATFORM_WIN
			ReturnError(mg::box::ErrorRaiseWSA(rc, "getaddrinfo()"));
#else
			if (rc == EAI_SYSTEM)
			{
				ReturnError(mg::box::ErrorRaiseErrno("getaddrinfo()"));
			}
			else
			{
				ReturnError(mg::box::ErrorRaiseFormat(
					mg::box::ERR_NET_ADDR_NOT_AVAIL, "getaddrinfo(): %s",
					gai_strerror(rc)));
			}
#endif
			if (res != nullptr)
				freeaddrinfo(res);
			return;
		}
		std::vector<DomainEndpoint> endpoints;
		for (const addrinfo* pos = res; pos != nullptr; pos = pos->ai_next)
		{
			if (pos->ai_family != AF_INET && pos->ai_family != AF_INET6)
				continue;
			DomainEndpoint point;
			point.myHost.Set(pos->ai_addr);
			bool isFound = false;
			for (const DomainEndpoint& e : endpoints)
			{
				if (e.myHost != point.myHost)
					continue;
				isFound = true;
				break;
			}
			if (!isFound)
				endpoints.push_back(std::move(point));
		}
		freeaddrinfo(res);

		mg::box::Error::Ptr err;
		if (endpoints.empty())
		{
			ReturnError(mg::box::ErrorRaise(mg::box::ERR_NET_ADDR_NOT_AVAIL,
				"no suitable IPs"));
		}
		else
		{
			ReturnEndpoints(endpoints);
		}
	}

#if IS_BUILD_DEBUG
	void
	DomainToIPDebugSetProcessLatency(
		uint64_t aValue)
	{
		theDebugProcessLatency = aValue;
	}

	void
	DomainToIPDebugSetError(
		mg::box::ErrorCode aCode)
	{
		theDebugErrorCode = aCode;
	}
#endif

	static DomainToIPWorker&
	DomainToIPGetInstance()
	{
		static DomainToIPWorker* worker = new DomainToIPWorker();
		return *worker;
	}


}
}
