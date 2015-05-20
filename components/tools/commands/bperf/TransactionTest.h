/*
 * Copyright (c) 2005 Palmsource, Inc.
 * 
 * This software is licensed as described in the file LICENSE, which
 * you should have received as part of this distribution. The terms
 * are also available at http://www.openbinder.org/license.html.
 * 
 * This software consists of voluntary contributions made by many
 * individuals. For the exact contribution history, see the revision
 * history and logs, available at http://www.openbinder.org
 */

#ifndef _TRANSACTION_TEST_H
#define _TRANSACTION_TEST_H

#include <support/Binder.h>
#include <support/Parcel.h>
#include <support/Package.h>
#include <support/Locker.h>
#include <support/Autolock.h>
#include <support/StaticValue.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

enum {
	kTestTransaction = 'TEST',
	kDropTransaction = 'DROP',
	kDropWeakTransaction = 'DRWK',
	kDropSameTransaction = 'DRSM',
	kDropSameWeakTransaction = 'DRSW',
	kKeepTransaction = 'KEEP',
	kKeepWeakTransaction = 'KPWK',
	kIncStrongTransaction = 'ISTR',
	kDecStrongTransaction = 'DSTR',
	kHoldTransaction = 'HOLD',
	kPingPongTransaction = 'PONG'
};

B_STATIC_STRING_VALUE_LARGE(key_validate, "validate", );

static bool validate_transaction_data(const SParcel& parcel)
{
	const uint32_t* data = (const uint32_t*)parcel.Data();
	const size_t N = parcel.Length();
	for (size_t i=0; i<N; i+=4) {
		uint32_t v = *data++;
		if ((N-i) < 4) {
			static const uint32_t masks[3] = {
				0x000000ff, 0x0000ffff, 0x00ffffff
			};
			v = (v&masks[N-i-1]) | ((i/4)&~masks[N-i-1]);
		}
		if (v != i/4) {
			bout << "Bad data at offset " << (void*)i << "!  Data is: "
				<< SHexDump(parcel.Data(), parcel.Length()) << endl;
			return false;
		}
	}
	
	return true;
}

class TransactionTest : public BBinder, public SPackageSptr
{
public:
	TransactionTest(const SContext& context, bool validate=false, bool pingpong = false)
		:	BBinder(context), m_lock("TransactionTest"),
			m_pongServer(pingpong), m_validate(validate)
	{
		SHOW_OBJS(bout << "TransactionTest " << this << " constructed..." << endl);
		if (m_pongServer) m_pongDatum = new TransactionTest(context, false);
	}

	TransactionTest(const SContext& context, const SValue& args)
		:	BBinder(context), m_lock("TransactionTest"),
			m_pongServer(false),
			m_validate(args[key_validate].AsBool())
	{
		SHOW_OBJS(bout << "TransactionTest " << this << " constructed..." << endl);
	}

	virtual status_t Transact(uint32_t code, SParcel& data, SParcel* reply = NULL, uint32_t flags = 0)
	{
		if (code == kTestTransaction) {
			if (m_validate) {
				//bout << "Received transaction of " << data.Length() << " bytes and replying." << endl;
				validate_transaction_data(data);
			}
			if (reply) reply->Copy(data);
			return B_OK;
		} else if (code == kDropTransaction) {
			if (m_validate) {
				sptr<IBinder> binder = data.ReadBinder();
				if (binder == NULL) {
					bout << "Transaction did not receive a binder!" << endl;
					return B_ERROR;
				}
			}
			// Do nothing.
			return B_OK;
		} else if (code == kDropWeakTransaction) {
			if (m_validate) {
				wptr<IBinder> binder = data.ReadWeakBinder();
				if (binder == NULL) {
					bout << "Transaction did not receive a binder!" << endl;
					return B_ERROR;
				}
			}
			// Do nothing.
			return B_OK;
		} else if (code == kDropSameTransaction) {
			if (m_validate) {
				sptr<IBinder> binder = data.ReadBinder();
				if (binder == NULL) {
					bout << "Transaction did not receive a binder!" << endl;
					return B_ERROR;
				} else if (binder != m_keepWeakObject) {
					bout << "Transaction did not receive same binder!" << endl;
					return B_ERROR;
				}
			}
			// Do nothing.
			return B_OK;
		} else if (code == kDropSameWeakTransaction) {
			if (m_validate) {
				wptr<IBinder> binder = data.ReadWeakBinder();
				if (binder == NULL) {
					bout << "Transaction did not receive a binder!" << endl;
					return B_ERROR;
				} else if (binder != m_keepWeakObject) {
					bout << "Transaction did not receive same binder!" << endl;
					return B_ERROR;
				}
			}
			// Do nothing.
			return B_OK;
		} else if (code == kKeepTransaction) {
			SAutolock _l(m_lock.Lock());
			if (m_validate) {
				sptr<IBinder> binder = data.ReadBinder();
				if (m_keepObject == NULL) {
					m_keepObject = binder;
					m_keepWeakObject = binder;
				} else if (m_keepObject != binder) {
					bout << "Transaction did not receive expected binder!" << endl;
					return B_ERROR;
				}
			}
			m_keep.Copy(data);
			return B_OK;
		} else if (code == kKeepWeakTransaction) {
			SAutolock _l(m_lock.Lock());
			if (m_validate) {
				wptr<IBinder> binder = data.ReadWeakBinder();
				if (m_keepWeakObject == NULL) m_keepWeakObject = binder;
				else if (m_keepWeakObject != binder) {
					bout << "Transaction did not receive expected binder!" << endl;
					return B_ERROR;
				}
			}
			m_keep.Copy(data);
			return B_OK;
		} else if (code == kIncStrongTransaction) {
			IncStrong(this);
			return B_OK;
		} else if (code == kDecStrongTransaction) {
			DecStrong(this);
			return B_OK;
		} else if (code == kHoldTransaction) {
			SAutolock _l(m_lock.Lock());
			m_hold = data.ReadBinder();
			return B_OK;
		} else if (code == kPingPongTransaction) {
			if (m_pongServer) {
				// "server" side replies with the pong datum
				if (reply) reply->WriteBinder(m_pongDatum);
			} else {
				// client side asks the server side for the datum
				sptr<IBinder> server = data.ReadBinder();
				if (server.ptr()) {
					// send an empty parcel to the server
					SParcel send;
					// get an SParcel suitable for replies
					// SParcel *my_reply = SParcel::GetParcel();
					SParcel my_reply;
					status_t result = server->Transact(kPingPongTransaction, send, &my_reply, flags);
					if (result == B_OK) {
						// read binder out of the reply and forward it on
						result = reply->WriteBinder(my_reply.ReadBinder());
					}
					// SParcel::PutParcel(my_reply);
					return result;
				} else if (m_validate) {
					bout << "Ping-Pong Transaction: Pong client did not receive server object!" << endl;
				}
			}
			return B_OK;
		}
		return BBinder::Transact(code, data, reply, flags);
	}

	const sptr<IBinder> PongDatum() const { return m_pongDatum; }
	
protected:
	~TransactionTest()
	{
		SHOW_OBJS(bout << "TransactionTest " << this << " destroyed..." << endl);
	}

private:
	SLocker			m_lock;
	SParcel			m_keep;
	sptr<IBinder>	m_keepObject;
	wptr<IBinder>	m_keepWeakObject;
	sptr<IBinder>	m_hold;
	bool			m_pongServer;
	bool			m_validate;
	sptr<IBinder>	m_pongDatum;
};

#endif /* _TRANSACTION_TEST_H */
