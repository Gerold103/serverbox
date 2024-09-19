#include "SSLSocket.h"

#include "mg/aio/TCPSocketCtl.h"
#include "mg/box/Error.h"
#include "mg/box/IOVec.h"
#include "mg/box/Log.h"

#define DEBUG_LOG MG_UNUSED // MG_LOG_DEBUG

namespace mg {
namespace aio {

	SSLSocketParams::SSLSocketParams()
		: mySSL(nullptr)
		, myHostName(nullptr)
		, myDoHandshake(true)
	{
	}

	SSLSocket::SSLSocket(
		IOCore& aCore)
		: TCPSocketIFace(aCore)
		, mySSL(nullptr)
	{
	}

	void
	SSLSocket::Open(
		const SSLSocketParams& aParams)
	{
		DEBUG_LOG("SSLSocket::Open.01", "%p", (void*)this);
		ProtOpen();
		myEncSendQueue.Clear();
		mySentSize = 0;
		delete mySSL;
		mySSL = new mg::net::SSLStream(aParams.mySSL);
		if (aParams.myDoHandshake)
			mySSL->Connect();
		if (aParams.myHostName != nullptr)
			mySSL->SetHostName(aParams.myHostName);
		// Report the socket as connected only after the SSL handshake is finished. This
		// should help not to do potentially expensive work in OnConnect() if the client
		// would fail anyway.
		ProtPostHandshake(new TCPSocketHandshake(
			[this](TCPSocketHandshake*) { return PrivHandshake(); }));
	}

	void
	SSLSocket::Handshake()
	{
		DEBUG_LOG("SSLSocket::Handshake.01", "%p", (void*)this);
		MG_BOX_ASSERT(myTask.IsInWorkerNow());
		MG_BOX_ASSERT(!mySSL->IsEncrypted());
		// Move all the already stacked non-encrypted data to the non-encrypted SSL
		// buffer. Because it looks naturally that the encryption affects only the data
		// sent after it was enabled.
		uint32_t offset = 0;
		while (true)
		{
			mySendQueue.SkipEmptyPrefix(offset);
			MG_DEV_ASSERT(offset == 0);
			const mg::net::BufferLink* link = mySendQueue.GetFirst();
			if (link == nullptr)
				break;
			const mg::net::Buffer* buf = link->myHead.GetPointer();
			MG_DEV_ASSERT(buf != nullptr);
			MG_DEV_ASSERT(buf->myPos > 0);
			mySentSize += buf->myPos;
			mySSL->AppendAppInputCopy(buf->myRData, buf->myPos);
			mySendQueue.SkipData(offset, buf->myPos);
			MG_DEV_ASSERT(offset == 0);
		}
		mySSL->Connect();
	}

	SSLSocket::~SSLSocket()
	{
		DEBUG_LOG("SSLSocket::Destroy.01", "%p", (void*)this);
		delete mySSL;
	}

	void
	SSLSocket::OnEvent(
		const IOArgs& aArgs)
	{
		DEBUG_LOG("SSLSocket::OnEvent.01", "%p", (void*)this);
		if (myTask.IsClosed()) {
			DEBUG_LOG("SSLSocket::OnEvent.02", "%p closed", (void*)this);
			return ProtClose();
		}
		mg::box::Error::Ptr err;
		if (!myTask.ProcessArgs(aArgs, err)) {
			MG_BOX_ASSERT(false);
			DEBUG_LOG("SSLSocket::OnEvent.03", "%p proc args error", (void*)this);
			return ProtCloseError(mg::box::ErrorRaise(err, "io"));
		}

		ProtOnEvent();
		PrivRecv();
		// Send must be in the end. Recv events could generate new data for sending.
		PrivSend();
	}

	bool
	SSLSocket::PrivHandshake()
	{
		DEBUG_LOG("SSLSocket::PrivHandshake.01", "%p", (void*)this);
		// Enable right away if no encryption. Although it could be better to add a new
		// listener method like OnConnectRaw() in the future.
		if (!mySSL->IsEncrypted())
		{
			DEBUG_LOG("SSLSocket::PrivHandshake.02", "%p not encrypted", (void*)this);
			return true;
		}
		if (!mySSL->Update())
		{
			MG_BOX_ASSERT(false);
			DEBUG_LOG("SSLSocket::PrivHandshake.03", "%p update error", (void*)this);
			ProtCloseError(mg::net::ErrorRaiseSSL(mySSL->GetError(), "handshake"));
			return false;
		}
		if (mySSL->IsConnected())
		{
			DEBUG_LOG("SSLSocket::PrivHandshake.04", "%p SSL connected", (void*)this);
			return true;
		}
		DEBUG_LOG("SSLSocket::PrivHandshake.05", "%p need more data", (void*)this);
		return false;
	}

