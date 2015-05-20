	AREA	|.text|, CODE, READONLY


	EXPORT ReadPerformanceCounter
ReadPerformanceCounter FUNCTION
	mov r1, pc
	; jump to the prefetch abort handler will generate to generate a prefetch
	; abort (this only works from user mode)
	mov pc, #0x0c
	bx lr

	ENDFUNC
	
	END
