#include "TCPSocket.h"

namespace mg {
namespace aio {

	TCPSocketParams::TCPSocketParams()
	{
	}

	TCPSocket::TCPSocket(
		IOCore& aCore)
		: TCPSocketIFace(aCore)
		, mySendOffset(0)
	{
	}

	void
	TCPSocket::Open(
		const TCPSocketParams&)
	{
		ProtOpen();
		mySendOffset = 0;
	}

	void
	TCPSocket::OnEvent(
		const IOArgs& aArgs)
	{
		if (myTask.IsClosed())
			return ProtClose();
		mg::box::Error::Ptr err;
		if (!myTask.ProcessArgs(aArgs, err))
			return ProtCloseError(mg::box::ErrorRaise(err, "io"));

		ProtOnEvent();
		PrivRecv();
		// Send must be in the end. Previous events could generate new data for sending.
		PrivSend();
	}

	void
	TCPSocket::PrivSend()
	{
		// Event could contain a result delivered from a previous execution loop
		// iteration. Happens on Windows as all its IO functions return data on next
		// execution.
		if (!PrivSendEventConsume())
			return;
		const mg::net::BufferLink* link = mySendQueue.GetFirst();
		if (link == nullptr)
			return;
		myTask.Send(link, mySendOffset, mySendEvent);
		if (!PrivSendEventConsume())
			return;
		// Finished right away. Either an error, or success which happens on Unix only.
		// Don't do it in a loop. If has more data to send, do it next time so as not to
		// starve the other tasks.
		if (!mySendQueue.IsEmpty())
			myTask.Reschedule();
	}

	void
	TCPSocket::PrivSendAbort(
		mg::box::Error* aError)
	{
		ProtOnSendError(aError);
	}

	void
	TCPSocket::PrivSendCommit(
		uint32_t aByteCount)
	{
		mySendQueue.SkipData(mySendOffset, aByteCount);
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
			PrivSendAbort(mg::box::ErrorRaise(mySendEvent.PopError(), "send"));
			return false;
		}
		PrivSendCommit(mySendEvent.PopBytes());
		return true;
	}

	void
	TCPSocket::PrivRecv()
	{
		// Event could contain a result delivered from a previous execution loop
		// iteration. Happens on Windows as all its IO functions return data on next
		// execution.
		if (!PrivRecvEventConsume())
			return;
		if (myRecvSize == 0)
		{
			MG_BOX_ASSERT(myRecvQueue.IsEmpty());
			return;
		}
		myRecvQueue.EnsureWriteSize(myRecvSize);
		myTask.Recv(myRecvQueue.GetWritePos(), myRecvEvent);
		if (!PrivRecvEventConsume())
			return;
		// Finished right away. Either an error, or success which happens on Linux only.
		// Don't do in a loop. If has more data to recv, do it next time so as not to
		// starve the other tasks.
		if (myRecvSize > 0)
			myTask.Reschedule();
	}

	void
	TCPSocket::PrivRecvAbort(
		mg::box::Error* aError)
	{
		ProtOnRecvError(aError);
	}

	void
	TCPSocket::PrivRecvCommit(
		uint32_t aByteCount)
	{
		if (aByteCount == 0)
		{
			// Graceful close.
			ProtClose();
			return;
		}
		myRecvQueue.PropagateWritePos(aByteCount);
		myRecvSize = 0;
		{
			mg::net::BufferReadStream stream(myRecvQueue);
			ProtOnRecv(stream);
		}
		if (myRecvSize == 0)
			myRecvQueue.Clear();
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
			PrivRecvAbort(mg::box::ErrorRaise(myRecvEvent.PopError(), "recv"));
			return false;
		}
		PrivRecvCommit(myRecvEvent.PopBytes());
		return true;
	}

}
}
