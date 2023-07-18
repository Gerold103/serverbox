#pragma once

#include "mg/box/Definitions.h"
#if IS_PLATFORM_WIN
#define MG_IOCORE_USE_IOCP 1
#define MG_IOCORE_USE_EPOLL 0
#define MG_IOCORE_USE_KQUEUE 0
#elif IS_PLATFORM_APPLE
#define MG_IOCORE_USE_IOCP 0
#define MG_IOCORE_USE_EPOLL 0
#define MG_IOCORE_USE_KQUEUE 1
#else
#define MG_IOCORE_USE_IOCP 0
#define MG_IOCORE_USE_EPOLL 1
#define MG_IOCORE_USE_KQUEUE 0
#endif

#if MG_IOCORE_USE_IOCP
#include "mg/box/ForwardList.h"
#endif

#include "mg/box/SharedPtr.h"

#include "mg/net/Socket.h"

F_DECLARE_STRUCT(mg, box, IOVec);
F_DECLARE_CLASS(mg, net, Buffer);
F_DECLARE_CLASS(mg, net, Host);

namespace mg {
namespace aio {

	class IOCore;

	// Windows IO events are WSAOVERLAPPED objects initialized by WSA functions
	// (WSASend/Recv/..()). Overlappeds are put into IOCP queue by the userspace, and
	// returned from the queue by the kernel, when the operation is finished.
	// Pointer at the overlapped is returned unchanged - it allows to inherit it in C
	// style, and attach any needed data in the parent struct, which is done below.
	//
	// On Linux and Apple the event is useful for platform-agnostic code which operates on
	// tasks and events instead of raw sockets and WSAOVERLAPPED objects. In such code it
	// is important though to remember, that any IO function would return the event with
	// result immediately. Not via IOCore.
	struct IOEvent
	{
	public:
		IOEvent();

		void Reset();
		void Lock();
		bool IsLocked() const;
		void Unlock();
		void UnlockForced();

		// Returns always false, because this is what is usually needed in case of an
		// error. Helps to write
		//
		//     return ReturnError();
		//
		// To enable tail-call optimization.
		bool ReturnError(
			mg::box::ErrorCode aError);

		// Returns always true. For the sake of tail-call optimization.
		bool ReturnBytes(
			uint32_t aByteCount);

		// Returns always true. For the sake of tail-call optimization.
		bool ReturnEmpty();

		bool IsEmpty() const;
		bool IsError() const;
		mg::box::ErrorCode GetError() const;
		mg::box::ErrorCode PopError();
		uint32_t GetBytes() const;
		uint32_t PopBytes();

#if MG_IOCORE_USE_IOCP
		// IOCP returns pointer at this overlapped, and the parent
		// struct is obtained using negative address offset to get
		// the struct beginning.
		WSAOVERLAPPED myOverlap;
		// One socket can get multiple events from IOCP before
		// they are dispatched. They are linked into a list.
		IOEvent* myNext;
#endif
	private:
		// Lock is not atomic. It is only intended for use in one
		// thread at a time. Usage is optional and can be various.
		// One example - use it as a flag of the event being in
		// progress inside of IoCore on Windows.
		// Another example - use it as a flag if event of this
		// type is even allowed now.
		// When event is used with task's IO methods, it means
		// the operation couldn't be completed right now. On
		// Linux it is EWOULDBLOCK, on Windows the event is sent
		// to IoCore and will be completed later.
		bool myIsLocked;
		bool myIsEmpty;
		bool myIsError;
		union
		{
			uint32_t myBytes;
			mg::box::ErrorCode myError;
		};
	};

#if MG_IOCORE_USE_IOCP
	using IOEventList = mg::box::ForwardList<IOEvent>;
#endif

	class IOArgs
	{
	public:
#if MG_IOCORE_USE_IOCP
		IOEvent* myEvents;
#else
		int myEvents;
#endif
	};

#if MG_IOCORE_USE_IOCP
	static constexpr uint32_t theAcceptAddrMinSize = sizeof(struct sockaddr_in6) + 16;