	void
	SSLSocket::PrivCipherPopulate()
	{
		DEBUG_LOG("SSLSocket::PrivCipherPopulate.01", "%p", (void*)this);
		constexpr uint64_t maxSize = mg::box::theIOVecMaxCount *
			mg::net::theBufferCopySize;
		// Limit amount of data encrypted. Encrypted data is very likely to be bigger (due
		// to TLS overhead), but still it will be limited, which provides a back pressure
		// against too fast appdata flow from the upper-level code. It should not be
		// faster than IO or the send queue would grow infinitely and crash with OOM
		// eventually.
		if (myEncSendQueue.GetReadSize() >= maxSize)
		{
			DEBUG_LOG("SSLSocket::PrivCipherPopulate.02", "%p too big already", (void*)this);
			return;
		}
		uint32_t sendOffset = 0;
		mySendQueue.SkipEmptyPrefix(sendOffset);
		MG_DEV_ASSERT(sendOffset == 0);
		if (mySendQueue.IsEmpty())
		{
			DEBUG_LOG("SSLSocket::PrivCipherPopulate.03", "%p nothing to send", (void*)this);
			return;
		}
		// Do not attempt to send any data until handshake is finished. It would look
		// strange if OnSend() was called before OnConnect().
		if (!ProtWasHandshakeDone())
		{
			DEBUG_LOG("SSLSocket::PrivCipherPopulate.04", "%p handshake isn't finished", (void*)this);
			return;
		}

		uint32_t offset = 0;
		uint64_t batchBufCount = 0;
		uint64_t batchSize = 0;
		do
		{
			mySendQueue.SkipEmptyPrefix(offset);
			MG_DEV_ASSERT(offset == 0);
			const mg::net::BufferLink* link = mySendQueue.GetFirst();
			if (link == nullptr)
				break;
			++batchBufCount;
			const mg::net::Buffer* buf = link->myHead.GetPointer();
			MG_DEV_ASSERT(buf != nullptr);
			MG_DEV_ASSERT(buf->myPos > 0);
			uint32_t size = buf->myPos;
			batchSize += size;
			mySentSize += size;
			mySSL->AppendAppInputCopy(buf->myRData, size);
			mySendQueue.SkipData(offset, buf->myPos);
			MG_DEV_ASSERT(offset == 0);
		} while (
			batchBufCount <= mg::box::theIOVecMaxCount &&
			batchSize <= maxSize);

		if (mySentSize == 0)
		{
			MG_BOX_ASSERT(false);
			DEBUG_LOG("SSLSocket::PrivCipherPopulate.05", "%p all was empty", (void*)this);
			return;
		}
		DEBUG_LOG("SSLSocket::PrivCipherPopulate.06", "%p populated", (void*)this);
		// Important - report the number of bytes as they were provided by the application
		// level. The upper-level code should not send, say, 10 bytes, and get 200 sent
		// bytes in this callback when SSL adds handshakes and all the other stuff.
		ProtOnSend(mySentSize);
		mySentSize = 0;
	}

	void
	SSLSocket::PrivCipherFetch()
	{
		mg::net::Buffer::Ptr head;
		mySSL->PopNetOutput(head);
		myEncSendQueue.WriteRef(std::move(head));
	}

	void
	SSLSocket::PrivSend()
	{
		DEBUG_LOG("SSLSocket::PrivSend.01", "%p", (void*)this);
		if (!PrivSendEventConsume())
		{
			DEBUG_LOG("SSLSocket::PrivSend.02", "%p not ready", (void*)this);
			return;
		}
		PrivCipherPopulate();
		// Even if didn't encrypt any new userdata, still SSL itself could generate some
		// service messages for sending. Always must try to get more of them.
		PrivCipherFetch();
		if (myEncSendQueue.GetReadSize() == 0)
		{
			DEBUG_LOG("SSLSocket::PrivSend.03", "%p nothing to send", (void*)this);
			return;
		}
		myTask.Send(myEncSendQueue.GetReadPos(), 0, mySendEvent);
		if (!PrivSendEventConsume())
		{
			DEBUG_LOG("SSLSocket::PrivSend.04", "%p not ready", (void*)this);
			return;
		}
		if (!mySendQueue.IsEmpty() || !myEncSendQueue.IsEmpty())
		{
			DEBUG_LOG("SSLSocket::PrivSend.05", "%p need more", (void*)this);
			myTask.Reschedule();
		}
		else
		{
			MG_BOX_ASSERT(false);
		}
	}

