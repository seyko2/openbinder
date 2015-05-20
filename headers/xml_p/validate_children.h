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

#ifndef VALIDATE_CHILDREN_H
#define VALIDATE_CHILDREN_H

#include <xml/BContent.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
#endif

// Give this an elementDecl and it will give you a pointer to the transition
// tables that it uses to do the validating.  This data structure is opaque.
// Don't forget to free it with _free_validate_table_
void		_init_validate_table_(const BElementDecl * decl, const void ** tablePtr);

// Give this a pointer to a uint16_t.  It initializes it.  Do this before you start.
void		_init_validate_state_(uint16_t * state);

// Give it the state, the table and an element name.  Will return B_OK if it's
// allowed, and B_XML_CHILD_ELEMENT_NOT_ALLOWED if it's not.
status_t	_check_next_element_(const char * element, uint16_t * state, const void * table);

// Give it the state and the table, and it will return B_OK if it's okay to
// be done with children here, and B_XML_CHILD_PATTERN_NOT_FINISHED if it's not.
status_t	_check_end_of_element_(uint16_t state, const void * table);

// Free the private table data
void		_free_validate_table_(const void * tableData);

#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif

#endif // VALIDATE_CHILDREN_H