	// Windows requires quite a lot of context to be carried with each OVERLAPPED server
	// socket. It would be too expensive to store that in IOTask directly as it would also
	// affect non-server sockets.
	struct IOServerSocket
	{
		IOServerSocket();
		~IOServerSocket();

		// Socket being accepted right now. It needs to be created before calling
		// AcceptEx().
		mg::net::Socket myPeerSock;
		// Server socket itself is carried here only until post into IOCore. Then it is
		// moved into IOTask and is owned by it.
		mg::net::Socket mySock;
		// For each OVERLAPPED server socket Windows forces users to fetch its private
		// function pointers and carry them all the time together.
		LPFN_ACCEPTEX myAcceptExF;
		LPFN_GETACCEPTEXSOCKADDRS myGetAcceptExSockaddrsF;
		// Windows saves here the local and remote addrs of an accepted socket. It has to
		// be created before AcceptEx() and kept valid until accept happens. And it can't
		// be skipped, unfortunately.
		uint8_t myBuffer[theAcceptAddrMinSize * 2];
	};
#else
	// Windows is the only reason why this struct exists. To keep things the same on
	// different platforms. Linux and Apple don't really need this.
	struct IOServerSocket
	{
		IOServerSocket();
		~IOServerSocket();

		mg::net::Socket mySock;
	};
#endif

	class IOSubscription
	{
		SHARED_PTR_API(IOSubscription, myRef)
	public:
		virtual void OnEvent(
			const IOArgs& aArgs) = 0;

	protected:
		virtual ~IOSubscription() = default;

		mg::box::RefCount myRef;
	};

	enum IOTaskStatus
	{
		// Task is pending for dispatch in one of the queues in the core.
		IOTASK_STATUS_PENDING,
		// Task is not in any queue. It is waiting for events from the kernel or for
		// external events such as close or wakeup.
		IOTASK_STATUS_WAITING,
		// Task is ready for execution, so will be executed asap. Or is being executed
		// right now, and was woken up during the execution. The task can be anywhere.
		IOTASK_STATUS_READY,
		// Task closure was requested but not yet noticed by the core. The task can be
		// anywhere.
		IOTASK_STATUS_CLOSING,
		// Task socket was closed. It will be eventually given to a worker thread. The
		// task can be anywhere except the front queue.
		IOTASK_STATUS_CLOSED,
	};

	struct IOTask
	{
		IOTask();
		~IOTask();

		//////////////////////////////////////////////////////////////////////////////////
		// Control methods for task start, wakeup, stop, and alike.
		//

		// After post the task can't be deleted and its socket can't be closed manually.
		// It is owned by IOCore now, and must be closed only via Close() method.
		void Post(
			IOCore& aCore,
			mg::net::Socket aSocket,
			IOSubscription* aSub);
		void Post(
			IOCore& aCore,
			IOServerSocket* aSocket,
			IOSubscription* aSub);
		// Socket can be omitted in case it is not ready for listening to kernel events,
		// or is not created yet. It is useful when socket creation can be heavy - need to
		// create socket handle, setup options, register in IOCP or epoll, start
		// connecting. Then the task can be posted empty, woken up, and in a worker thread
		// attached to socket.
		void Post(
			IOCore& aCore,
			IOSubscription* aSub);

		// Schedule close of a socket. It will be eventually closed when requests
		// currently working with it are finished.
		//
		// The task can't be just 'unregistered'. It can only be closed. Because on
		// Windows there is no a legal way to remove a socket from IOCP without closing
		// it.
		//
		// Note:
		// * Can be called multiple times and any time.
		// * When called even before the task is posted first time, the task is closed in
		//   a worker thread right after the post.
		void Close();

		// Make the task wakeup as soon as possible regardless of where it is right now.
		// Even if the task currently is being executed, it will be scheduled for another
		// execution. After this call the task owner can be sure the listener will be
		// invoked again unless the task is already closed for good.
		// Note:
		//
		// * Can be called multiples times and any time.
		// * If done after closure, then it is nop.
		void Wakeup();

