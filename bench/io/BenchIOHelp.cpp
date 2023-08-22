#include "mg/box/StringFunctions.h"

#include <iostream>

namespace mg {
namespace bench {
namespace io {

	void
	BenchIOShowTutorial()
	{
		std::cout <<
R"--(BenchIO network stack benchmarking

#### The simplest test

    Console 1:

    $> bench_io -mode server

    Console 2:

    $> bench_io -mode client

That starts a server with 1 listening port, and a client. The client establishes one
connection to the server, and plays in ping-pong with it using 1KB messages.

Any number of the same clients can be started in separate processes or can be configured
to run in one process. See below.

#### Highload

    Console 1:

    $> bench_io -mode server -thread_count 5 -ports 12345,12346,12347,12348

    Console 2:

    $> bench_io -mode client -thread_count 5 -ports 12345,12346,12347,12348 \
           -connect_count_per_port 500 -message_int_count 50

That starts a server with 5 IOCore worker threads, which listens to 4 ports.

Client starts 5 IOCore worker threads. It also creates 2000 connections, 500 to each of
the server ports. It starts sending heavy-to-decode-messages consisting of 50 random
integers and 1KB buffer payload.

More of the same client processes can be started or you can increase the number of clients
in one process. Have to pay attention to the file descriptor count limit which often
defaults to 1024 and won't allow to open more connections.

Starting clients on different machines might make sense to create more realistic load
involving real network, and to put more pressure on the server than one client-machine
could provide.

#### Many small messages

	Console 1:

	$> bench_io -mode server -thread_count 5

	Console 2:

	$> bench_io -mode client -thread_count 5 -connect_count_per_port 500 \
	        -message_payload_size 128

This will start 500 clients sending small easy to decode 128 byte messages. The smaller
the message, and the more of them are in parallel (-message_parallel_count), the higher
will be the RPS.
)--";
		std::cout.flush();
	}

	void
	BenchIOShowHelp()
	{
		std::cout << "## BenchIO Help\n";
        std::cout <<
R"--(A command line utility to benchmark network code performance.

    -tutorial - Show a short tutorial how to quickly get started with the tool.

    -mode - Role of the instance to start: 'client' or 'server'.

    -thread_count - Number of IOCore worker threads. Default is the number of CPU cores.

    -ports - Comma separated list of ports. Server listens on them all, clients will
        connect to them all. Default is the single port 12345.

    -recv_size - Number of bytes in the receipt buffer of each socket. It means, a socket
        at once won't receive more than this. Default is 8192.

###### Client settings (-mode client)

    -connect_count_per_port - How many connections should be established to each port.
        Default is 1.

    -message_payload_size - Each message consists of a payload buffer and a set of
        integers. Payload helps to simulate big messages, but not too heavy decoding.
        Default is 1024.

    -message_int_count - Addition to the payload, help to simulate heavy decoding, like if
        each message is processed in some non-zero time duration. Default is 0.

    -message_parallel_count - How many messages are sent to each connection in parallel.
        For example, if the value is 10, then a client sends 10 messages from the
        beginning, waits one response, sends next message, waits next response, sends
        message, etc. So each connection always has 10 messages in fly. Default is 1.

    -target_host - Host where the server listens. Default is localhost.

    -disconnect_period - Disconnect a client each 'disconnect_period' messages. That
        allows to test how the server deals with reconnects under load. When 0 or omitted,
        no disconnects happen. Default is 0.

    -backend - Which networking backend to use. Available options are:
        * mg_aio (default) - use IOCore as the backend.
        * boost_asio )--"
#if !MG_BENCH_IO_HAS_BOOST
        R"--((not supported in this build) )--"
#endif
        R"--(- use boost::asio as the backend.
)--";

		std::cout.flush();
	}

}
}
}
