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

#ifndef _SUPPORT_MESSAGECODES_H_
#define _SUPPORT_MESSAGECODES_H_

/*!	@file support/MessageCodes.h
	@ingroup CoreSupportUtilities
	@brief Common message code constants.
*/

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

/*!	@addtogroup CoreSupportUtilities
	@{
*/

/**************************************************************************************/

enum {
	B_KEY_DOWN 					= 'keyd',
	B_KEY_UP 					= 'keyu',
	B_KEY_HELD					= 'keyh',
	B_STRING_INPUT				= 'stri',
	B_UNMAPPED_KEY_DOWN 		= 'ukyd',
	B_UNMAPPED_KEY_UP 			= 'ukyu',
	B_FIND_HAPPENING_SOMEWHERE	= 'find',	/* PRIVATE -- temporary until find can be run from not main UI thread */
	B_INVALIDATE				= 'invl',
	B_MODIFIERS_CHANGED			= 'mdch',
	B_PEN_DOWN					= 'pndn',
	B_PEN_MOVED 				= 'pnmv',
	B_PEN_UP 					= 'pnup',
	B_PEN_MOVED_WHILE_UP		= 'pnmu',
	B_PEN_DRAGGED				= 'pndg',
	B_PEN_EXITED				= 'pnex',
	B_PEN_DROPPED				= 'pndp',
	B_MOUSE_WHEEL_CHANGED		= 'whel',
	B_NULL_EVENT				= 'null',
	B_DEBUG_EVENT				= 'debg',
	B_SPY_EVENT					= 'spye',
	B_SET_TIMEOUTS_EVENT		= 'stim',
	B_PLAYBACK_EVENT			= 'play',
	B_HIGHLEVEL_EVENT			= 'hlvl',
	B_DISCOVER					= 'disc',
	B_STOP_DISCOVERY			= 'ndsc',
	B_VIEW_PROPERTIES			= 'prop',
	B_STOP_LISTENING			= 'nlst',

	// Private for input_server.
	bmsgEnqueuePointerEvent		='_ENP',
	bmsgEnqueueKeyEvent			='_ENK'
};

/**************************************************************************************/

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::support
#endif

#endif	// _SUPPORT_MESSAGECODES_H_