		//////////////////////////////////////////////////////////////////////////////////
		// In-worker methods for checking and handling events, work with the data, etc.
		//

		bool IsClosed() const;
		// Check if the task was woken up on a deadline, not (or not only) by IO, close,
		// or wakeup. Is always false for tasks which didn't have a deadline before waking
		// up.
		bool IsExpired() const;
		mg::net::Socket GetSocket() const;
		bool HasSocket() const;

		// Deadline is reset on each wakeup. Setting a new deadline works only if it is
		// lower than the previous deadline installed during the same task execution. That
		// allows to use the deadline from multiple independent parts of the task owner.
		void SetDeadline(
			uint64_t aDeadline);

		// Fast analogue of a wakeup. It can be used only from an IO worker thread. But is
		// order of magnitude faster, because does not use any atomics.
		void Reschedule();

#if MG_IOCORE_USE_EPOLL
		// Epoll is used in the edge-triggered mode. It means, that if an event is not
		// consumed, it won't trigger the socket again and it won't be returned from
		// epoll_wait() unless a newer event happens.
		//
		// In order not to loose events the task owner must save the non-consumed events.
		// Can only be called from event handler callbacks, when the task is being
		// executed in a worker thread.

		void SaveEventWritable();
		void SaveEventReadable();
#else
		// IOCP with WSA functions gives a clear concept of an operation with the center
		// in an WSAOVERLAPPED object. All the operations must be accounted to be able to
		// tell when all of them are finished when want to close and free the task safely.
		// Must call after a new event is sent to the kernel using WSA functions.
		// There is no public operation end, because it is done by IoCore internally.
		void OperationStart();
		// Useful when operation start notification was given and then something went
		// wrong so the owner is sure the operation won't be back via IOCP.
		void OperationAbort();

#endif
		// Check if the task is in the current thread, and this thread is an IO worker.
		// When it is in a worker, IO functions can be called right away. This helps to
		// reduce latency (no need to re-schedule the task), and save CPU (no need to
		// wakeup the task).
		bool IsInWorkerNow() const;

		// Being inside the core and not having a socket, attach to one right now.
		void AttachSocket(
			mg::net::Socket aSocket);

		//
		// IO functions for the task's socket.
		//
		// Behaviour on Unix-like and Windows is of course different. But with this API it
		// is possible to handle it regardless. Only need to account all possible
		// outcomes.
		//
		// If returns false, the event is finished and has an error result.
		//
		// If returns true, check the lock. If locked, the operation is in progress. Can't
		// get a result now. Unlock depends on the platform.
		//
		// If not locked, it is a success. Can get result byte count.
		//
		// Unlock for Linux can be done when get EPOLLIN/OUT events. On Windows can unlock
		// when the event is returned from IOCore by the pointer.
		//

		bool Send(
			const mg::box::IOVec* aBuffers,
			uint32_t aBufferCount,
			IOEvent& aEvent);

		bool Send(
			const mg::net::Buffer* aHead,
			uint32_t aByteOffset,
			IOEvent& aEvent);

		bool Recv(
			mg::box::IOVec* aBuffers,
			uint32_t aBufferCount,
			IOEvent& aEvent);

		bool Recv(
			mg::net::Buffer* aHead,
			IOEvent& aEvent);

		bool ConnectStart(
			mg::net::Socket aSocket,
			const mg::net::Host& aHost,
			IOEvent& aEvent);

		bool ConnectStart(
			const mg::net::Host& aHost,
			IOEvent& aEvent);

		bool ConnectUpdate(
			IOEvent& aEvent);

