; The Catacomb Source Code
; Copyright (C) 1993-2014 Flat Rock Software
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License along
; with this program; if not, write to the Free Software Foundation, Inc.,
; 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

	IDEAL
	MODEL SMALL,C

;
;offsets into .SPK file at segment SPKRfile, offset 0
;
;sound records each take up 16 bytes, and start at $10, and continue to $3F0

snd_start	equ	0
snd_priority	equ	2
snd_samples	equ	3
snd_name	equ	4


;
; EGA registers
;
SCindex		equ	3C4h
SCmapmask	equ	2
GCindex		equ	3CEh
GCmode 		equ	5


	DATASEG


;
; stuff for pics and sprites
;
STRUC	picstruct		; sprite info extracted by refresh
width		dw	?
height		dw	?
shapeptr	dw	?
ENDS

	EXTRN pictable: picstruct	; array of picture info
	EXTRN charptr:WORD
	EXTRN tileptr:WORD
	EXTRN picptr:WORD
	EXTRN spriteptr:WORD	;paraligned pointers
	EXTRN grmode:WORD
	EXTRN crtcaddr:WORD

;
; stuff for pics and sprites
;
STRUC	picstruct		; sprite info extracted by refresh
width		dw	?
height		dw	?
shapeptr	dw	?
ENDS

	EXTRN	imageinfo: picstruct	; array of picture info

SPKactive dw    0                   ;set non zero when started

SoundData	dd	?	;two word pointer to SPKR file, PARA aligned
soundmode	dw	1       ;0=nosound, 1=SPKR, 2= adlib...
OldInt8		dd	?	;StartupSPK saves here, Shutdown restores
ExtraInts	db	?	;number of PlaySPKR's to a regular int 8
Intcount	db	?	;counter for extraints, call OldInt8 at 0
sndspeed	dw	?	;timer count speed

SndPtr		dw	?	;Pointer to frequency of current sound
SndPriority	db	?	;current sound's priority

pausesndptr	dw	?
pausepriority	db	?
pauseextraints	db	?
pauseintcount	db	?
pausespeed	dw	?

_dontplay	dw	0	;set to 1 to avoid all interrupt and timer stuff

xormask		dw	0	;to invert characters
_yshift		dw	0	;to shift char lines

screenseg	dw	0a000h	;changes with grmode, page flips, & scrolls

	PUBLIC soundmode,SoundData,xormask,screenseg,_dontplay

	  CODESEG

	PUBLIC CGAylookup,EGAylookup,VGAylookup

;========
;
; YLOOKUP has the offsets from screen mem of each screen line (multiples of 8)
;
;========

CGAylookup dw    0,8192,  80,8272, 160,8352, 240,8432, 320,8512, 400,8592, 480,8672
    dw  560,8752, 640,8832, 720,8912, 800,8992, 880,9072, 960,9152,1040,9232
    dw 1120,9312,1200,9392,1280,9472,1360,9552,1440,9632,1520,9712,1600,9792
    dw 1680,9872,1760,9952,1840,10032,1920,10112,2000,10192,2080,10272,2160,10352
    dw 2240,10432,2320,10512,2400,10592,2480,10672,2560,10752,2640,10832,2720,10912
    dw 2800,10992,2880,11072,2960,11152,3040,11232,3120,11312,3200,11392,3280,11472
    dw 3360,11552,3440,11632,3520,11712,3600,11792,3680,11872,3760,11952,3840,12032
    dw 3920,12112,4000,12192,4080,12272,4160,12352,4240,12432,4320,12512,4400,12592
    dw 4480,12672,4560,12752,4640,12832,4720,12912,4800,12992,4880,13072,4960,13152
    dw 5040,13232,5120,13312,5200,13392,5280,13472,5360,13552,5440,13632,5520,13712
    dw 5600,13792,5680,13872,5760,13952,5840,14032,5920,14112,6000,14192,6080,14272
    dw 6160,14352,6240,14432,6320,14512,6400,14592,6480,14672,6560,14752,6640,14832
    dw 6720,14912,6800,14992,6880,15072,6960,15152,7040,15232,7120,15312,7200,15392
    dw 7280,15472,7360,15552,7440,15632,7520,15712,7600,15792,7680,15872,7760,15952
    dw 7840,16032,7920,16112

EGAylookup dw    0,  40,  80, 120, 160, 200, 240, 280, 320, 360, 400, 440, 480, 520
    dw  560, 600, 640, 680, 720, 760, 800, 840, 880, 920, 960,1000,1040,1080
    dw 1120,1160,1200,1240,1280,1320,1360,1400,1440,1480,1520,1560,1600,1640
    dw 1680,1720,1760,1800,1840,1880,1920,1960,2000,2040,2080,2120,2160,2200
    dw 2240,2280,2320,2360,2400,2440,2480,2520,2560,2600,2640,2680,2720,2760
    dw 2800,2840,2880,2920,2960,3000,3040,3080,3120,3160,3200,3240,3280,3320
    dw 3360,3400,3440,3480,3520,3560,3600,3640,3680,3720,3760,3800,3840,3880
    dw 3920,3960,4000,4040,4080,4120,4160,4200,4240,4280,4320,4360,4400,4440
    dw 4480,4520,4560,4600,4640,4680,4720,4760,4800,4840,4880,4920,4960,5000
    dw 5040,5080,5120,5160,5200,5240,5280,5320,5360,5400,5440,5480,5520,5560
    dw 5600,5640,5680,5720,5760,5800,5840,5880,5920,5960,6000,6040,6080,6120
    dw 6160,6200,6240,6280,6320,6360,6400,6440,6480,6520,6560,6600,6640,6680
    dw 6720,6760,6800,6840,6880,6920,6960,7000,7040,7080,7120,7160,7200,7240
    dw 7280,7320,7360,7400,7440,7480,7520,7560,7600,7640,7680,7720,7760,7800
    dw 7840,7880,7920,7960,8000,8040,8080,8120,8160,8200,8240,8280,8320,8360
    dw 8400,8440,8480,8520,8560,8600,8640,8680,8720,8760,8800,8840,8880,8920
    dw 8960,9000,9040,9080,9120,9160,9200,9240,9280,9320,9360,9400,9440,9480
    dw 9520,9560,9600,9640,9680,9720,9760,9800,9840,9880,9920,9960,10000,10040
    dw 10080,10120,10160,10200

