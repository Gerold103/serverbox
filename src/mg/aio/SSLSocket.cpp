#include "SSLSocket.h"

#include "mg/aio/TCPSocketCtl.h"
#include "mg/box/Error.h"
#include "mg/box/IOVec.h"
#include "mg/net/SSLStream.h"

namespace mg {
namespace aio {

	SSLSocketParams::SSLSocketParams()
		: mySSL(nullptr)
		, myHostName(nullptr)
		, myDoEncrypt(true)
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
		ProtOpen();
		myEncSendQueue.Clear();
		mySentSize = 0;
		delete mySSL;
		mySSL = new mg::net::SSLStream(aParams.mySSL);
		if (aParams.myDoEncrypt)
			mySSL->Connect();
		if (aParams.myHostName != nullptr)
			mySSL->SetHostName(aParams.myHostName);
		// Report the socket as connected only after the SSL handshake is finished. This
		// should help not to do potentially expensive work in OnConnect() if the client
		// would fail anyway.
		ProtPostHandshake(new TCPSocketHandshake(
			[this](TCPSocketHandshake*) { return PrivHandshake(); }));
	}

	std::string
	SSLSocket::GetHostName() const
	{
		MG_BOX_ASSERT(myTask.IsInWorkerNow());
		return mySSL->GetHostName();
	}

	void
	SSLSocket::Encrypt()
	{
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
		delete mySSL;
	}

	void
	SSLSocket::OnEvent(
		const IOArgs& aArgs)
	{
		if (myTask.IsClosed())
			return ProtClose();
		mg::box::Error::Ptr err;
		if (!myTask.ProcessArgs(aArgs, err))
			return ProtCloseError(mg::box::ErrorRaise(err, "io"));

		ProtOnEvent();
		PrivRecv();
		// Send must be in the end. Recv events could generate new data for sending.
		PrivSend();
	}

	bool
	SSLSocket::PrivHandshake()
	{
		// Enable right away if no encryption. Although it could be better to add a new
		// listener method like OnConnectRaw() in the future.
		if (!mySSL->IsEncrypted())
			return true;
		if (!mySSL->Update())
		{
			ProtCloseError(mg::net::ErrorRaiseSSL(mySSL->GetError(), "handshake"));
			return false;
		}
		return mySSL->IsConnected();
	}

	void
	SSLSocket::PrivCipherPopulate()
	{
		constexpr uint64_t maxSize = mg::box::theIOVecMaxCount *
			mg::net::theBufferCopySize;
		// Limit amount of data encrypted. Encrypted data is very likely to be bigger (due
		// to TLS overhead), but still it will be limited, which provides a back pressure
		// against too fast appdata flow from the upper-level code. It should not be
		// faster than IO or the send queue would grow infinitely and crash with OOM
		// eventually.
		if (myEncSendQueue.GetReadSize() >= maxSize)
			return;
		uint32_t offset = 0;
		mySendQueue.SkipEmptyPrefix(offset);
		MG_DEV_ASSERT(offset == 0);
		if (mySendQueue.IsEmpty())
			return;
		// Do not attempt to send any data until handshake is finished. It would look
		// strange if OnSend() was called before OnConnect().
		if (!ProtWasHandshakeDone())
			return;

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
			mySSL->AppendAppInputCopy(buf->myRData, size);
			mySendQueue.SkipData(offset, buf->myPos);
			MG_DEV_ASSERT(offset == 0);
		} while (batchBufCount <= mg::box::theIOVecMaxCount && batchSize <= maxSize);

		mySentSize += batchSize;
		// It can't be empty. The empty prefix was skipped in the beginning of the
		// function as a fast-path.
		MG_DEV_ASSERT(mySentSize > 0);
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
		if (!PrivSendEventConsume())
			return;
		PrivCipherPopulate();
		// Important - report the number of bytes as they were provided by the application
		// level. The upper-level code should not send, say, 10 bytes, and get 200 sent
		// bytes in this callback when SSL adds handshakes and all the other stuff.
		if (mySentSize > 0)
		{
			uint32_t toReport = mySentSize > UINT32_MAX ?
				UINT32_MAX : (uint32_t)mySentSize;
			ProtOnSend(toReport);
			mySentSize -= toReport;
			// In case a user managed to send 4GB at once, lets deliver it in parts.
			if (mySentSize > 0)
				myTask.Reschedule();
		}
		// Even if didn't encrypt any new userdata, still SSL itself could generate some
		// service messages for sending. Always must try to get more of them.
		PrivCipherFetch();
		if (myEncSendQueue.GetReadSize() == 0)
			return;
		myTask.Send(myEncSendQueue.GetReadPos(), 0, mySendEvent);
		if (!PrivSendEventConsume())
			return;
		if (!mySendQueue.IsEmpty() || myEncSendQueue.GetReadSize() != 0)
			myTask.Reschedule();
	}

	void
	SSLSocket::PrivSendAbort(
		mg::box::Error* aError)
	{
		ProtOnSendError(aError);
	}

	void
	SSLSocket::PrivSendCommit(
		uint32_t aByteCount)
	{
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
		if (!PrivRecvEventConsume())
			return;
		if (myRecvSize == 0)
		{
			if (ProtWasHandshakeDone())
			{
				MG_BOX_ASSERT(myEncRecvQueue.IsEmpty());
				return;
			}
			myEncRecvQueue.EnsureWriteSize(mg::net::theBufferCopySize);
		}
		else
		{
			myEncRecvQueue.EnsureWriteSize(myRecvSize);
		}
		myTask.Recv(myEncRecvQueue.GetWritePos(), myRecvEvent);
		if (!PrivRecvEventConsume())
			return;
		if (myRecvSize > 0 || !ProtWasHandshakeDone())
			myTask.Reschedule();
	}

	void
	SSLSocket::PrivRecvCommit(
		uint32_t aByteCount)
	{
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
			ProtOnRecvError(mg::net::ErrorRaiseSSL(mySSL->GetError(), "decrypt"));
			return;
		}
		// If this was a part of the handshake, then need to handle it on a next wakeup.
		if (!ProtWasHandshakeDone())
			myTask.Reschedule();
		if (!myRecvQueue.IsEmpty())
		{
			mg::net::BufferReadStream stream(myRecvQueue);
			ProtOnRecv(stream);
		}
		else
		{
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
