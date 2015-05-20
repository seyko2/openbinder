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

#ifndef TYPEBANK_H
#define TYPEBANK_H

#include "idlc.h"
#include "InterfaceRec.h"
//extern SKeyedVector<SString, sptr<IDLType> > TypeBank;

void cleanTypeBank(SKeyedVector<SString, sptr<IDLType> >& tb);
status_t  initTypeBank(SKeyedVector<SString, sptr<IDLType> >& tb);
void checktb (SKeyedVector<SString, sptr<IDLType> > tb);

const SKeyedVector<SString, sptr<IDLType> >& getTypeBank();

#endif // TYPEBANK_H