VGAylookup dw    0, 320, 640, 960,1280,1600,1920,2240,2560,2880,3200,3520,3840,4160
    dw 4480,4800,5120,5440,5760,6080,6400,6720,7040,7360,7680,8000,8320,8640
    dw 8960,9280,9600,9920,10240,10560,10880,11200,11520,11840,12160,12480,12800,13120
    dw 13440,13760,14080,14400,14720,15040,15360,15680,16000,16320,16640,16960,17280,17600
    dw 17920,18240,18560,18880,19200,19520,19840,20160,20480,20800,21120,21440,21760,22080
    dw 22400,22720,23040,23360,23680,24000,24320,24640,24960,25280,25600,25920,26240,26560
    dw 26880,27200,27520,27840,28160,28480,28800,29120,29440,29760,30080,30400,30720,31040
    dw 31360,31680,32000,32320,32640,-32576,-32256,-31936,-31616,-31296,-30976,-30656,-30336,-30016
    dw -29696,-29376,-29056,-28736,-28416,-28096,-27776,-27456,-27136,-26816,-26496,-26176,-25856,-25536
    dw -25216,-24896,-24576,-24256,-23936,-23616,-23296,-22976,-22656,-22336,-22016,-21696,-21376,-21056
    dw -20736,-20416,-20096,-19776,-19456,-19136,-18816,-18496,-18176,-17856,-17536,-17216,-16896,-16576
    dw -16256,-15936,-15616,-15296,-14976,-14656,-14336,-14016,-13696,-13376,-13056,-12736,-12416,-12096
    dw -11776,-11456,-11136,-10816,-10496,-10176,-9856,-9536,-9216,-8896,-8576,-8256,-7936,-7616
    dw -7296,-6976,-6656,-6336,-6016,-5696,-5376,-5056,-4736,-4416,-4096,-3776,-3456,-3136
    dw -2816,-2496,-2176,-1856


;=======================================================================

;========
;
; StartupSound
;
; Sets up the new INT 8 ISR and various internal pointers.
; Assumes that the calling program has pointer SOUNDDATA to something
; meaningful...
;
;========

PROC StartupSound
	PUBLIC	StartupSound

	test	[_dontplay],0ffffh
	je	@@dowork
	ret
@@dowork:
	test	[SPKactive],0FFFFh	;see if library is active
	jne	@@started		;library was allready started

@@start:
	call	NEAR PTR StopSound	;make sure nothing is playing

	mov	ax,3508h	;call bios to get int 8
	int	21h
	mov	[WORD oldint8],bx
	mov	ax,es
	mov	[WORD oldint8+2],ax

	mov	ax,1
	mov	[extraints],al		;the timer is still going at the
	mov	[intcount],al		;normal rate now

	push	ds

	push	cs
	pop	ds
	lea	dx,[UpdateSPKR]
	mov	ax,2508h	;call bios to set int 8
	int	21h

	pop	ds

	inc	[SPKactive]	;sound routines are now active

@@started:
	mov	ax,1
	mov	[soundmode],ax ;set soundmode to SPKR
	ret

ENDP

;=======================================================================

;========
;
; ShutdownSound
;
;========

PROC	ShutdownSound
	PUBLIC ShutdownSound

	test	[_dontplay],0ffffh
	je	@@dowork
	ret
@@dowork:
	mov	al,36h		;tell the timer chip we are going to
	out	43h,al		;change the speed of timer 0
	mov	al,0		;system expects 0000 for rate
	out	40h,al		;low
	out	40h,al		;high

	mov	ax,[SPKactive]
	cmp	ax,0
	je	@@done		;sound library wasn't started...

	push	ds

	mov	dx,[WORD Oldint8]
	mov	ax,[WORD Oldint8+2]
	mov	ds,ax
	mov	ax,2508h	;call bios to set int 8
	int	21h

	pop	ds

	mov	[SPKactive],0	;sound routines are now inactive

	in	al,61h		;get peripheral (speaker) port value
	and	al,11111101b	;turn speaker off
	out	61h,al

@@done:
	ret

ENDP

;=========================================================================

;===========
;
; PlaySound (soundnum)
;
; If the sound's priority is >= the current priority, SoundPtr, SndPriority,
; and the timer speed are changed
;
;===========

PROC	PlaySound playnum:WORD
	PUBLIC PlaySound

	test	[_dontplay],0ffffh
	je	@@dowork
	ret
@@dowork:
	mov	ax,[playnum]	;index into the sound headers
	push	si

	mov	si,ax
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1

	mov	ax,[WORD SoundData+2]
	mov	es,ax		;point es: to the spkr file

	mov	al,[es:si+snd_Priority]	;priority table (one byte each)
	cmp	al,[SndPriority]
	jb	@@playdone	;current sound has higher priority
	mov	[SndPriority],al
	mov	ax,[es:si+snd_Start]	;offset in .SPK file
	mov	[SndPtr],ax	;store it in the sound playing table

	mov	bl,[es:si+snd_samples]	;samples / regular timer tick (1-255)
	mov	[extraints],bl	;an OldInt8 will be called after this
	mov	[intcount],bl	;many UpdateSPKR times have been called

	cmp	bl,1		;sample rate of 0 or 1 = 0000 for timer
	ja	@@oktodiv
	xor	bx,bx
	jmp	@@settimer

@@oktodiv:
	xor	bh,bh
	xor	ax,ax
	mov	dx,1
	div	bx		;$10000 / samples = timer rate

	mov	bx,ax
	mov	[sndspeed],bx	;save off the timer rate

@@settimer:
	mov	al,36h		;tell the timer chip we are going to
	out	43h,al		;change the speed of timer 0

	mov	al,bl		;low byte of sample rate
	out	40h,al
	mov	al,bh		;high byte of sample rate
	out	40h,al

@@playdone:
	pop	si
	ret

ENDP


;======================================================================

;===========
;
; StopSound
;
;===========

PROC	StopSound
	PUBLIC	StopSound

	test	[_dontplay],0ffffh
	je	@@dowork
	ret
