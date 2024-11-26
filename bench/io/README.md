# Serverbox AIO

The tests show `mg::aio` things perform. Specifically - `mg::aio::IOCore` and `mg::aio::TCPSocketIFace`.

To make it more interesting, the benchmarks compare those guys with `boost::asio` - `context`, `strand`, `ip::tcp::socket`.

The assumption was that one of the major boost problems would be that it heavily uses callbacks. Specifically, it requires to pass them for each read and write and any other network operation. That implies intense usage of the heap, to allocate and keep all those callbacks until the operations are done.

Another point, without any proofs, is that this boost `asio::context` most likely has a different scheduling algorithm than Serverbox. Supposedly less efficient when it comes to scaling horizontally over many threads. In particular, Serverbox' scheduling algo is `TaskScheduler`, located in `mg/sch` which implements something what could be called "Perfectly Fair Scheduling". Its main goal is to balance work across multiple threads evenly and in batches.

Besides, the benchmarks try to use different scheduling backends - `epoll`, `io_uring`, IO Completion Ports, `kqueue`. Usually one operating system has only one backend, but on Linux there are 2 - `epoll` and `io_uring`, which makes it fair to compare their different implementations (Serverbox epoll, Serverbox io_uring, Boost epoll, Boost io_uring).

The results of those comparisons follow below.

## Results: Serverbox vs Boost

The numbers depend very much on the hardware and test scenario. They are all available in the `results/` folder right here.

As expected, on all platforms in all scenarios Serverbox is faster than Boost in x1.1 - x5 times. Average seems to be around x1.5. The only exceptions are some runs on Windows in `weak_many_cores`. About those below.

Why Serverbox is so much faster, especially on large messages, could be explained by the assumptions made above in the intro:
- Serverbox doesn't require callbacks. At least not on the transport layer, where there is just a byte stream. Each byte is a "request". Instead, Serverbox allows to overload some virtual functions to listen for IO events. Indeed, even in Boost those callbacks are usually the same on each invocation. In Serverbox they are just regular functions provided once.
- Serverbox uses Perfectly Fair Scheduling strategy, which could explain why it scales so well. Even in the unfriendly Windows environment it managed to beat Boost on the massively parallel scenario.

Now why Windows was faster in 2 runs? When thread count was 10. But was slower on 1 thread and on 20 threads - how? The possible explanation is that on Windows boost doesn't seem to have any manual scheduling. Instead, (at the time of writing) it was using `GetQueuedCompletionStatus()` in each worker thread. This function returns one IO event from the kernel at a time, and is safe to call in multiple threads. In this setup the kernel or the Windows' built-in library seem to be doing the scheduling on their own. The Boost code just calls this in each worker in a loop.

However this function has no batching. It returns only one IO event at a time. If this is a syscall, then it is clear why the performance is like this. On the single thread case Serverbox wins because of batching - it uses `GetQueuedCompletionStatusEx()` to fetch many events at once. When thread count goes up, Boost becomes a bit faster, because the absence of batching is covered by just doing this syscall in more threads in parallel. But when thread count goes beyond a certain limit, Boost seems to loose probably because that number of syscalls is just too much overhead even in parallel.

Another explanation is that whatever `GetQueuedCompletionStatus()` uses internally as a queue, is just too different from Serverbox queues, and has different limitations.

The syscall explanation seems the most plausible.

## Results: `epoll` vs `io_uring`

`io_uring` is widely, almost fanatically, claimed to be the fastest possible IO scheduling tech in Linux, because it allows to batch multiple system calls into one, and not have buffer copying between user- and kernel-space.

And yet there are reports, where `io_uring` is quite suboptimal when it comes to networking:
- https://github.com/axboe/liburing/issues/189
- https://github.com/axboe/liburing/issues/536
- https://news.ycombinator.com/item?id=35548289
- https://ryanseipp.com/post/iouring-vs-epoll/

It seems that `io_uring` is in general slower than `epoll` and becomes faster only when super heavily tuned and `epoll` is limited in resources.

For instance, `io_uring` mostly makes sense with a single thread or one ring per thread. It might also need to be used with active polling which is basically a thread running in the kernel and busy-looping over the ring's queues. And finally, to get its no-copying feature the users evidently need to switch their buffering mechanism to the pre-allocated buffers attached to the ring.

All those requirements make `io_uring` not very usable for generic purpose networking.

It is clearly visible in the Serverbox and Boost `epoll` vs `io_uring` benchmarks, which are available in `results/io_uring/` folder. There `epoll` is faster than `io_uring` **up to x2.8**. Especially interesting how the ring is very much slower even in Boost, which is used worldwide and yet couldn't get it to work faster than `epoll`. Not faster, but even just with the same speed.

One could claim that `io_uring` is the king in the single thread usecase, but even there it is considerably slower, as the benchmarks show (look for "*Single thread*" scenario in the reports). Except for Boost, but Boost has shown itself slower than Serverbox in 100% runs, and Serverbox `io_uring` looses to Serverbox `epoll` even on the single thread run. Which means that Boost single thread usecase is simply suboptimal in general.

Why does it happen, despite the system call count being really lower with `io_uring`? The only reason I could think of is that `epoll`, when used properly (edge-triggered, socket stays in epoll until gets closed), attaches to the registered sockets just once, to listen for events. And this attachment perhaps is very expensive. On the other hand, `io_uring` has no registration mechanism - it needs to attach to each socket every time when an operation is submitted for that socket, and then detach when the operation is done. For instance, when requesting a read from a socket, this operation needs to be registered somewhere in the socket until the socket becomes readable.

Lets consider an example, imagine a usecase where 100 sockets need to be read 1000 times each. And a single socket has only one read in fly.

In `epoll` we would pay 100 attachment costs and 100 * 1000 system calls for reads. In `io_uring` we pay 1000 system calls (`io_uring_submit()`, each submitting 100 reads) and 100 * 1000 attachment costs. That would be:
```
- epoll: 100'000 syscalls,     100 attachments.
- io_uring: 1000 syscalls, 100'000 attachments.
```
The ring has many many less system calls, and still `epoll` is faster. Which makes an impression like the attachment to a socket to listen for events (like the socket becoming readable) is more expensive than a system call.

Perhaps the ring can get faster in the future, but at the time of writing it still looks very much raw and too slow compared to `epoll`.

## P.S.

The tests can be reproduced by anybody anywhere. When boost is not available, it won't be built and the benchmarks will simply give some absolute numbers how Serverbox network performs. When boost is available, it is built and used in the benches too, and can be compared with Serverbox.
