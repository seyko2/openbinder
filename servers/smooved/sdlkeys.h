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

#ifndef SDLKEYS_H
#define SDLKEYS_H

#include <KeyCodes.h>

#if LIBSDL_PRESENT

#include <SDL/SDL.h>

const uint16_t kSDL2KeyCode[] = {
	SDLK_BACKSPACE,			keyBackspace,
	SDLK_TAB,				keyTab,
	SDLK_RETURN,			keyReturn,
	SDLK_PAUSE,				keyPause,
	SDLK_ESCAPE,			keyEscape,
	SDLK_DELETE,			keyBack,

	/* Arrows + Home/End pad */
	SDLK_UP,				keyRockerUp,
	SDLK_DOWN,				keyRockerDown,
	SDLK_RIGHT,				keyRockerRight,
	SDLK_LEFT,				keyRockerLeft,
	SDLK_INSERT,			keyInsert,
	SDLK_HOME,				keyLauncher,
	SDLK_END,				keyRockerCenter,
	SDLK_PAGEUP,			keyPageUp,
	SDLK_PAGEDOWN,			keyPageDown,

	/* Function keys */
	SDLK_F1,				keyHard1,
	SDLK_F2,				keyHard2,
	SDLK_F3,				keyHard3,
	SDLK_F4,				keyHard4,
	SDLK_F5,				keyHardPower,
	SDLK_F6,				keyHardCradle,
	SDLK_F7,				keyHardCradle2,
	SDLK_F8,				keyHardContrast,
	SDLK_F9,				keyPhoneTalk,
	SDLK_F10,				keyPhoneEnd,
	SDLK_F11,				keyBlue,
	SDLK_F12,				keyGreen,

	/* Key state modifier keys */
	SDLK_NUMLOCK,			keyNumLock,
	SDLK_CAPSLOCK,			keyCapsLock,
	SDLK_SCROLLOCK,			keyScrollLock,
	SDLK_RSHIFT,			keyRightShift,
	SDLK_LSHIFT,			keyLeftShift,
	SDLK_RCTRL,				keyRightControl,
	SDLK_LCTRL,				keyLeftControl,
	SDLK_RALT,				keyRightAlt,
	SDLK_LALT,				keyLeftAlt,
	SDLK_RMETA,				keyLeftCommand,
	SDLK_LMETA,				keyRightCommand,
	SDLK_LSUPER,			keyLeftCommand,		/* Left "Windows" key */
	SDLK_RSUPER,			keyRightCommand,	/* Right "Windows" key */
	SDLK_MODE,				keyRightAlt,		/* "Alt Gr" key */

	/* Miscellaneous function keys */
	SDLK_PRINT,				keyPrintScreen,
	SDLK_MENU,				keyMenu,
	SDLK_POWER,				keyHardPower,		/* Power Macintosh power key */
};

#endif // LIBSDL_PRESENT


#if 0		// This is the old scancode to keycode table

/*			0				1				2				3				4				5				6				7
			8				9				a				b				c				d				e				f */

			
/* 0 */		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,
			keyNull,		keyEscape,		keyOne,			keyTwo,			keyThree,		keyFour,		keyFive,		keySix,

/* 1 */		keySeven,		keyEight,		keyNine,		keyZero,		keyDash,		keyEquals,		keyBackspace,	keyTab,
			keyQ,			keyW,			keyE,			keyR,			keyT,			keyY,			keyU,			keyI,

/* 2 */		keyO,			keyP,			keyOpenBracket,	keyCloseBracket,keyReturn,		keyLeftControl,	keyA,			keyS,
			keyD,			keyF,			keyG,			keyH,			keyJ,			keyK,			keyL,			keySemiColon,

/* 3 */		keySingleQuote,	keyBacktick,	keyLeftShift,	keyBackslash,	keyZ,			keyX,			keyC,			keyV,
			keyB,			keyN,			keyM,			keyComma,		keyPeriod,		keySlash,		keyRightShift,	keyNumericAsterisk,

/* 4 */		keyLeftAlt,		keySpace,		keyCapsLock,	keyHard1,		keyHard2,		keyHard3,		keyHard4,		keyHardPower,
			keyHardCradle,	keyHardCradle2,	keyHardContrast,keyPhoneTalk,	keyPhoneEnd,	keyNumLock,	keyScrollLock,	keyNumericSeven,

/* 5 */		keyNumericEight,keyNumericNine,	keyNumericDash,	keyNumericFour,	keyNumericFive,	keyNumericSix,	keyNumericPlus,	keyNumericOne,
			keyNumericTwo,	keyNumericThree,keyNumericZero,	keyNumericPeriod,keyNull,		keyNull,		keyLessThan,	keyBlue,

/* 6 */		keyGreen,		keyLauncher,	keyRockerUp,	keyPageUp,		keyRockerLeft,	keyNull,		keyRockerRight,	keyRockerCenter,
			keyRockerDown,	keyPageDown,	keyInsert,		keyBack,		keyNumericEnter,keyRightControl,keyPause,		keyPrintScreen,

/* 7 */		keyNumericNumberSign,keyRightAlt,keyNull,		keyLeftCommand,	keyRightCommand,keyMenu,		keyNull,		keyNull,
			keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,

/* 8 */		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,
			keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,

/* 9 */		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,
			keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,

/* a */		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,
			keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,

/* b */		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,
			keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,

/* c */		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,
			keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,

/* d */		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,
			keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,

/* e */		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,
			keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,

/* f */		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,
			keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,		keyNull,

#endif // old scancode to keycode table

#endif // SDLKEYS

