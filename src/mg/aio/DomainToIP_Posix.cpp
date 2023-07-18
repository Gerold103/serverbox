#include "DomainToIP.h"

#include "mg/box/BinaryHeap.h"
#include "mg/box/DoublyList.h"
#include "mg/box/ForwardList.h"

namespace mg {
namespace aio {

	class DomainToIPRequest
	{
		SHARED_PTR_API(DomainToIPRequest, myRef)

	public:
		DomainToIPRequest(
			const char* aDomain,
			uint32_t aTimeout,
			const DomainToIPCallback& aCallback);

		void ReturnCancel();
		void ReturnTimeout();
		void ReturnError(
			mg::box::Error* aError);
		void ReturnEndpoints(
			const std::vector<DomainEndpoint>& aEndpoints);

		DomainToIPRequest* myNext;
		DomainToIPRequest* myPrev;
		DomainToIPRequest* myNextCancel;
		uint64_t myDeadline;
		int32_t myIndex;
		mg::box::RefCount myRef;
		std::string myDomain;
		DomainToIPCallback myCallback;
	};

	using DomainToIPHeap = mg::box::BinaryHeapMinIntrusive<DomainToIPRequest>;
	using DomainToIPForwardList = mg::box::ForwardList<DomainToIPRequest>;
	using DomainToIPDoublyList = mg::box::DoublyList<DomainToIPRequest>;
	using DomainToIPCancelList = mg::box::ForwardList<DomainToIPRequest,
		&DomainToIPRequest::myNextCancel>;

	class DomainToIPWorker
		: private mg::box::Thread
	{
	public:
		DomainToIPWorker();
		~DomainToIPWorker() = delete;

		void Post(
			DomainToIPRequest::Ptr& aOutRequest,
			const char* aDomain,
			uint32_t aTimeout,
			const DomainToIPCallback& aCallback);

	private:
		void Run() override;
		void PrivProcessWait();
		void PrivProcessMain();
		void PrivProcessFront();

		DomainToIPDoublyList myMain;
		DomainToIPHeap myMainHeap;

		DomainToIPForwardList myWait;
		DomainToIPForwardList myWaitCancel;

		mg::box::Mutex myMutex;
		mg::box::ConditionVariable myCond;
		DomainToIPForwardList myFront;
		DomainToIPCancelList myFrontCancel;
	};

	static DomainToIPWorker& DomainToIPGetInstance();

	//////////////////////////////////////////////////////////////////////////////////////

	void
	DomainToIPAsync(
		DomainToIPRequest::Ptr& aOutRequest,
		const char* aDomain,
		uint32_t aTimeout,
		const DomainToIPCallback& aCallback)
	{
		DomainToIPGetInstance().Post(aOutRequest, aDomain, aTimeout, aCallback);
	}

	bool
	DomainToIPBlocking(
		DomainToIPRequest::Ptr& aOutRequest,
		const char* aDomain,
		uint32_t aTimeout,
		mg::net::Host& aOutHost,
		mg::box::Error::Ptr& aOutError);

	//////////////////////////////////////////////////////////////////////////////////////

	inline
	DomainToIPWorker::DomainToIPWorker()
	{
		Start();
	}

	void
	DomainToIPWorker::Post(
		DomainToIPRequestPtr& aOutRequest,
		const char* aDomain,
		uint32_t aTimeout,
		const DomainToIPCallback& aCallback)
	{
		aOutRequest = DomainToIPRequest::NewShared(aDomain, aTimeout, aCallback);
		DomainToIPRequest* raw = aOutRequest.GetPointer();

		raw->PrivRef();
		myMutex.Lock();
		bool wasEmpty = myFront.IsEmpty();
		myFront.Append(raw);
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
			PrivProcessWait();
			PrivProcessMain();
			PrivProcessFront();
		}
	}

	void
	DomainToIPWorker::PrivProcessWait()
	{
		while (!myWait.IsEmpty())
		{
			DomainToIPRequest* req = myWait.PopFirst();
			myMain.Append(req);
			myMainHeap.Push(req);
		}
		while (!myWaitCancel.IsEmpty())
		{
			DomainToIPRequest* req = myWaitCancel.PopFirst();
			if (req->myIndex >= 0)
			{
				myMain.Remove(req);
				myMainHeap.Remove(req);
				req->ReturnCancel();
				req->PrivUnref();
			}
			req->PrivUnref();
		}
	}