@@dowork:
	xor	ax,ax		;set to 0
	mov	[SndPtr],ax
	mov	[SndPriority],al
	mov	[sndspeed],ax

	in	al,61h		;get peripheral (speaker) port value
	and	al,11111101b	;turn speaker off
	out	61h,al

	mov	al,36h		;tell the timer chip we are going to
	out	43h,al		;change the speed of timer 0

	mov	al,0		;back to normal clock speed
	out	40h,al
	out	40h,al

	inc	al
	mov	[extraints],al  ;one bios int / int8
	mov	[intcount],al

	ret
ENDP

;======================================================================

;===========
;
; PauseSound
;
;===========

PROC	PauseSound
	PUBLIC	PauseSound

	test	[_dontplay],0ffffh
	je	@@dowork
	ret
@@dowork:
	mov	ax,[SndPtr]	;save off the current values
	mov	[pausesndptr],ax
	mov	al,[SndPriority]
	mov	[pausepriority],al
	mov	al,[extraints]
	mov	[pauseextraints],al
	mov	al,[intcount]
	mov	[pauseintcount],al
	mov	ax,[sndspeed]
	mov	[pausespeed],ax
	call	StopSound
	ret
ENDP

;======================================================================

;===========
;
; ContinueSound
;
;===========

PROC	ContinueSound
	PUBLIC	ContinueSound

	test	[_dontplay],0ffffh
	je	@@dowork
	ret
@@dowork:
	mov	ax,[pausesndptr]
	mov	[SndPtr],ax	;restore the old values
	mov	al,[pausepriority]
	mov	[SndPriority],al
	mov	al,[pauseextraints]
	mov	[extraints],al
	mov	al,[pauseintcount]
	mov	[intcount],al

	mov	bx,[pausespeed]

	mov	al,36h		;tell the timer chip we are going to
	out	43h,al		;change the speed of timer 0

	mov	al,bl		;low byte of sample rate
	out	40h,al
	mov	al,bh		;high byte of sample rate
	out	40h,al


	ret
ENDP

;======================================================================

;========
;
; WaitendSound
; Just waits around until the current sound stops playing
;
;========

PROC	WaitEndSound
	PUBLIC WaitEndSound

	test	[_dontplay],0ffffh
	je	@@dowork
	ret
@@dowork:
	pushf
	call FAR PTR UpdateSPKR	;in case a sound was just started and hasn't
				;been hit by an INT yet
@@wait:
	mov	ax,[sndptr]
	cmp	ax,0		;when the ptr is 0, nothing is on
	jne	@@wait

	ret

ENDP

;=========================================================================

;========
;
; UpdateSPKR
; only called by interrupt $8!
;
;=========

PROC	UpdateSPKR FAR

	push	ax
	push	bx
	push	cx
	push	si
	push	ds
	push	es

	mov	ax,_DATA
	mov	ds,ax		;ds to this data segment
	mov	ax,[WORD SoundData+2]
	mov	es,ax		;es to sound file

	mov	al,20h
	out	20h,al		;we got the interrupt here

	dec	[intcount]	;see if it is time for a BIOS int 8...
	jne	@@dosounds

	mov	al,[extraints]
	mov	[intcount],al	;reset interrupt counter

	pushf			;so the IRET from bios returns right
	call	[OldInt8]	;call the old BIOS timer routine

@@dosounds:
;
; play the speaker
;
	mov	si,[SndPtr]
	cmp	si,0
	je	@@nosound	;nothing playing

	mov	bx,[es:si]
	inc	[SndPtr]
	inc	[SndPtr]

	cmp	bx,0
	je	@@nosound	;a zero frequency is no sound, but don't stop

	cmp	bx,-1		;a -1 frequency is end of sound
	jne	@@playfreq

	call	StopSound
	jmp	@@doneplay

@@nosound:
	in	al,61h		;get peripheral (speaker) port value
	and	al,11111100b	;turn speaker off
	out	61h,al
	jmp	@@doneplay

@@playfreq:
	test	[soundmode],0FFh	;if soundon=0, don't play anything
	je	@@nosound

	mov	al,10110110b	;write to channel 2 (speaker) timer
	out	43h,al
	mov	al,bl
	out	42h,al		;low byte
	mov	al,bh
	out	42h,al		;high byte

	in	al,61h		;get peripheral (speaker) port value
	or	al,00000011b	;turn speaker on to timer
	out	61h,al

@@doneplay:
	pop	es
	pop	ds
	pop	si
	pop	cx
	pop	bx
	pop	ax

	iret


ENDP

;==========================================================================

;========================================================================
	DATASEG

rndindex	dw	?

rndtable db    0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66
    db   74,  21, 211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36
    db   95, 110,  85,  48, 212, 140, 211, 249,  22,  79, 200,  50,  28, 188
    db   52, 140, 202, 120,  68, 145,  62,  70, 184, 190,  91, 197, 152, 224
    db  149, 104,  25, 178, 252, 182, 202, 182, 141, 197,   4,  81, 181, 242
    db  145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0
    db  175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,   2, 235
    db   25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113
    db   94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75
    db  136, 156,  11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196
    db  135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113
    db   80, 250, 108,   7, 255, 237, 129, 226,  79, 107, 112, 166, 103, 241
    db   24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224
    db  145, 224,  81, 206, 163,  45,  63,  90, 168, 114,  59,  33, 159,  95
    db   28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226
    db   71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36
    db   17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106
    db  197, 242,  98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136
    db  120, 163, 236, 249

;
; Random # Generator vars
;
indexi		dw	?	;Rnd#Generator
indexj		dw	?
LastRnd		dw	?
RndArray	dw	17 dup (?)

baseRndArray	dw	1,1,2,3,5,8,13,21,54,75,129,204
   		dw	323,527,850,1377,2227

	CODESEG

;=================================================
;
; Init RND generator
; if randomize is false, the counter is set to 0
;
; 11-Sep-90	LR	FIX initialization to use TIME!
;=================================================

PROC	initrnd	randomize:word
	public	initrnd

	push	si
	push	di

	mov	ax,ds
	mov	es,ax
	mov	di,offset RndArray
	mov	si,offset baseRndArray
	mov	cx,17
	cld
	rep	movsw		;set up the table (which is constantly changed)

	mov	[LastRnd],0
	mov	[indexi],17*2
	mov	[indexj],5*2

	mov	ax,[randomize]
	cmp	ax,0
	je	@@setit		;if randomize is true, really random

	mov	ah,2ch
	int	21h			;GetSystemTime

	mov	[RndArray+34-2],dx
	xor dx,cx				;init w/seconds values
	mov	[RndArray+10-2],dx

