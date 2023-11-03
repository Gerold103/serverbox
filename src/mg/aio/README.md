## `IOCore`

**IOCore** is a multi-threaded event-loop for asynchronous code execution and network IO. It is like `TaskScheduler`, the internal architecture and the API are very similar, but `IOCore`'s main use case is work with sockets and efficient IO. You can think of it as if each of your sockets (server, accepted sockets, client sockets) is like a very lightweight coroutine.

A simple example:
```C++
// Global or not - you choose. It is not a singleton.
IOCore theCore;

class MyClient : private TCPSocketSubscription
{
public:
	MyClient(const Host& host) : myHost(host)
	{
		mySock = new TCPSocket(theCore);
		mySock->Open(TCPSocketParams{});
		// Receive with a 16KB buffer first time.
		mySock->PostRecv(16 * 1024);
		TCPSocketConnectParams connParams;
		connParams.myHost = &host;
		mySock->PostConnect(connParams, this);
	}

	void Send(const Message& msg)
	{
		BufferStream stream;
		msg.ToStream(stream);
		// Thread-safe send() from anywhere.
		mySock->PostSendRef(stream.TakeData());
	}

private:
	void OnRecv(BufferReadStream& stream) override
	{
		Message msg;
		while (msg.FromStream(stream))
		{
			// Your handling function.
			HandleMyMessage(msg);
		}
		// Keep receiving until close happens.
		mySock->Recv(16 * 1024);
	}

	void OnClose() override
	{
		// Reconnect with backoff 10ms after disconnect. Frequent
		// usecase not to reconnect too frequently.
		TCPSocketConnectParams connParams;
		connParams.myHost = &myHost;
		connParams.myDelay = 10;
		mySock->PostConnect(connParams, this);
	}

	TCPSocket* mySock;
	const Host myHost;
};
```

`IOCore` (`src/mg/aio/IOCore.h`) is the event-loop itself. `IOTask` (`src/mg/aio/IOTask.h`) is a task context which a socket can be attached to, or any other data of your choice. With a socket attached `IOTask` can do asynchronous IO. In addition to that it also has the task features - wakeup and deadlines, just like `Task` in `TaskScheduler`.

For convenience some of the most popular socket types are already implemented on top of `IOTask`, such as `TCPSocket` (`src/mg/aio/TCPSocket.h`) for pure TCP interaction and `TCPServer` (`src/mg/aio/TCPServer.h`) for accepting new clients.

For building more specific sockets you can use those as a basis, or directly inherit `TCPSocketIFace`.

#### Use cases
Not limited by these, but most typical:
- Fair load of middle-size multi-step requests (aka coroutines) which would want to send async sub-requests (to a database, etc) with deadlines.
- Massive network IO, thousands of sockets, millions of quick requests per second.
- Rare big network messages.

Among the coroutine-features for your tasks `IOCore` offers task sleeping, wakeup deadlines, explicit wakeups. It might be highly beneficial to combine `IOCore` with `TaskScheduler`. For example, to process IO in `IOCore` threads, and to execute the business-logic of your requests in `TaskScheduler`.

#### Fairness
The task execution is fair. It means they are not pinned to threads. IO system calls and task wakeups are all evenly spread across worker threads depending on their load. That in turn gives even CPU load on all workers and there is no IO starvation for any of the sockets. It will not happen that some tasks appear to be too heavy and occupy one thread's CPU 100% while other threads do nothing.

The execution is also fair in terms of order. Multiple data blocks posted into one socket are always sent into the network in the same order as they were posted in.

#### Performance
To be added. For now refer to `bench/BenchIO.cpp`. Preliminary tests show 50% speed up compared to boost ASIO on heavy load, and same or slightly better perf on small load.

#### Correctness
The algorithms used in `IOCore` are absolutely the same as in `TaskScheduler` which in turn is validated in TLA+ specifications to ensure absence of deadlocks, unintentional reordering, task loss, and other logical mistakes.

### Examples

To be added. For now refer to tests, `bench/BenchIOTCPClient.cpp`, `bench/BenchIOTCPServer.cpp`.

## Architecture overview

`IOCore` is the same as `TaskScheduler`, go have a look at it if you didn't yet. Further it is assumed that you know `TaskScheduler` internals.

The difference in `IOCore` is that internally it has one another queue next to the waiting queue - the kernel event queue. On Linux that would be `epoll`, on Windows - `IOCP` (IO Completion Ports), on Mac/BSD - `kqueue`. All sockets are stored in that kernel-queue. When the kernel reports an event, such as if the socket became writable or readable, `IOCore` saves that event inside `IOTask` and wakes the task up.

Another difference is that `IOTask`s don't need to be re-posted after each wakeup. They belong to the `IOCore` instance which they were posted into, and stay in there until closure. The reason is that the sockets stay inside `IOCore`'s kernel-queue and that forces to keep the tasks attached to `IOCore` too.