	void
	DomainToIPWorker::PrivProcessMain()
	{
		DomainToIPRequest* req;
		uint64_t ts = mg::box::GetMilliseconds();
		while ((req = myHeap.GetTop())->myDeadline <= ts)
		{
			myHeap.RemoveTop();
			myMain.Remove(req);
			req->ReturnTimeout();
			req->PrivUnref();
		}
		int batchSize = 10;
		while (!myMain.IsEmpty() && --batchSize >= 0)
		{
			req = myMain.PopFirst();
			myHeap.Remove(req);

			std::vector<DomainEndpoint> endpoints;
			addrinfo* res = nullptr;
			int rc = getaddrinfo(req->myDomain.c_str(), nullptr, nullptr, &res);
			if (rc != 0)
			{
				int sysCode = errno;
				if (res != nullptr)
					freeaddrinfo(res);
				if (rc == EAI_SYSTEM)
				{
					req->ReturnError(mg::box::ErrorRaiseErrno(
						sysCode, "getaddrinfo()"));
				}
				else
				{
					req->ReturnError(mg::box::ErrorRaiseFormat(
						mg::box::ERR_NET_ADDR_NOT_AVAIL, "getaddrinfo(): %s",
						gai_strerror(rc)));
				}
				req->PrivUnref();
				continue;
			}
			for (addrinfo* pos = res; pos != nullptr; pos = pos->ai_next)
			{
				if (pos->ai_family != AF_INET && pos->ai_family != AF_INET6)
					continue;
				DomainEndpoint point;
				point.myHost.Set(pos->ai_addr);
				endpoints.push_back(std::move(point));
			}
			freeaddrinfo(res);

			mg::box::Error::Ptr err;
			if (endpoints.empty())
			{
				req->ReturnError(mg::box::ErrorRaise(
					mg::box::ERR_NET_ADDR_NOT_AVAIL, "no suitable IPs"));
			}
			else
			{
				req->ReturnEndpoints(endpoints);
			}
			req->PrivUnref();
		}
	}

	void
	DomainToIPWorker::PrivProcessFront()
	{
		uint64_t deadline = MG_DEADLINE_INFINITE;
		if (!myMainHeap.IsEmpty())
			deadline = myHeap.GetTop()->myDeadline;

		myMutex.Lock();
		if (myMain.IsEmpty() && myFront.IsEmpty() && myFrontCancel.IsEmpty())
		{
			uint64_t ts = mg::box::GetMilliseconds();
			if (ts < deadline)
			{
				bool isTimedOut = false;
				myCond.TimedWait(myMutex, deadline - ts, &isTimedOut);
			}
		}
		myWait.AppendList(std::move(myFront));
		myWaitCancel.AppendList(std::move(myFrontCancel));
		myMutex.Unlock();
	}

	//////////////////////////////////////////////////////////////////////////////////////

	DomainToIPRequest::DomainToIPRequest(
		const char* aDomain,
		uint32_t aTimeout,
		const DomainToIPCallback& aCallback)
		: myNext(nullptr)
		, myPrev(nullptr)
		, myNextCancel(nullptr)
		, myDeadline(mg::box::GetMilliseconds() + aTimeout)
		, myIndex(-1)
		, myDomain(aDomain)
	{
	}

	void
	DomainToIPRequest::ReturnCancel()
	{
		ReturnError(mg::box::ErrorRaise(mg::box::ERR_CANCEL));
	}

	void
	DomainToIPRequest::ReturnTimeout()
	{
		ReturnError(mg::box::ErrorRaise(mg::box::ERR_TIMEOUT));
	}

	void
	DomainToIPRequest::ReturnError(
		mg::box::Error* aError)
	{
		MG_DEV_ASSERT(myIndex < 0);
		myCallback(myDomain.c_str(), {}, aError);
	}

	void
	DomainToIPRequest::ReturnEndpoints(
		const std::vector<DomainEndpoint>& aEndpoints)
	{
		MG_DEV_ASSERT(myIndex < 0);
		myCallback(myDomain.c_str(), aEndpoints, nullptr);
	}

	static DomainToIPWorker&
	DomainToIPGetInstance()
	{
		static DomainToIPWorker* worker = new DomainToIPWorker();
		return *worker;
	}


}
}