	void
	SSLSocket::PrivSendAbort(
		mg::box::Error* aError)
	{
		DEBUG_LOG("SSLSocket::PrivSendAbort.01", "%p", (void*)this);
		ProtOnSendError(aError);
	}

	void
	SSLSocket::PrivSendCommit(
		uint32_t aByteCount)
	{
		DEBUG_LOG("SSLSocket::PrivSendCommit.01", "%p sent %u", (void*)this, aByteCount);
		myEncSendQueue.SkipData(aByteCount);
	}

	bool
	SSLSocket::PrivSendEventConsume()
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
		return !mySendEvent.IsLocked();
	}

	void
	SSLSocket::PrivRecv()
	{
		DEBUG_LOG("SSLSocket::PrivRecv.01", "%p", (void*)this);
		// Event could contain a result delivered from a previous execution loop
		// iteration. Happens on Windows as all its IO functions return data on next
		// execution.
		if (!PrivRecvEventConsume())
		{
			DEBUG_LOG("SSLSocket::PrivRecv.02", "%p not ready", (void*)this);
			return;
		}
		if (myRecvSize == 0)
		{
			if (ProtWasHandshakeDone())
			{
				DEBUG_LOG("SSLSocket::PrivRecv.03", "%p no receive buffer", (void*)this);
				MG_BOX_ASSERT(myEncRecvQueue.IsEmpty());
				return;
			}
			DEBUG_LOG("SSLSocket::PrivRecv.04", "%p add internal receive buffer", (void*)this);
			myEncRecvQueue.EnsureWriteSize(mg::net::theBufferCopySize);
		}
		else
		{
			myEncRecvQueue.EnsureWriteSize(myRecvSize);
		}
		myTask.Recv(myEncRecvQueue.GetWritePos(), myRecvEvent);
		if (!PrivRecvEventConsume())
		{
			DEBUG_LOG("SSLSocket::PrivRecv.05", "%p not ready", (void*)this);
			return;
		}
		// Finished right away. Either an error, or success which happens on Linux only.
		// Don't do in a loop. If has more data to recv, do it next time so as not to
		// starve the other tasks.
		if (myRecvSize > 0 || !ProtWasHandshakeDone())
		{
			DEBUG_LOG("SSLSocket::PrivRecv.06", "%p need more", (void*)this);
			myTask.Reschedule();
		}
	}

	void
	SSLSocket::PrivRecvCommit(
		uint32_t aByteCount)
	{
		DEBUG_LOG("SSLSocket::PrivRecvCommit.01", "%p received %u", (void*)this, aByteCount);
		if (aByteCount == 0)
		{
			// Graceful close.
			ProtClose();
			return;
		}
		myEncRecvQueue.PropagateWritePos(aByteCount);
		uint64_t recvSizeWas = myRecvSize;
		myRecvSize = 0;
		mySSL->AppendNetInputRef(myEncRecvQueue.PopData());
		mg::net::Buffer::Ptr head;
		mySSL->PopAppOutput(head);
		myRecvQueue.WriteRef(std::move(head));
		if (mySSL->HasError())
		{
			DEBUG_LOG("SSLSocket::PrivRecvCommit.02", "%p SSL error", (void*)this);
			ProtOnRecvError(mg::net::ErrorRaiseSSL(mySSL->GetError(), "decrypt"));
			return;
		}
		// If this was a part of the handshake, then need to handle it on a next wakeup.
		if (!ProtWasHandshakeDone())
		{
			DEBUG_LOG("SSLSocket::PrivRecvCommit.03", "%p need more for handshake", (void*)this);
			myTask.Reschedule();
		}
		if (!myRecvQueue.IsEmpty())
		{
			DEBUG_LOG("SSLSocket::PrivRecvCommit.04", "%p deliver to user", (void*)this);
			mg::net::BufferReadStream stream(myRecvQueue);
			ProtOnRecv(stream);
		}
		else
		{
			DEBUG_LOG("SSLSocket::PrivRecvCommit.05", "%p keep reading, got no user data", (void*)this);
			// Keep reading until get data. User has requested it, so need to be reading
			// until the data comes. Reading not returning any data can happen because
			// some data is SSL-internal stuff, user shouldn't care about that.
			myRecvSize = recvSizeWas;
		}
	}

	void
	SSLSocket::PrivRecvAbort(
		mg::box::Error* aError)
	{
		DEBUG_LOG("SSLSocket::PrivRecvAbort.01", "%p", (void*)this);
		ProtOnRecvError(aError);
	}

	bool
	SSLSocket::PrivRecvEventConsume()
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
		return !myRecvEvent.IsLocked();
	}

}
}
