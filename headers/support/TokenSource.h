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
 
#ifndef _TOKEN_SOURCE_H
#define _TOKEN_SOURCE_H

/*!	@file support/TokenSource.h
	@ingroup CoreSupportBinder
	@brief Binder-based access control.
*/

#include <support/Autolock.h>
#include <support/Handler.h>
#include <support/IIterable.h>
#include <support/Node.h>
#include <support/Package.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportBinder
	@{
*/

//!	A Binder class used to implement mutual access control.
/*!	When you create one of these, and access the "token" ICatalog
	property, or the GetToken method, it returns you an SValue
	containing an IBinder.  If someone else already has a
	reference to this IBinder, it gives you the same one.  If
	nobody else has it, it gives you a new one.  When the IBinder
	is created, a binder method you specify is called on an object
	you specify.  When the last reference on the IBinder is 
	released, another binder method is called.
	
	To specify which
	objects and binder calls are made, pass the following into
	the args parameter of the constructor:
@code
args=@{
	acquired->{ object->$target, call->$bindercall },
	released->{ object->$target, call->$bindercall }
}
@endcode
	target can be any IBinder (binder object)
	bindercall can either be just a function name, or a function
	name mapped to arguments:
@code
bindercall=@{ FunctionName }
bindercall=@{ FunctionName->0->arg0 }
bindercall=@{ FunctionName->{ 0->arg0, 1->arg1 }
@endcode
*/
class BTokenSource : public BnNode, public BnIterable, private BHandler
{
public:
						BTokenSource(const SContext& context, const SValue& args);
	virtual				~BTokenSource();

	virtual void 		InitAtom();
	virtual	SValue		Inspect(const sptr<IBinder>& caller, const SValue &which, uint32_t flags = 0);
	
	/*! @name INode Interface */
	//@{
	virtual	sptr<INode> Attributes() const;
	virtual	SString MimeType() const;
	virtual	void SetMimeType(const SString& value);
	virtual	nsecs_t CreationDate() const;
	virtual	void SetCreationDate(nsecs_t value);
	virtual	nsecs_t ModifiedDate() const;
	virtual	void SetModifiedDate(nsecs_t value);	
	virtual	status_t Walk(SString* path, uint32_t flags, SValue* node);
	//@}

	/*! @name IIterable Interface */
	//@{
	virtual	sptr<IIterator> NewIterator(const SValue& args, status_t* error);
	//@}

	// BHandler
	virtual status_t 	HandleMessage(const SMessage& msg);

						// returns the token.  The same as accessing it through
						// the "token" catalog entry.
			SValue		GetToken();

protected:
	// Disambiguate.
	const SContext&	Context() const;
	
private:
	class Token : public BBinder
	{
	public:
		Token(const sptr<BTokenSource> &source);
		virtual ~Token();
	private:
		sptr<BTokenSource> m_source;
	};

	friend class Token;

	void token_done();

	SLocker m_lock;
	SValue m_acquiredAction;
	SValue m_releasedAction;
	wptr<Token> m_token;
};

/*!	@} */
#if _SUPPORTS_NAMESPACE
} }
#endif

#endif // _TOKEN_SOURCE_H