		// Accept a new client-socket if the task's server-socket is bound and listening.
		// It might complete right away with an error or a success, or start an async
		// operation. How to deal with it to make it work on any platform:
		//
		// * Valid socket returned = success. Use the socket. The event is emptied by this
		//     call.
		//
		// * Invalid socket + the event is locked = async work is started. Wait then. Go
		//     to sleep. This task will be woken up automatically when the work is done.
		//     Then you have to try to accept again.
		//
		// * Invalid socket + the event has an error = actual error. Handle it any way you
		//     want. Get it from event.PopError().
		//
		mg::net::Socket Accept(
			IOEvent& aEvent,
			mg::net::Host& aOutPeer);

		bool ProcessArgs(
			const IOArgs& aArgs,
			mg::box::Error::Ptr& aOutErr);
	public:
		// Comparison operator is public because is used by the waiting queue's binary
		// heap.
		bool operator<=(
			const IOTask& aTask) const;
	private:
		void PrivDumpReadyEvents(
			IOArgs& aOutArgs);
		void PrivTouch() const;
		bool PrivExecute();
		bool PrivCloseStart();
		void PrivCloseDo();
		void PrivCloseEndPlatform();
		void PrivAttach(
			IOSubscription* aListener,
			mg::net::Socket aSocket);
		void PrivConstructPlatform();
		void PrivDestructPlatform();

		mg::box::Atomic<IOTaskStatus> myStatus;
#if MG_IOCORE_USE_EPOLL
		// Epoll events to dispatch in a worker thread. Events are flushed here when the
		// task appears in the core via the front queue. And then cleared by a worker
		// thread. While the events are being handled, the worker may decide to put some
		// events back if they were not consumed. These saved events will be returned
		// again on the next invocation of the task. It makes the field accessed only by
		// one thread at a time, and therefore it can be non-atomic.
		int myReadyEvents;
		// Epoll events to flush to the task when it will appear in the core again. The
		// field is updated *only* by the core, and therefore can be non-atomic. Events
		// are added here by the core from epoll_wait() results while the task can be
		// anywhere, even in a worker thread.
		int myPendingEvents;
		// Last blocked read-event. Unlocked by EPOLLIN.
		IOEvent* myInEvent;
		// Last blocked write-event. Unlocked by EPOLLOUT. In case ever need to have
		// multiple events here, they could be linked into a list like on Windows.
		IOEvent* myOutEvent;
#else
		// Meaning of the events is exactly the same as for epoll but the events are
		// objects instead of flags.

		IOEventList myReadyEvents;
		int myReadyEventCount;

		IOEventList myPendingEvents;
		int myPendingEventCount;

		// Number of operations on the task which are not finished yet. It helps to track
		// when the task can be safely closed and removed from IOCP. The counter is
		// updated only by the worker thread owning the task right now, and is not atomic.
		int myOperationCount;

		// Null for non-server sockets. Otherwise stores some platform-specific extra data
		// needed for the server-socket to work.
		IoCoreServerSocket* myServerSock;
#endif
		IOSubscription::Ptr mySub;
		mg::net::Socket mySocket;
	public:
		// Next must be public, because it is used by intrusive queues.
		IOTask* myNext;
		// Deadline and index are public because are used by the waiting queue's binary
		// heap.
		uint64_t myDeadline;
		int32_t myIndex;
	private:
		// Atomic flag whether close was requested. It filters out all non-first close
		// requests.
		mg::box::AtomicBool myCloseGuard;
		bool myIsClosed;
		bool myIsInQueues;
		bool myIsExpired;
		IOCore* myCore;

		friend class IOCore;
	};

	mg::net::Socket SocketCreate(
		mg::net::SockAddrFamily aAddrFamily,
		mg::net::TransportProtocol aProtocol,
		mg::box::Error::Ptr& aOutErr);

	IOServerSocket* SocketBind(
		const mg::net::Host& aHost,
		mg::box::Error::Ptr& aOutErr);

	// Start listening on a previously bound socket.
	bool SocketListen(
		IOServerSocket* aSock,
		mg::box::Error::Ptr& aOutErr);

	//////////////////////////////////////////////////////////////////////////////////////

	inline
	IOEvent::IOEvent()
	{
		Reset();
	}

