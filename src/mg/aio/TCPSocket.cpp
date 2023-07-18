// ProjectFilter(Network)
#include "stdafx.h"

#include "mg/common/Error.h"

#include "mg/serverbox/NetworkMonitor.h"
#include "mg/serverbox/TCPSocket.h"

namespace mg {
namespace serverbox {

	TCPSocketParams::TCPSocketParams()
		: myRecvSize(0)
	{
	}

	void
	TCPSocket::Open(
		const TCPSocketParams& aParams)
	{
		ProtOpen(aParams.myRecvSize);
		mySendOffset = 0;
	}

	void
	TCPSocket::Execute(
		const IoCoreArgs& aArgs)
	{
		if (myTask.IsClosed())
			return ProtClose();
		if (!myTask.ProcessArgs(aArgs))
			return ProtCloseError(mg::common::ErrorRaiseSys("io"));

		ProtOnWakeup();
		PrivRecv();
		// Send must be in the end. Recv events could generate new
		// data for sending.
		PrivSend();
	}

	void
	TCPSocket::PrivSend()
	{
		// Event could contain a result delivered from a previous
		// execution loop iteration. Happens on Windows as all its
		// IO functions return data on next execution.
		if (!PrivSendEventConsume())
			return;
		const mg::network::WriteBuffer* wbuf = mySendQueue.GetFirst();
		if (wbuf == nullptr)
			return;
		myTask.Send(wbuf, mySendOffset, mySendEvent);
		if (!PrivSendEventConsume())
			return;
		// Finished right away. Either an error, or success which
		// happens on Linux only.
		// Don't do it in a loop. If has more data to send, do it
		// next time so as not to starve the other sockets.
		if (!mySendQueue.IsEmpty())
			myTask.Reschedule();
	}

	void
	TCPSocket::PrivSendAbort(
		mg::common::Error* aError)
	{
		ProtOnSendError(aError);
	}

	void
	TCPSocket::PrivSendCommit(
		uint32 aByteCount)
	{
		NetworkMonitor::GetInstance().UpdateOutgoing(aByteCount);
		mySendOffset = mySendQueue.SkipBytesStrict(mySendOffset, aByteCount);
		ProtOnSend(aByteCount);
	}

	bool
	TCPSocket::PrivSendEventConsume()
	{
		if (mySendEvent.IsLocked())
			return false;
		if (mySendEvent.IsEmpty())
			return true;
		if (mySendEvent.IsError())
		{
			PrivSendAbort(mg::common::ErrorRaiseSys(mySendEvent.PopError(), "send"));
			return false;
		}

		PrivSendCommit(mySendEvent.PopBytes());
		// Could be locked inside if an error has happened.
		return !mySendEvent.IsLocked();
	}

	void
	TCPSocket::PrivRecv()
	{
		// Event could contain a result delivered from a previous
		// execution loop iteration. Happens on Windows as all its
		// IO functions return data on next execution.
		if (!PrivRecvEventConsume())
			return;
		mg::network::WriteBuffer* wbuf = myRecvQueue.GetFirst();
		if (wbuf == nullptr)
			return;
		myTask.Recv(wbuf, myRecvEvent);
		if (!PrivRecvEventConsume())
			return;
		// Finished right away. Either an error, or success which
		// happens on Linux only.
		// Don't do in a loop. If has more data to recv, do it
		// next time so as not to starve the other sockets.
		myTask.Reschedule();
	}

	void
	TCPSocket::PrivRecvAbort(
		mg::common::Error* aError)
	{
		ProtOnRecvError(aError);
	}

	void
	TCPSocket::PrivRecvCommit(
		uint32 aByteCount)
	{
		if (aByteCount == 0)
		{
			// Graceful close.
			ProtClose();
			return;
		}
		NetworkMonitor::GetInstance().UpdateIncoming(aByteCount);
		mg::network::WriteBuffer* tail;
		mg::network::WriteBuffer::Ptr head(myRecvQueue.PopBytes(aByteCount, tail));
		ProtOnRecv(head.GetPointer(), tail, aByteCount);
		// Unref the received buffers explicitly, before
		// re-filling the recv-queue. There is a good chance, that
		// the listener copied or parsed the buffers, didn't need
		// to ref them, and they are going to be deleted now.
		//
		// Then they would be reused by the recv-queue
		// re-population below right from the thread-local pool.
		head.Clear();
		myRecvQueue.AppendBytes(aByteCount);
	}

	bool
	TCPSocket::PrivRecvEventConsume()
	{
		if (myRecvEvent.IsLocked())
			return false;
		if (myRecvEvent.IsEmpty())
			return true;
		if (myRecvEvent.IsError())
		{
			PrivRecvAbort(mg::common::ErrorRaiseSys(myRecvEvent.PopError(), "recv"));
			return false;
		}

		PrivRecvCommit(myRecvEvent.PopBytes());
		// Could be locked inside if an error has happened.
		return !myRecvEvent.IsLocked();
	}

} // serverbox
} // mg
