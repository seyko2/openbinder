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

#ifndef _SUPPORT_SIGNAL_THREAD_INIT_H
#define _SUPPORT_SIGNAL_THREAD_INIT_H

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

class SignalHandlerStaticInit
{
public:
	SignalHandlerStaticInit();
	~SignalHandlerStaticInit();
	
};

extern SignalHandlerStaticInit g_signalHandlerStaticInit;

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif /* _SUPPORT_SIGNAL_HANDLER_H */

