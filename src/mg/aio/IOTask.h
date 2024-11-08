#pragma once

#include "mg/box/Definitions.h"
#if IS_PLATFORM_WIN
#define MG_IOCORE_USE_IOCP 1
#define MG_IOCORE_USE_EPOLL 0
#define MG_IOCORE_USE_KQUEUE 0
#define MG_IOCORE_USE_IOURING 0
#elif IS_PLATFORM_APPLE
#define MG_IOCORE_USE_IOCP 0
#define MG_IOCORE_USE_EPOLL 0
#define MG_IOCORE_USE_KQUEUE 1
#define MG_IOCORE_USE_IOURING 0
#elif MG_IOCORE_USE_IOURING
#define MG_IOCORE_USE_IOCP 0
#define MG_IOCORE_USE_EPOLL 0
#define MG_IOCORE_USE_KQUEUE 0
#define MG_IOCORE_USE_IOURING 1
#else
#define MG_IOCORE_USE_IOCP 0
#define MG_IOCORE_USE_EPOLL 1
#define MG_IOCORE_USE_KQUEUE 0
#define MG_IOCORE_USE_IOURING 0
#endif

#if MG_IOCORE_USE_IOCP || MG_IOCORE_USE_IOURING
#include "mg/box/ForwardList.h"
#endif
#include "mg/box/IOVec.h"
#include "mg/box/SharedPtr.h"
#include "mg/net/Socket.h"

#if MG_IOCORE_USE_IOCP
#include <mswsock.h>
#endif

F_DECLARE_STRUCT(mg, box, IOVec)
F_DECLARE_CLASS(mg, net, Buffer)
F_DECLARE_CLASS(mg, net, BufferLink)

namespace mg {
namespace aio {

	class IOCore;
	struct IOTask;

#if MG_IOCORE_USE_KQUEUE
	struct IOKqueueEvents
	{
		IOKqueueEvents();

		void Merge(const IOKqueueEvents& events);
		bool IsEmpty() const;

		bool myHasRead;
		bool myHasWrite;
		mg::box::ErrorCode myError;
	};
#endif

#if MG_IOCORE_USE_IOURING
	static constexpr uint32_t theIOUringMaxBufCount = 128;

	enum IOUringOpcode
	{
		MG_IO_URING_OP_NOP,
		MG_IO_URING_OP_CONNECT,
		MG_IO_URING_OP_ACCEPT,
		MG_IO_URING_OP_RECVMSG,
		MG_IO_URING_OP_SENDMSG,
		MG_IO_URING_OP_CANCEL_FD,
		MG_IO_URING_OP_READ,
	};

	struct IOUringParamsIOMsg
	{
		int myFd;
		msghdr myMsg;
		mg::box::IOVec myData[theIOUringMaxBufCount];
	};

	struct IOUringParamsAccept
	{
		int myFd;
		union
		{
			mg::net::Sockaddr base;
			mg::net::SockaddrIn in;
			mg::net::SockaddrIn6 in6;
		} myAddr;
		mg::net::SocklenT myAddrLen;
	};

	struct IOUringParamsConnect
	{
		int myFd;
		union
		{
			mg::net::Sockaddr base;
			mg::net::SockaddrIn in;
			mg::net::SockaddrIn6 in6;
		} myAddr;
		mg::net::SocklenT myAddrLen;
	};

	struct IOUringParamsRead
	{
		int myFd;
		void* myBuf;
		uint64_t mySize;
	};

	struct IOUringParamsCancelFd
	{
		int myFd;
	};

	// The accepted sockets are going to be returned in the 'bytes' field of the event.
	// Hence should fit into the byte count type.
	static_assert(sizeof(mg::net::Socket) == sizeof(uint32_t), "socket is int");
#endif

	// Windows IO events are WSAOVERLAPPED objects initialized by WSA functions
	// (WSASend/Recv/..()). Overlappeds are put into IOCP queue by the userspace, and
	// returned from the queue by the kernel, when the operation is finished.
	// Pointer at the overlapped is returned unchanged - it allows to inherit it in C
	// style, and attach any needed data in the parent struct, which is done below.
	//
	// On Linux io_uring the events are submission and completion queue entries. They are
	// stored in cyclic queues of the ring. Operations never end right away. They always
	// get sent as submission entries, and later produce a completion entry. The entries
	// use IOEvent as user-data to link the low-level operations with high-level data.
	//
	// On Linux epoll and Apple kqueue the event is useful for platform-agnostic code
	// which operates on tasks and events instead of raw sockets and WSAOVERLAPPED
	// objects. In such code it is important though to remember, that any IO function
	// would return the event with result immediately. Without going a full circle through
	// IOCore first.
	//
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

