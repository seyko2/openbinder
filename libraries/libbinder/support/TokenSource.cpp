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

#include <support/Iterator.h>
#include <support/TokenSource.h>
#include <support/StdIO.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::storage;
using namespace palmos::support;
using namespace palmos::xml;
#endif

enum {
	bmsgCall = 'call',
};

B_STATIC_STRING_VALUE_SMALL(key_call_action, "act", );
B_STATIC_STRING_VALUE_LARGE(key_acquired, "acquired", );
B_STATIC_STRING_VALUE_LARGE(key_released, "released", );
B_STATIC_STRING_VALUE_LARGE(key_token, "token", );
B_STATIC_STRING_VALUE_LARGE(key_object, "object", );
B_STATIC_STRING_VALUE_LARGE(key_call, "call", );

static void do_call(const SValue &v);

BTokenSource::BTokenSource(const SContext& context, const SValue& args)
	:	BnNode(context),
		BnIterable(context),
	 	BHandler(context),
	 	m_acquiredAction(args[key_acquired]),
	 	m_releasedAction(args[key_released]),
	 	m_token()
{
}

BTokenSource::~BTokenSource()
{
}

void BTokenSource::InitAtom()
{
	BnNode::InitAtom();
	BnIterable::InitAtom();
	BHandler::InitAtom();
}

const SContext & BTokenSource::Context() const
{
	return static_cast<BnNode const*>(this)->Context();
}

SValue BTokenSource::Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags)
{
	// not the handler
	return BnNode::Inspect(caller, which, flags)
			.Join(BnIterable::Inspect(caller, which, flags));
}

sptr<IIterator> BTokenSource::NewIterator(const SValue& args, status_t* error)
{
	sptr<IIterator> it(new BValueIterator(SValue(key_token, GetToken())));
	if (error) *error = it == NULL ? B_NO_MEMORY : B_OK;
	return it;
}

status_t BTokenSource::Walk(SString* path, uint32_t flags, SValue* node)
{
	status_t err = B_ENTRY_NOT_FOUND;
	if (*path == "token")
	{
		*node = GetToken();
		err = B_OK;
	}
	// make the path empty to show that we walked it.
	path->Remove(0, path->Length());
	return err;
}

sptr<INode> BTokenSource::Attributes() const
{
	return NULL;
}

SString BTokenSource::MimeType() const
{
	return B_EMPTY_STRING;
}

void BTokenSource::SetMimeType(const SString& value)
{
}

nsecs_t BTokenSource::CreationDate() const
{
	return 0;
}

void BTokenSource::SetCreationDate(nsecs_t value)
{
}

nsecs_t BTokenSource::ModifiedDate() const
{
	return 0;
}

void BTokenSource::SetModifiedDate(nsecs_t value)
{
}

status_t BTokenSource::HandleMessage(const SMessage& msg)
{
	switch (msg.What())
	{
		case bmsgCall:
			do_call(msg[key_call_action]);
			break;
	}
	return B_OK;
}

SValue BTokenSource::GetToken()
{
	sptr<Token> token;
	m_lock.Lock();
	token = m_token.promote();
	if (token == NULL) {
		m_token = token = new Token(this);
		// send the notice that someone's acquired it
		SMessage msg(bmsgCall);
		msg.JoinItem(key_call_action, m_acquiredAction);
		PostMessage(msg);
	}
	m_lock.Unlock();
	return SValue::Binder(token.ptr());
}

void BTokenSource::token_done()
{
	SAutolock(m_lock.Lock());
	m_token = NULL;
	// send the notice that someone's released it
	SMessage msg(bmsgCall);
	msg.JoinItem(key_call_action, m_releasedAction);
	PostMessage(msg);
}

BTokenSource::Token::Token(const sptr<BTokenSource> &source)
	:BBinder(),
	 m_source(source)
{
}

BTokenSource::Token::~Token()
{
	m_source->token_done();
}

static void do_call(const SValue &v)
{
	sptr<IBinder> b = v[key_object].AsBinder();
	if (b == NULL) return ;
	SValue call(v[key_call]);
	SValue key, value;
	void* cookie = NULL;
	while (call.GetNextItem(&cookie, &key, &value) == B_OK) {
		if (key.IsWild()) {
			b->Invoke(value);
		} else {
			b->Invoke(key, value);
		}
	}
}