@@setit:
	mov	ax,0ffffh
	push	ax
	call	Random			;warm up generator!
	pop	ax

	pop	di
	pop	si
	ret

ENDP

;=================================================
;
; Return a random # between 0-?
; Exit : AX = 0-max value
;
; 11-Sep-90 LR -modify to save registers!
;=================================================
PROC	random maxval:WORD
	public	random

	push	si
	push	bx			;save registers so we work OK!
	push	cx
	push	dx

	mov	ax,[maxval]

	push	ax			;save max value
;
; create a mask to cut down on the # of SUBTRACTS!
;
	mov	dx,0ffffh		;full-mask
@@0:
	shl	ax,1
	jc	@@0a
	shr	dx,1
	jmp	@@0
@@0a:
	mov	bx,[indexi]		;this routine was converted from
	mov	si,[indexj]		;the Random macro on Merlin GS
	mov	ax,[RndArray-2+bx]
	adc	ax,[RndArray-2+si]
	mov	[RndArray-2+bx],ax
	add	ax,[LastRnd]
	mov	[LastRnd],ax
	dec	bx
	dec	bx
	jne	@@1
	mov	bx,17*2
@@1:
	dec	si
	dec	si
	jne	@@2
	mov	si,17*2
@@2:
	mov	[indexi],bx
	mov	[indexj],si
	pop	cx                      ;loop -- returns value in range
	and	ax,dx			;AND our mask!
@@3:
	cmp	ax,cx			;SUBTRACT to be within range
	jbe	@@4
	shr	ax,1
@@4:
	pop	dx
	pop	cx				;restore registers
	pop	bx
	pop	si

	ret

ENDP



;===========================================================================

;=================================================
;
; Init table based RND generator
; if randomize is false, the counter is set to 0
;
;=================================================

PROC	initrndt	randomize:word
	uses	si,di
	public	initrndt

	mov	ax,[randomize]
	cmp	ax,0
;	jne	@@timeit		;if randomize is true, really random

	mov	dx,0			;set to a definate value
	jmp	@@setit

@@timeit:
	mov	ah,2ch
	int	21h			;GetSystemTime
	and	dx,0ffh

@@setit:
	mov	[rndindex],dx

	ret

ENDP

;=================================================
;
; Return a random # between 0-255
; Exit : AX = value
;
;=================================================
PROC	rndt
	public	rndt

	mov	bx,[rndindex]
	inc	bx
	and	bx,0ffh
	mov	[rndindex],bx
	mov	al,[rndtable+BX]
	xor	ah,ah

	ret

ENDP



;===========================================================================

;========
;
; WAITVBL
;
;========

PROC	WaitVBL
	PUBLIC	WaitVBL

	push	si
	push	di

	mov	dx,[crtcaddr]
	add	dx,6

waitvbl1:
	in	al,dx
	test	al,00001000b	;look for vbl
	jnz	waitvbl1

waitvbl2:
	in	al,dx
	test	al,00001000b	;look for vbl
	jz	waitvbl2

	pop	di
	pop	si

	ret

ENDP

;=======================================================================

;====================
;
; EGAplane
; Sets read/write mode 0 and selects the given plane (0-3)
; for reading and writing
;
;====================

PROC	EGAplane plane:WORD
	PUBLIC	EGAplane

	mov	dx,GCindex
	mov	ax,GCmode
	out	dx,ax		;set read / write mode 0

	mov	dx,GCindex
	mov	al,4		;read map select
	mov	ah,[BYTE plane] ;read from this plane number
	out	dx,ax
	mov	dx,SCindex
	mov	al,SCmapmask
	mov	ah,1
	mov	cl,[BYTE plane]	;write to this plane only
	shl	ah,cl
	out	dx,ax
	ret

ENDP

;=======================================================================

;====================
;
; EGAlatch
; Sets write mode 1 with all planes selected
;
;====================

PROC	EGAlatch plane:WORD
	PUBLIC	EGAlatch

	mov	dx,GCindex
	mov	ax,1*256 + GCmode
	out	dx,ax		;set read 0 / write mode 1

	mov	dx,SCindex
	mov	ax,15*256 + SCmapmask	;write to all planes
	out	dx,ax
	ret
ENDP

;=======================================================================

;============
;
; drawchar
; Calls CGAcharout / EGAcharout / VGAcharout
; based on grmode
;
;============

PROC	drawchar xcoordinate:WORD, ycoordinate:WORD, char:WORD
	PUBLIC	drawchar

	push	si
	push	di

	mov	ax,[screenseg]
	mov	es,ax

	mov	si,[char]       	;low level routines get stuff in registers
	mov	di,[xcoordinate]
	mov	bx,[ycoordinate]
	shl	bx,1
	shl	bx,1
	shl	bx,1
	shl	bx,1
	add bx,[_yshift]		;allow printing on non-char lines

	mov	ax,[grmode]
	cmp	ax,1		;CGA mode
	jne	@@notcga
	call    CgaCharout
	pop   	di
	pop	si
	ret


@@notcga:
	cmp	ax,2		;EGA mode
	jne	@@notega
	call	EgaCharout
	pop   	di
	pop	si
	ret


@@notega:
	cmp	ax,3		;VGA mode
	jne	@@done
	call	VgaCharout
@@done:
	pop   	di
	pop	si
	ret

ENDP

;===========================================================================

;============
;
; CGAcharout
; SI= character; DI= xcoordinate; BX= ycoordinate*2
;
;============