		// Returns always true. For tail-call optimization.
		bool ReturnBytes(
			uint32_t aByteCount);

		// Returns always true. For tail-call optimization.
		bool ReturnEmpty();

		bool IsEmpty() const;
		bool IsError() const;
		mg::box::ErrorCode GetError() const;
		mg::box::ErrorCode PopError();
		uint32_t GetBytes() const;
		uint32_t PopBytes();

#if MG_IOCORE_USE_IOCP
		WSAOVERLAPPED myOverlap;
		// One socket can get multiple events from IOCP before they are dispatched. They
		// are linked into a list.
		IOEvent* myNext;
#elif MG_IOCORE_USE_IOURING
		IOUringOpcode myOpcode;
		union
		{
			IOUringParamsIOMsg myParamsIOMsg;
			IOUringParamsAccept myParamsAccept;
			IOUringParamsConnect myParamsConnect;
			IOUringParamsRead myParamsRead;
			IOUringParamsCancelFd myParamsCancelFd;
		};
		// One socket can get multiple events from io_uring before they are dispatched.
		// They are linked into a list.
		IOEvent* myNext;
		// io_uring takes a single user data pointer. If it would be the task, then the
		// result couldn't be matched to the source event. If it would be the event, then
		// how to wakeup the owner-task? The only way is to store the event as user data
		// in io_uring and link it with the task implicitly via the event.
		IOTask* myTask;
#endif
	private:
		// Lock is not atomic. It is only intended for use in one thread at a time. Usage
		// is optional and can vary. One example - use it as a flag of the event being in
		// progress inside of IOCore on Windows. Another example - use it as a flag if
		// event of this type is even allowed now. Or set it when get EWOULDBLOCK on
		// Linux.
		//
		// When event is used with task's IO methods, it means the operation couldn't be
		// completed right now. On Linux epoll and Apple kqueue it is EWOULDBLOCK, on
		// Linux io_uring and on Windows IOCP the event is sent to IOCore and will be
		// completed later.
		bool myIsLocked;
		bool myIsEmpty;
		bool myIsError;
		union
		{
			uint32_t myBytes;
			mg::box::ErrorCode myError;
		};
	};

#if MG_IOCORE_USE_IOCP || MG_IOCORE_USE_IOURING
	using IOEventList = mg::box::ForwardList<IOEvent>;
#endif

	class IOArgs
	{
	public:
#if MG_IOCORE_USE_IOCP || MG_IOCORE_USE_IOURING
		IOEvent* myEvents;
#elif MG_IOCORE_USE_EPOLL
		int myEvents;
#elif MG_IOCORE_USE_KQUEUE
		IOKqueueEvents myEvents;
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
		// Needed to understand what socket family to use for newly accepted sockets.
		mg::net::SockAddrFamily myAddFamily;
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
	protected:
		SHARED_PTR_REF(myRef)
	public:
		SHARED_PTR_TYPE(IOSubscription)

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
		// Task is not in any queue. It is waiting for events from the kernel; or for a
		// deadline; or for external events such as close or wakeup.
		IOTASK_STATUS_WAITING,
		// Task is ready for execution, so will be executed asap. Or is being executed
		// right now, and was woken up during the execution. The task can be anywhere.
		IOTASK_STATUS_READY,
		// Task closure was requested but not yet noticed by the core. The task can be
		// anywhere.
		IOTASK_STATUS_CLOSING,
		// Task socket was closed. It will be eventually given to a worker thread for the
		// final execution. The task can be anywhere but the front queue.
		IOTASK_STATUS_CLOSED,
	};

	struct IOTask
	{
		IOTask(
			IOCore& aCore);
		~IOTask();

		//////////////////////////////////////////////////////////////////////////////////
		// Control methods for task start, wakeup, stop, and alike.
		//

