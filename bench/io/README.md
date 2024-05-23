# Serverbox AIO

The tests show `mg::aio` things perform. Specifically - `mg::aio::IOCore` and `mg::aio::TCPSocketIFace`.

To make it more interesting, the benchmarks compare those guys with `boost::asio` - `context`, `strand`, `ip::tcp::socket`.

The assumption was that one of the major boost problems would be that it heavily uses callbacks. Specifically, it requires to pass them for each read and write and any other network operation. That implies intense usage of the heap, to allocate and keep all those callbacks until the operations are done.

Another point, without any proofs, is that this boost `asio::context` most likely has a different scheduling algorithm than Serverbox. Supposedly less efficient when it comes to scaling horizontally over many threads. In particular, Serverbox' scheduling algo is TaskScheduler, located in `mg/sch` which implements something what could be called "Perfectly Fair Scheduling". Its main goal is to balance work across multiple threads evenly and in batches.

The results of those comparisons follow below.

## Results

The numbers depend very much on the hardware and test scenario. They are all available in the `results/` folder right here.

As expected, on all platforms in all scenarios Serverbox is faster than Boost in x1.1 - x5 times. Average seems to be around x1.5. The only exceptions are some runs on Windows in `weak_many_cores`. About those below.

Why Serverbox is so much faster, especially on large messages, could be explained by the assumptions made above in the intro:
- Serverbox doesn't require callbacks. At least not on the transport layer, where there is just a byte stream. Each byte is a "request". Instead, Serverbox allows to overload some virtual functions to listen for IO events. Indeed, even in Boost those callbacks are usually the same on each invocation. In Serverbox they are just regular functions provided once.
- Serverbox uses Perfectly Fair Scheduling strategy, which could explain why it scales so well. Even in the unfriendly Windows environment it managed to beat Boost on the massively parallel scenario.

Now why Windows was faster in 2 runs? When thread count was 10. But was slower on 1 thread and on 20 threads - how? The possible explanation is that on Windows boost doesn't seem to have any manual scheduling. Instead, (at the time of writing) it was using `GetQueuedCompletionStatus()` in each worker thread. This function returns one IO event from the kernel at a time, and is safe to call in multiple threads. In this setup the kernel or the Windows' built-in library seem to be doing the scheduling on their own. The Boost code just calls this in each worker in a loop.

However this function has no batching. It returns only one IO event at a time. If this is a syscall, then it is clear why the performance is like this. On the single thread case Serverbox wins because of batching - it uses `GetQueuedCompletionStatusEx()` to fetch many events at once. When thread count goes up, Boost becomes a bit faster, because the absence of batching is covered by just doing this syscall in more threads in parallel. But when thread count goes beyond a certain limit, Boost seems to loose probably because that number of syscalls is just too much overhead even in parallel.

Another explanation is that whatever `GetQueuedCompletionStatus()` uses internally as a queue, is just too different from Serverbox queues, and has different limitations.

The syscall explanation seems the most plausible.

The tests can be reproduced by anybody anywhere. When boost is not available, it won't be built and the benchmarks will simply give some absolute numbers how Serverbox network performs. When boost is available, it is built and used in the benches too.