PROC	CgaCharOut

	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1		;char * 16 = tile's offset in PICS

	shl	di,1		;x * 2 + ylookup[y] = screen location

	add	di,[CGAylookup+bx]	;BX is pointer into YLOOKUP

	mov	bx,[xormask]	;so chars can be inverted

	mov	ax,[WORD charptr+2]
	mov	ds,ax		;segment of tile pictures (PARA'd)

	cld

	lodsw			;load in a row of the tile's picture
	xor	ax,bx
	stosw
	add	di,1FFEh
	lodsw
	xor	ax,bx
	stosw
	sub	di,1FB2h
	lodsw
	xor	ax,bx
	stosw
	add	di,1FFEh
	lodsw
	xor	ax,bx
	stosw
	sub	di,1FB2h
	lodsw
	xor	ax,bx
	stosw
	add	di,1FFEh
	lodsw
	xor	ax,bx
	stosw
	sub	di,1FB2h
	lodsw
	xor	ax,bx
	stosw
	add	di,1FFEh
	lodsw
	xor	ax,bx
	stosw

	mov	ax,_DATA
	mov	ds,ax		;restore turbo's data segment
	ret
ENDP


;=======================================================================

;============
;
; EGAcharout
; SI= character; DI= xcoordinate; BX= ycoordinate
;
;============

PROC	EgaCharOut

	shl	si,1
	shl	si,1
	shl	si,1		;char * 8 = tile's offset in PICS

	add	di,[EGAylookup+bx]	;BX is pointer into YLOOKUP

	mov	cx,ds

	mov	dx,GCindex
	mov	ax,GCmode
	out	dx,ax		;set read / write mode 0

	cld

	mov	bx,si		;so the planes can be drawn the same
	mov	cx,di

	mov	dx,GCindex
	mov	ax,0*256+4	;read map select
	out	dx,ax
	mov	dx,SCindex
	mov	ax,1*256+SCmapmask
	out	dx,ax
	mov	dx,[xormask]	;so chars can be inverted

	mov	ax,[charptr+2]
	mov	ds,ax		;segment of tile pictures (PARA'd)

	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39

	mov	si,bx
	mov	di,cx

	push	dx
	mov	dx,GCindex
	mov	ax,1*256+4	;read map select
	out	dx,ax
	mov	dx,SCindex
	mov	ax,2*256+SCmapmask	;write mask
	out	dx,ax
	pop	dx

	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39

	mov	si,bx
	mov	di,cx

	push	dx
	mov	dx,GCindex
	mov	ax,2*256+4	;read map select
	out	dx,ax
	mov	dx,SCindex
	mov	ax,4*256+SCmapmask	;write mask
	out	dx,ax
	pop	dx

	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39

	mov	si,bx
	mov	di,cx

	push	dx
	mov	dx,GCindex
	mov	ax,3*256+4	;read map select
	out	dx,ax
	mov	dx,SCindex
	mov	ax,8*256+SCmapmask	;write mask
	out	dx,ax
	pop	dx

	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39
	lodsb
	xor	al,dl
	stosb
	add	di,39

	mov	dx,SCindex
	mov	ax,15*256 + SCmapmask	;write to all planes
	out	dx,ax

	mov	ax,_DATA
	mov	ds,ax		;restore turbo's data segment
	ret
ENDP


;=======================================================================

;============
;
; VGAcharout
; SI= character; DI= xcoordinate; BX= ycoordinate
;
;============

PROC	VgaCharOut

	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1		;char * 64 = tile's offset in PICS

	shl	di,1
	shl	di,1
	shl	di,1		;x * 8 + ylookup[y] = screen location

	add	di,[VGAylookup+bx]	;BX is pointer into YLOOKUP

	mov	bx,[xormask]	;so chars can be inverted

	mov	ax,[WORD charptr+2]
	mov	ds,ax		;segment of tile pictures (PARA'd)

	cld

	lodsw			;load in a row of the tile's picture
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw

	add	di,312

	lodsw			;load in a row of the tile's picture
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw

	add	di,312

	lodsw			;load in a row of the tile's picture
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw

	add	di,312

	lodsw			;load in a row of the tile's picture
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw

	add	di,312

	lodsw			;load in a row of the tile's picture
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw

	add	di,312

	lodsw			;load in a row of the tile's picture
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw

	add	di,312

	lodsw			;load in a row of the tile's picture
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw

	add	di,312

	lodsw			;load in a row of the tile's picture
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw
	lodsw
	xor	ax,bx
	stosw

	mov	ax,_DATA
	mov	ds,ax		;restore turbo's data segment
	ret
ENDP

;=======================================================================

;============
;
; drawtile
; Just drops a 16*16 tile onto the screen
; Calls CGAtileout / EGAtileout / VGAtileout
; based on grmode
;
;============

PROC	drawtile xcoordinate:WORD, ycoordinate:WORD, tile:WORD
	PUBLIC	drawtile

	push	si
	push	di

	cld

	mov	ax,[screenseg]
	mov	es,ax

	mov	si,[tile]
	mov	di,[xcoordinate] ;low level routines get stuff in registers
	mov	bx,[ycoordinate]
	shl	bx,1

	mov	ax,[grmode]

	mov	cx,[WORD tileptr+2]
	mov	ds,cx		;segment of pictures (PARA'd)

	cmp	ax,1		;CGA mode
	jne	@@notcga
	call    CGAtileout
	jmp	@@done

@@notcga:
	cmp	ax,2		;EGA mode
	jne	@@notega
	call	EGAtileout
	jmp	@@done

@@notega:
	cmp	ax,3		;VGA mode
	jne	@@done
	call	VGAtileout
@@done:

	mov	ax,_DATA
	mov	ds,ax		;restore turbo's data segment
	pop   	di
	pop	si
	ret

ENDP


;=======================
;
; CGAtileout
; SI= tile #, BX=y, DI=x
;
;=======================
PROC	CGAtileout
	PUBLIC	CGAtileout

	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1

	shl	bx,1

	add	di,[CGAylookup + BX]	;destination on screen

	movsw
	movsw
	add	di,8188
	movsw
	movsw
	sub	di,8116
	movsw
	movsw
	add	di,8188
	movsw
	movsw
	sub	di,8116
	movsw
	movsw
	add	di,8188
	movsw
	movsw
	sub	di,8116
	movsw
	movsw
	add	di,8188
	movsw
	movsw
	sub	di,8116
	movsw
	movsw
	add	di,8188
	movsw
	movsw
	sub	di,8116
	movsw
	movsw
	add	di,8188
	movsw
	movsw
	sub	di,8116
	movsw
	movsw
	add	di,8188
	movsw
	movsw
	sub	di,8116
	movsw
	movsw
	add	di,8188
	movsw
	movsw

	ret
ENDP

;=======================
;
; EGAtileout
; SI= tile #, BX=y, DI=x
;
;=======================
PROC	EGAtileout
	PUBLIC	EGAtileout

	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1
	add	di,[EGAylookup + BX]	;destination on screen

	mov	dx,SCindex
	mov	al,SCmapmask
	out	dx,al
	mov	al,1111b
	inc	dx
	out	dx,al

	mov	dx,GCindex
	mov	al,GCmode
	out	dx,al
	mov	al,1
	inc	dx
	out	dx,al

	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb
	add	di,38
	movsb
	movsb

	ret
ENDP


;=======================
;
; VGAtileout
; SI= tile #, BX=y, DI=x
;
;=======================
PROC	VGAtileout
	PUBLIC	VGAtileout

	mov	ah,al
	xor	al,al			;fast multiply by 256
	add	di,[VGAylookup + BX]	;destination on screen

	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	add	di,304
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw

	ret
ENDP



;=======================================================================

;============
;
; drawpic
; Just drops a width/depth block onto the screen with no masking
; Calls CGAdrawpic / EGAdrawpic / VGAdrawpic
; based on grmode
;
;============

PROC	drawpic xcoordinate:WORD, ycoordinate:WORD, pic:WORD
	PUBLIC	drawpic

	push	si
	push	di

	cld

	mov	si,[pic]
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1		;pictable records are 16 bytes

	mov	ax,[si+pictable.width]
	mov	[WORD cs:picwidth],ax
	mov	ax,[si+pictable.height]
	mov	[WORD cs:picheight],ax
	mov	si,[si+pictable.shapeptr]

	mov	ax,[screenseg]
	mov	es,ax

	mov	di,[xcoordinate] ;low level routines get stuff in registers
	mov	bx,[ycoordinate]
	shl	bx,1         	;because it will be indexed into a table

	mov	ax,[grmode]

	mov	cx,[WORD picptr+2]
	mov	ds,cx		;segment of pictures (PARA'd)

	cmp	ax,1		;CGA mode
	jne	@@notcga
	call    CgaDrawpic
	jmp	@@done

@@notcga:
	cmp	ax,2		;EGA mode
	jne	@@notega
	call	EgaDrawpic
	jmp	@@done

@@notega:
	cmp	ax,3		;VGA mode
	jne	@@done
	call	VgaDrawpic
@@done:
	mov	ax,_DATA
	mov	ds,ax		;restore turbo's data segment
	pop   	di
	pop	si
	ret

ENDP

picwidth	dw	?	;used by all modes
picxcoord	dw	?
picheight	dw	?

;===========================================================================

;============
;
; CgaDrawpic
; SI= source; DI= xcoordinate; BX= ycoordinate*2
;
;============

PROC	CgaDrawpic

	shr	di,1		;x / 4 + ylookup[y] = screen location
	shr	di,1
	mov	[WORD cs:picxcoord],di

@@drawrow:
	mov	cx,[WORD cs:picwidth] 	;number of words to write
	mov	di,[WORD cs:CGAylookup+bx]
	add	bx,2
	add	di,[WORD cs:picxcoord]
@@drawword:
	movsb
	loop	@@drawword
	dec	[WORD cs:picheight]
	jnz	@@drawrow

	ret
ENDP



;=======================================================================

;============
;
; EgaDrawpic
; SI= source; DI= xcoordinate; BX= ycoordinate
;
;============

PROC	EgaDrawpic

	mov	ax,105h
	mov	dx,3ceh		;set write mode 1
	out	dx,ax

	shr	di,1		;x / 8 + ylookup[y] = screen location
	shr	di,1
	shr	di,1
	mov	[WORD cs:picxcoord],di

	mov	ax,0A000h	;segment of screen memory
	mov	es,ax

@@drawrow:
	mov	cx,[WORD cs:picwidth] 	;number of bytes to write
	mov	di,[WORD cs:EGAylookup+bx]
	add	bx,2
	add	di,[WORD cs:picxcoord]
@@drawword:
	movsb
	loop	@@drawword
	dec	[WORD cs:picheight]
	jnz	@@drawrow

	ret
ENDP


;=======================================================================

;============
;
; VgaDrawpic
; SI= source; DI= xcoordinate; BX= ycoordinate
;
;============

PROC	VgaDrawpic

	mov	[WORD cs:picxcoord],di

@@drawrow:
	mov	cx,[WORD cs:picwidth] 	;number of words to write
	mov	di,[WORD cs:VGAylookup+bx]
	add	bx,2
	add	di,[WORD cs:picxcoord]
@@drawword:
	movsb
	loop	@@drawword
	dec	[WORD cs:picheight]
	jnz	@@drawrow

	ret
ENDP

;===========================================================================

	MASM
;อออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
;
; Name:	VideoID
;
; Function:	Detects the presence of various video subsystems
;
; int VideoID;
;
; Subsystem ID values:
; 	 0  = (none)
; 	 1  = MDA
; 	 2  = CGA
; 	 3  = EGA
; 	 4  = MCGA
; 	 5  = VGA
; 	80h = HGC
; 	81h = HGC+
; 	82h = Hercules InColor
;
;อออออออออออออออออออออออออออออออออออออออออออออออออออออออออ

;อออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
;
; Equates
;
;อออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
VIDstruct	STRUC		; corresponds to C data structure

Video0Type	DB	?	; first subsystem type
Display0Type	DB	? 	; display attached to first subsystem

Video1Type	DB	?	; second subsystem type
Display1Type	DB	?	; display attached to second subsystem

VIDstruct	ENDS


Device0	EQU	word ptr Video0Type[di]
Device1	EQU	word ptr Video1Type[di]


MDA	EQU	1	; subsystem types
CGA	EQU	2
EGA	EQU	3
MCGA	EQU	4
VGA	EQU	5
HGC	EQU	80h
HGCPlus	EQU	81h
InColor	EQU	82h

MDADisplay	EQU	1	; display types
CGADisplay	EQU	2
EGAColorDisplay	EQU	3
PS2MonoDisplay	EQU	4
PS2ColorDisplay	EQU	5

TRUE	EQU	1
FALSE	EQU	0

;อออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
;
; Program
;
;อออออออออออออออออออออออออออออออออออออออออออออออออออออออออ

Results	VIDstruct <>	;results go here!

EGADisplays	DB	CGADisplay	; 0000b, 0001b	(EGA switch values)
	DB	EGAColorDisplay	; 0010b, 0011b
	DB	MDADisplay	; 0100b, 0101b
	DB	CGADisplay	; 0110b, 0111b
	DB	EGAColorDisplay	; 1000b, 1001b
	DB	MDADisplay	; 1010b, 1011b

DCCtable	DB	0,0	; translate table for INT 10h func 1Ah
	DB	MDA,MDADisplay
	DB	CGA,CGADisplay
	DB	0,0
	DB	EGA,EGAColorDisplay
	DB	EGA,MDADisplay
	DB	0,0
	DB	VGA,PS2MonoDisplay
	DB	VGA,PS2ColorDisplay
	DB	0,0
	DB	MCGA,EGAColorDisplay
	DB	MCGA,PS2MonoDisplay
	DB	MCGA,PS2ColorDisplay

TestSequence	DB	TRUE	; this list of flags and addresses
	DW	FindPS2	;  determines the order in which this
			;  program looks for the various
EGAflag	DB	?	;  subsystems
	DW	FindEGA

CGAflag	DB	?
	DW	FindCGA

Monoflag	DB	?
	DW	FindMono

NumberOfTests	EQU	($-TestSequence)/3


	PUBLIC	VideoID
VideoID	PROC	near

	push	bp	; preserve caller registers
	mov	bp,sp
	push	ds
	push	si
	push	di

	push	cs
	pop	ds
	ASSUME	DS:@Code

; initialize the data structure that will contain the results

	lea	di,Results	; DS:DI -> start of data structure

	mov	Device0,0	; zero these variables
	mov	Device1,0

; look for the various subsystems using the subroutines whose addresses are
; tabulated in TestSequence; each subroutine sets flags in TestSequence
; to indicate whether subsequent subroutines need to be called

	mov	byte ptr CGAflag,TRUE
	mov	byte ptr EGAflag,TRUE
	mov	byte ptr Monoflag,TRUE

	mov	cx,NumberOfTests
	mov	si,offset TestSequence

@@L01:	lodsb		; AL := flag
	test	al,al
	lodsw		; AX := subroutine address
	jz	@@L02	; skip subroutine if flag is false

	push	si
	push	cx
	call	ax	; call subroutine to detect subsystem
	pop	cx
	pop	si

@@L02:	loop	@@L01

; determine which subsystem is active

	call	FindActive

	mov	al,Results.Video0Type
	mov	ah,0	; was:  Results.Display0Type

	pop	di	; restore caller registers and return
	pop	si
	pop	ds
	mov	sp,bp
	pop	bp
	ret

VideoID	ENDP


;
; FindPS2
;
; This subroutine uses INT 10H function 1Ah to determine the video BIOS
; Display Combination Code (DCC) for each video subsystem present.
;

FindPS2	PROC	near

	mov	ax,1A00h
	int	10h	; call video BIOS for info

	cmp	al,1Ah
	jne	@@L13	; exit if function not supported (i.e.,
			;  no MCGA or VGA in system)

; convert BIOS DCCs into specific subsystems & displays

	mov	cx,bx
	xor	bh,bh	; BX := DCC for active subsystem

	or	ch,ch
	jz	@@L11	; jump if only one subsystem present

	mov	bl,ch	; BX := inactive DCC
	add	bx,bx
	mov	ax,[bx+offset DCCtable]

	mov	Device1,ax

	mov	bl,cl
	xor	bh,bh	; BX := active DCC

@@L11:	add	bx,bx
	mov	ax,[bx+offset DCCtable]

	mov	Device0,ax

; reset flags for subsystems that have been ruled out

	mov	byte ptr CGAflag,FALSE
	mov	byte ptr EGAflag,FALSE
	mov	byte ptr Monoflag,FALSE

	lea	bx,Video0Type[di]  ; if the BIOS reported an MDA ...
	cmp	byte ptr [bx],MDA
	je	@@L12

	lea	bx,Video1Type[di]
	cmp	byte ptr [bx],MDA
	jne	@@L13

@@L12:	mov	word ptr [bx],0    ; ... Hercules can't be ruled out
	mov	byte ptr Monoflag,TRUE

@@L13:	ret

FindPS2	ENDP


;
; FindEGA
;
; Look for an EGA.  This is done by making a call to an EGA BIOS function
;  which doesn't exist in the default (MDA, CGA) BIOS.

FindEGA	PROC	near	; Caller:	AH = flags
			; Returns:	AH = flags
			;		Video0Type and
			;		 Display0Type updated

	mov	bl,10h	; BL := 10h (return EGA info)
	mov	ah,12h	; AH := INT 10H function number
	int	10h	; call EGA BIOS for info
			; if EGA BIOS is present,
			;  BL <> 10H
			;  CL = switch setting
	cmp	bl,10h
	je	@@L22	; jump if EGA BIOS not present

	mov	al,cl
	shr	al,1	; AL := switches/2
	mov	bx,offset EGADisplays
	xlat		; determine display type from switches
	mov	ah,al	; AH := display type
	mov	al,EGA	; AL := subystem type
	call	FoundDevice

	cmp	ah,MDADisplay
	je	@@L21	; jump if EGA has a monochrome display

	mov	CGAflag,FALSE	; no CGA if EGA has color display
	jmp	short @@L22

@@L21:	mov	Monoflag,FALSE	; EGA has a mono display, so MDA and
			;  Hercules are ruled out
@@L22:	ret

FindEGA	ENDP

;
; FindCGA
;
; This is done by looking for the CGA's 6845 CRTC at I/O port 3D4H.
;
FindCGA	PROC	near	; Returns:	VIDstruct updated

	mov	dx,3D4h	; DX := CRTC address port
	call	Find6845
	jc	@@L31	; jump if not present

	mov	al,CGA
	mov	ah,CGADisplay
	call	FoundDevice

@@L31:	ret

FindCGA	ENDP

;
; FindMono
;
; This is done by looking for the MDA's 6845 CRTC at I/O port 3B4H.  If
; a 6845 is found, the subroutine distinguishes between an MDA
; and a Hercules adapter by monitoring bit 7 of the CRT Status byte.
; This bit changes on Hercules adapters but does not change on an MDA.
;
; The various Hercules adapters are identified by bits 4 through 6 of
; the CRT Status value:
;
; 000b = HGC
; 001b = HGC+
; 101b = InColor card
;

FindMono	PROC	near	; Returns:	VIDstruct updated

	mov	dx,3B4h	; DX := CRTC address port
	call	Find6845
	jc	@@L44	; jump if not present

	mov	dl,0BAh	; DX := 3BAh (status port)
	in	al,dx
	and	al,80h
	mov	ah,al	; AH := bit 7 (vertical sync on HGC)

	mov	cx,8000h	; do this 32768 times
@@L41:	in	al,dx
	and	al,80h	; isolate bit 7
	cmp	ah,al
	loope	@@L41	; wait for bit 7 to change
	jne	@@L42	; if bit 7 changed, it's a Hercules

	mov	al,MDA	; if bit 7 didn't change, it's an MDA
	mov	ah,MDADisplay
	call	FoundDevice
	jmp	short @@L44

@@L42:	in	al,dx
	mov	dl,al	; DL := value from status port
	and	dl,01110000b	; mask bits 4 thru 6

	mov	ah,MDADisplay	; assume it's a monochrome display

	mov	al,HGCPlus	; look for an HGC+
	cmp	dl,00010000b
	je	@@L43	; jump if it's an HGC+

	mov	al,HGC	; look for an InColor card or HGC
	cmp	dl,01010000b
	jne	@@L43	; jump if it's not an InColor card

	mov	al,InColor	; it's an InColor card
	mov	ah,EGAColorDisplay

@@L43:	call	FoundDevice

@@L44:	ret

FindMono	ENDP

;
; Find6845
;
; This routine detects the presence of the CRTC on a MDA, CGA or HGC.
; The technique is to write and read register 0Fh of the chip (cursor
; low).  If the same value is read as written, assume the chip is
; present at the specified port addr.
;

Find6845	PROC	near	; Caller:  DX = port addr
			; Returns: cf set if not present
	mov	al,0Fh
	out	dx,al	; select 6845 reg 0Fh (Cursor Low)
	inc	dx
	in	al,dx	; AL := current Cursor Low value
	mov	ah,al	; preserve in AH
	mov	al,66h	; AL := arbitrary value
	out	dx,al	; try to write to 6845

	mov	cx,100h
@@L51:	loop	@@L51	; wait for 6845 to respond

	in	al,dx
	xchg	ah,al	; AH := returned value
			; AL := original value
	out	dx,al	; restore original value

	cmp	ah,66h	; test whether 6845 responded
	je	@@L52	; jump if it did (cf is reset)

	stc		; set carry flag if no 6845 present

@@L52:	ret

Find6845	ENDP


;
; FindActive
;
; This subroutine stores the currently active device as Device0.  The
; current video mode determines which subsystem is active.
;

FindActive	PROC	near

	cmp	word ptr Device1,0
	je	@@L63	; exit if only one subsystem

	cmp	Video0Type[di],4	; exit if MCGA or VGA present
	jge	@@L63	;  (INT 10H function 1AH
	cmp	Video1Type[di],4	;  already did the work)
	jge	@@L63

	mov	ah,0Fh
	int	10h	; AL := current BIOS video mode

	and	al,7
	cmp	al,7	; jump if monochrome
	je	@@L61	;  (mode 7 or 0Fh)

	cmp	Display0Type[di],MDADisplay
	jne	@@L63	; exit if Display0 is color
	jmp	short @@L62

@@L61:	cmp	Display0Type[di],MDADisplay
	je	@@L63	; exit if Display0 is monochrome

@@L62:	mov	ax,Device0	; make Device0 currently active
	xchg	ax,Device1
	mov	Device0,ax

@@L63:	ret

FindActive	ENDP


;
; FoundDevice
;
; This routine updates the list of subsystems.
;

FoundDevice	PROC	near	; Caller:    AH = display #
			;	     AL = subsystem #
			; Destroys:  BX
	lea	bx,Video0Type[di]
	cmp	byte ptr [bx],0
	je	@@L71	; jump if 1st subsystem

	lea	bx,Video1Type[di]	; must be 2nd subsystem

@@L71:	mov	[bx],ax	; update list entry
	ret

FoundDevice	ENDP

;============================================================================

	IDEAL

	DATASEG

tickcount	dw	?

timecall	dw	?
timeax		dw	?
timebx		dw	?
timecx		dw	?
timedx		dw	?
timesi		dw	?
timedi		dw	?
timebp		dw	?
timees		dw	?

	PUBLIC	timecall,timeax,timebx,timecx,timedx,timesi,timedi,timebp,timees

	CODESEG

;=============
;
; TIMESUB
; Returns the number of iterations of the given subroutine
; are possible in 1/18 second
;
;=============

PROC	timesub	ticks:WORD
	USES	si,di,bp
	PUBLIC	timesub

	mov	ax,[ticks]
	mov	[tickcount],ax

	mov	ax,351ch	;call bios to get int 1c
	int	21h
	mov	[cs:@@old1c],bx
	mov	ax,es
	mov	[cs:@@old1c+2],ax

	push	ds

	push	cs
	pop	ds
	lea	dx,[@@intermark]
	mov	ax,251ch	;call bios to set int 1c
	int	21h

	pop	ds

	call	stopsound	;make sure the timer is going normal speed
	xor	ax,ax
	mov	[cs:@@counter],ax	;set counter to 0
	mov	[cs:@@mark],ax	;will be inc to 1 after int 1c

@@wait1:
	mov	ax,[cs:@@mark]	;wait for the interrupt to go off once
	cmp	ax,0
	je	@@wait1

@@countmore:
	xor	ax,ax
	mov	[cs:@@mark],ax	;will be inc to 1 after int 1c
@@count:
	push	bp
	mov	ax,[timees]
	mov	es,ax
	mov	ax,[timeax]
	mov	bx,[timebx]
	mov	cx,[timecx]
	mov	dx,[timedx]
	mov	si,[timesi]
	mov	di,[timedi]
	mov	bp,[timebp]
	call	[timecall]
	pop	bp
	inc	[cs:@@counter]
	mov	ax,[cs:@@mark]	;has the interrupt gone off again?
	or	ax,ax
	je	@@count
	dec	[tickcount]
	jne	@@countmore

	push	ds

	mov	ax,[cs:@@old1c+2]
	mov	ds,ax
	mov	dx,[cs:@@old1c]
	mov	ax,251ch	;call bios to set int 1c
	int	21h

	pop	ds

	mov	ax,[cs:@@counter]	;return value
	ret

@@intermark:
	inc	[WORD cs:@@mark]
	jmp	[DWORD cs:@@old1C]


@@old1C	dw	0,0		;old [1Ch*4]
@@counter	dw	0
@@mark	dw	0		;set to 1 when the int has occured

ENDP

	END