	inline void
	IOEvent::Reset()
	{
#if IS_COMPILER_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
		memset(this, 0, sizeof(*this));
#if IS_COMPILER_GCC
#pragma GCC diagnostic pop
#endif
		myIsEmpty = true;
	}

	inline void
	IOEvent::Lock()
	{
		MG_DEV_ASSERT(!myIsLocked);
		myIsLocked = true;
	}

	inline bool
	IOEvent::IsLocked() const
	{
		return myIsLocked;
	}

	inline void
	IOEvent::Unlock()
	{
		MG_DEV_ASSERT(myIsLocked);
		myIsLocked = false;
	}

	inline void
	IOEvent::UnlockForced()
	{
		myIsLocked = false;
	}

	inline bool
	IOEvent::ReturnError(
		mg::box::ErrorCode aError)
	{
		myIsEmpty = false;
		myIsError = true;
		myError = aError;
		return false;
	}

	inline bool
	IOEvent::ReturnBytes(
		uint32_t aByteCount)
	{
		myIsEmpty = false;
		myIsError = false;
		myBytes = aByteCount;
		return true;
	}

	inline bool
	IOEvent::ReturnEmpty()
	{
		MG_DEV_ASSERT(!myIsLocked);
		myIsEmpty = true;
		return true;
	}

	inline bool
	IOEvent::IsEmpty() const
	{
		MG_DEV_ASSERT(!myIsLocked);
		return myIsEmpty;
	}

	inline bool
	IOEvent::IsError() const
	{
		MG_DEV_ASSERT(!myIsLocked);
		MG_DEV_ASSERT(!myIsEmpty);
		return myIsError;
	}

	inline mg::box::ErrorCode
	IOEvent::GetError() const
	{
		MG_DEV_ASSERT(!myIsLocked);
		MG_DEV_ASSERT(!myIsEmpty);
		MG_DEV_ASSERT(myIsError);
		return myError;
	}

	inline mg::box::ErrorCode
	IOEvent::PopError()
	{
		MG_DEV_ASSERT(!myIsLocked);
		MG_DEV_ASSERT(!myIsEmpty);
		MG_DEV_ASSERT(myIsError);
		myIsEmpty = true;
		return myError;
	}

	inline uint32_t
	IOEvent::GetBytes() const
	{
		MG_DEV_ASSERT(!myIsLocked);
		MG_DEV_ASSERT(!myIsEmpty);
		MG_DEV_ASSERT(!myIsError);
		return myBytes;
	}

	inline uint32_t
	IOEvent::PopBytes()
	{
		MG_DEV_ASSERT(!myIsLocked);
		MG_DEV_ASSERT(!myIsEmpty);
		MG_DEV_ASSERT(!myIsError);
		myIsEmpty = true;
		return myBytes;
	}

	inline bool
	IOTask::IsClosed() const
	{
		PrivTouch();
		return myIsClosed;
	}

	inline bool
	IOTask::IsExpired() const
	{
		PrivTouch();
		return myIsExpired;
	}

	inline mg::net::Socket
	IOTask::GetSocket() const
	{
		PrivTouch();
		return mySocket;
	}

	inline bool
	IOTask::HasSocket() const
	{
		return GetSocket() != mg::net::theInvalidSocket;
	}

	inline void
	IOTask::SetDeadline(
		uint64_t aDeadline)
	{
		PrivTouch();
		if (aDeadline < myDeadline)
			myDeadline = aDeadline;
	}

	inline void
	IOTask::Reschedule()
	{
		MG_DEV_ASSERT(IsInWorkerNow());
		myDeadline = 0;
	}

	inline bool
	IOTask::operator<=(
		const IOTask& aTask) const
	{
		return myDeadline <= aTask.myDeadline;
	}

	inline void
	IOTask::PrivTouch() const
	{
		MG_DEV_ASSERT_F(!myIsInQueues,
			"An attempt to modify a task while it is in iocore queues");
	}

}
}