		// After post the task can't be deleted and its socket can't be closed manually.
		// It is owned by IOCore now, and must be closed only via Close() method.
		void Post(
			mg::net::Socket aSocket,
			IOSubscription* aSub);
		void Post(
			IOServerSocket* aSocket,
			IOSubscription* aSub);
		// Socket can be omitted in case it is not ready for listening to kernel events,
		// or is not created yet. It is useful when socket creation can be heavy - need to
		// create socket handle, setup options, register in IOCP or epoll, start
		// connecting. Then the task can be posted empty, woken up, and in a worker thread
		// attached to socket.
		//
		// Can also be used for socket-less tasks just for doing non-IO work.
		void Post(
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
		//     a worker thread right after the post.
		void PostClose();

		// Make the task wakeup as soon as possible regardless of where it is right now.
		// Even if the task currently is being executed, it will be scheduled for another
		// execution. After this call the task owner can be sure the listener will be
		// invoked again unless the task is already closed for good.
		// Note:
		//
		// * Can be called multiples times and any time.
		// * If done after closure, then it is nop.
		void PostWakeup();

		//////////////////////////////////////////////////////////////////////////////////
		// In-worker methods for checking and handling events, work with the data, etc.
		//

		// When closed, the task won't be re-posted back into IOCore automatically. Can be
		// safely deleted or reused.
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
		// incomparably faster, because does not use any atomics.
		void Reschedule();

#if MG_IOCORE_USE_EPOLL || MG_IOCORE_USE_KQUEUE
		// Epoll is used in the edge-triggered mode. It means, that if an event is not
		// consumed, it won't trigger the socket again and it won't be returned from
		// epoll_wait() unless a newer event happens.
		//
		// In order not to loose events the task owner must save the non-consumed events.
		// Can only be called from event handler callbacks, when the task is being
		// executed in a worker thread.
		//
		// This is done automatically when task's IO methods are used.

		void SaveEventWritable();
		void SaveEventReadable();
#elif MG_IOCORE_USE_IOCP
		// IOCP with WSA functions gives a clear concept of an operation with the center
		// in an WSAOVERLAPPED object. All the operations must be accounted to be able to
		// tell when all of them are finished when want to close and free the task safely.
		// Must call after a new event is sent to the kernel using WSA functions.
		// There is no public operation end, because it is done by IOCore internally.
		//
		// This is done automatically when task's IO methods are used.
		void OperationStart();
		// Useful when operation start notification was given and then something went
		// wrong so the owner is sure the operation won't be back via IOCP.
		//
		// This is done automatically when task's IO methods are used.
		void OperationAbort();
#elif MG_IOCORE_USE_IOURING
		// In io_uring each sqe gets an cqe as a result. All the submitted sqes must be
		// accounted to be able to tell if all of them are finished when want to close and
		// free the task safely. Doing so without waiting for all the events to complete
		// would result into getting them anyway after the task is already deleted.
		//
		// This is done automatically when task's IO methods are used.
		void OperationStart();
#else
		#error "Unknown backend"
#endif
		// Check if the task is in the current thread, and this thread is an IO worker.
		// When it is in a worker, IO functions can be called right away. This helps to
		// reduce latency (no need to re-schedule the task), and save CPU (no need to
		// wakeup the task). Same as boost ASIO strand.running_in_this_thread().
		bool IsInWorkerNow() const;

		// Being inside the core and not having a socket, attach to one right now.
		void AttachSocket(
			mg::net::Socket aSocket);

		//
		// IO functions for the task's socket.
		//
		// Behaviour on various OS backends is of course different. Some finish the
		// operation always right away (direct send/recv with epoll or kqueue scheduler),
		// some always async (io_uring), some would do both (IOCP). But with this API it
		// is possible to handle it regardless. Only need to account all possible
		// outcomes.
		//
		// If returns false, the event is finished and has an error result.
		//
		// If returns true, check the lock. If locked, the operation is in progress. Can't
		// get a result now. Unlock happens when IOArgs are processed on a future wakeup.
		//
		// If returns true and not locked, the it is a success. Can get the result byte
		// count.
		//

		bool Send(
			const mg::box::IOVec* aBuffers,
			uint32_t aBufferCount,
			IOEvent& aEvent);

		bool Send(
			const mg::net::BufferLink* aHead,
			uint32_t aByteOffset,
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
		// * Invalid socket + no error means couldn't accept due to a non-critical error.
		//     The task is already re-scheduled to try on next wakeup again.
		//
		mg::net::Socket Accept(
			IOEvent& aEvent,
			mg::net::Host& aOutPeer);

		// Do that on each wakeup to unlock incoming events.
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
		void PrivCloseEnd();
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
		// Currently blocked read-event. Unlocked by EPOLLIN.
		IOEvent* myInEvent;
		// Currently blocked write-event. Unlocked by EPOLLOUT.
		IOEvent* myOutEvent;
#elif MG_IOCORE_USE_KQUEUE
		// Kqueue events to dispatch in a worker thread. Events are flushed here when the
		// task appears in the core via the front queue. And then cleared by a worker
		// thread. While the events are being handled, the worker may decide to put some
		// events back if they were not consumed. These saved events will be returned
		// again on the next invocation of the task. It makes the field accessed only by
		// one thread at a time, and therefore it can be non-atomic.
		IOKqueueEvents myReadyEvents;
		// Epoll events to flush to the task when it will appear in the core again. The
		// field is updated *only* by the core, and therefore can be non-atomic. Events
		// are added here by the core from kevent() results while the task can be
		// anywhere, even in a worker thread.
		IOKqueueEvents myPendingEvents;
		// Currently blocked read-event. Unlocked by EVFILT_READ.
		IOEvent* myInEvent;
		// Currently blocked write-event. Unlocked by EVFILT_WRITE.
		IOEvent* myOutEvent;
#elif MG_IOCORE_USE_IOCP
		// Meaning of the events is exactly the same as for epoll but the events are
		// objects instead of flags.
		IOEventList myReadyEvents;
		int myReadyEventCount;
		IOEventList myPendingEvents;
		int myPendingEventCount;

		// Number of operations on the task which are not finished yet. It helps to track
		// when the task can be safely closed and removed from IOCP. The counter is
		// updated only by the worker thread owning the task right now, hence is not
		// atomic.
		int myOperationCount;

		// Null for non-server sockets. Otherwise stores some platform-specific extra data
		// needed for the server-socket to work.
		IOServerSocket* myServerSock;
#elif MG_IOCORE_USE_IOURING
		// io_uring operates in 2 cyclic buffers - one of them is for submission. It is
		// only thread-safe for single-producer-single-consumer usage. The tasks can't
		// submit the operations right away. Instead, they put them into this to-submit
		// queue which is later flushed by the scheduler into io_uring.
		IOEventList myToSubmitEvents;
		// Same as IOCP.
		IOEventList myReadyEvents;
		int myReadyEventCount;
		IOEventList myPendingEvents;
		int myPendingEventCount;
		int myOperationCount;
		// A special event for io_uring. Unlike IOCP, a socket close won't cancel all the
		// running operations. They just hang. Unless cancelled explicitly and manually.
		// This event is pre-allocated for cancelling all in-progress operations on a
		// closing socket.
		IOEvent myCancelEvent;
#else
	#error "Uknown aio backend"
#endif
		IOSubscription::Ptr mySub;
		mg::net::Socket mySocket;
	public:
		// 'Next' is public because is used by intrusive lists.
		IOTask* myNext;
		// Index is public because is used by the binary heap.
		int32_t myIndex;
	private:
		// Atomic flag whether close was requested. It filters out all non-first close
		// requests.
		mg::box::AtomicBool myCloseGuard;
		uint64_t myDeadline;
		bool myIsClosed;
		bool myIsInQueues;
		bool myIsExpired;
		IOCore& myCore;

		friend class IOCore;
	};

	mg::net::Socket SocketCreate(
		mg::net::SockAddrFamily aAddrFamily,
		mg::net::TransportProtocol aProtocol,
		mg::box::Error::Ptr& aOutErr);

	IOServerSocket* SocketBind(
		const mg::net::Host& aHost,
		mg::box::Error::Ptr& aOutErr);

	bool SocketListen(
		IOServerSocket* aSock,
		uint32_t aBacklog,
		mg::box::Error::Ptr& aOutErr);

	//////////////////////////////////////////////////////////////////////////////////////
#if MG_IOCORE_USE_KQUEUE

	inline
	IOKqueueEvents::IOKqueueEvents()
		: myHasRead(false)
		, myHasWrite(false)
		, myError(mg::box::ERR_BOX_NONE)
	{}

	inline void
	IOKqueueEvents::Merge(const IOKqueueEvents& events)
	{
		myHasRead |= events.myHasRead;
		myHasWrite |= events.myHasWrite;
		if (myError == mg::box::ERR_BOX_NONE &&
			events.myError != mg::box::ERR_BOX_NONE)
			myError = events.myError;
	}

	inline bool
	IOKqueueEvents::IsEmpty() const
	{
		return !myHasRead && !myHasWrite && myError == mg::box::ERR_BOX_NONE;
	}

#endif
	//////////////////////////////////////////////////////////////////////////////////////

	inline
	IOEvent::IOEvent()
	{
		Reset();
	}

	inline void
	IOEvent::Reset()
	{
#if IS_COMPILER_GCC && (__GNUC__ > 8 || (__GNUC__ == 8 && __GNUC_MINOR__ >= 1))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
		memset(this, 0, sizeof(*this));
#if IS_COMPILER_GCC && (__GNUC__ > 8 || (__GNUC__ == 8 && __GNUC_MINOR__ >= 1))
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

	//////////////////////////////////////////////////////////////////////////////////////

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
