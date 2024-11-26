# Serverbox

![test](https://github.com/Gerold103/serverbox/actions/workflows/test.yml/badge.svg)

**Serverbox** is a framework for networking in C++. The purpose is similar to `boost::asio`. The focus is on these key points:

- **Simplicity** - the API is hard to misuse and is easy to understand.
- **Compactness** - the framework is small, both in code and binary size.
- **Speed** - extreme optimizations and efficiency for run-time and even compile-time.
- **Fairness** - huge accent on algorithms' fairness and even utilization of the CPU.

Being tens if not hundreds of times smaller than Boost, this small framework outperforms the former more than 50% in RPS on the same highload benchmark, while being on par in small loads.

The framework consists of several modules implementing things most needed on the backend: network IO, task scheduling, fast data structures and containers, and some smaller utilities.

The core features in the framework are `IOCore`  - a networking layer to accept clients, to send and receive data, and `TaskScheduler` - request processing engine. More about them below.

## `TaskScheduler`

**TaskScheduler** is a multi-threaded task processing engine for asynchronous code execution. It is the heart of the framework which is valuable on its own and its algorithm is the base for `IOCore`. The scheduler can be used for things from simple one-shot async callback execution to multistep complex pipelines with yields in between.

A very simple example:
```C++
TaskScheduler sched("name", thread_count, queue_size);
Task* t = new Task([](Task *self) {
	printf("Executed in scheduler!\n");
	delete self;
});
sched.Post(t);
```
The `Task` object is a context which can be just deleted right after single callback invocation, or can be attached to your own data and re-used across multiple steps of your pipeline, and can be used for deadlines, wakeups, signaling, etc.

It can also be used with C++20 coroutines:
```C++
Task* t = new Task();
t->SetCallback([](Task *self) -> mg::box::Coro {
	printf("Executed in scheduler!\n");
	self->SetDeadline(someDeadline);
	bool ok = co_await self->AsyncReceiveSignal();
	if (ok)
		printf("Received a signal");
	else
		printf("No signal");
	co_await self->AsyncExitDelete();
	co_return;
}(t));
sched.Post(t);
```

See more in `src/mg/sch/README.md`.

## `IOCore`

**IOCore** is a multi-threaded event-loop for asynchronous code execution and network IO. It is like `TaskScheduler`, the internal architecture and the API are very similar, but `IOCore`'s main use case is work with sockets and efficient IO. You can think of it as if each of your sockets (server, accepted sockets, client sockets) is like a very lightweight coroutine. See more in `src/mg/aio/README.md`.

A simple example:
```C++
// Global or not - you choose. The type is not a singleton.
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
		// Reconnect with backoff 10ms after disconnect. Easy way to avoid too frequent
		// reconnects. If reconnect is needed at all.
		TCPSocketConnectParams connParams;
		connParams.myHost = &myHost;
		connParams.myDelay = 10;
		// Can change socket params between reconnects if want.
		mySock->Open(TCPSocketParams{});
		mySock->PostConnect(connParams, this);
	}

	TCPSocket* mySock;
	const Host myHost;
};
```
`IOCore` core can be used as a task scheduler - `IOTask`s don't need to have sockets right from the start or at all. It can also be combined with `TaskScheduler` to execute your business logic in there, and do just IO in `IOCore`.

## Getting Started

### Dependencies

* At least C++11;
* A standard C++ library. On non-Windows also need `pthread` library.
* Compiler support:
	- Windows MSVC;
	- Clang;
	- GCC;
* OS should be any version of Linux, Windows, WSL, Mac. The actual testing was done on:
	- Windows 10;
	- WSLv1;
	- Debian 4.19;
	- MacOS Catalina 11.7.10;
	- Ubuntu 22.04.4 LTS;
* Supports only architecture x86-64. On ARM it might work, but wasn't compiled nor tested (yet);
* CMake. Compatible with `cmake` CLI and with VisualStudio CMake.

### Build and test

#### Configuration options

It is possible to choose certain things at the CMake configuration stage. Each option can be given to CMake using `-D<name>=<value>` syntax. For example, `-DMG_AIO_USE_IOURING=1`.

* `MG_AIO_USE_IOURING` - 1 = enable `io_uring` on Linux, 0 = use `epoll`. Default is 0.
* `MG_BOOST_USE_IOURING` - 1 = enable `io_uring` on Linux for `boost` in the benchmarks, 0 = use `epoll`. Default is 0.

#### Visual Studio
* Open VisualStudio;
* Select "Open a local folder";
* Select `serverbox/` folder where the main `CMakeLists.txt` is located;
* Wait for CMake parsing and configuration to complete;
* Build and run as a normal VS project.

#### CMake CLI
From root of the project:

```Bash
mkdir build
cd build
cmake ../
```
Now can run the tests: `./test/test`.

Useful tips (for clean project, from the `build` folder created above):
* Compiler switch:
	- clang:<br/>
	  `CC=clang CXX=clang++ cmake ../`;
	- GCC:<br/>
	  `CC=gcc CXX=g++ cmake ../`;
* Build type switch:
	- Release, all optimizations, no debug info:<br/>
	  `cmake -DCMAKE_BUILD_TYPE=Release ../`;
	- Release, all optimizations:<br/>
	  `cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ../`;
	- Debug, no optimization at all:<br/>
	  `cmake -DCMAKE_BUILD_TYPE=Debug ../`;
* Change C++ standard:
	`cmake -DCMAKE_CXX_STANDARD=11/14/17/20/...`;

### Installation

```Bash
mkdir build
cd build
# Your own install directory is optional.
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(pwd)/installed ../
make install
```
This creates a folder `installed/` which contains the library binaries and headers. The libraries are self-sufficient and isolated. It means you can take just `TaskScheduler` headers and static library, or just `IOCore`'s, or just basic `libmgbox` and its headers. Or any combination of those.

#### Stubs

Some of the libraries might have **intentionally undefined symbols**. For example, `mg/box` doesn't have logging functions defined (`mg::box::LogV()`). That is done so as you could define them in your code to intercept logging output, statistics, contention tracking, etc.

If none of the proposed interception endpoints are needed, you can simply link with a stub-library to use the defaults, such as `libmgboxstub`.

### Further info

See more details here:

- `src/mg/sch/README.md`
- `src/mg/aio/README.md`
