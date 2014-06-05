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

	%NOLIST
	%TITLE	"RLE compression v0.00"
;==========================================================
; RLE compression in assemlby.
; Written by Lane Roath, Copyright (c) 1990 IFD & Softdisk
;
;----------------------------------------------------------
;
;09-Aug-90 0.00	Start coding, converting source C in Dr. Dobb's
;
;==========================================================

	IDEAL

	MODEL	small,C
	STACK	0200


;==========================================================

MinCnt	=	3-1	;min count for repeat
MaxCnt	=	127+MinCnt	;max count (128+ = non repeat string)

	DATASEG

handle		dw	?	;file i/o requirements are here!
flength1	dw	?
flength2	dw	?
buf1		dw	?
buf2		dw	?
foff1		dw	?
foff2		dw	?

; We store our memory allocation results here

RLESeg	dw	?	;RLE packed buf (src/dest)
SrcSeg	dw	?	;original source data

RLEIdx	dw	?	;indexes for read/write bufs
SRCIdx	dw	?

SrcLen	dw	2 dup (?)	;length of source & dest bufs
RLELen	dw	2 dup (?)

ByteCnt	dw	?	;# of sequential bytes
LastByte	db	?	;last byte found
DifCnt	dw	?	;# of different bytes

EndByte	dw	?

;==========================================================
;
	CODESEG
;
;==========================================================
;
;	Initialize our variables & buffers
;
;==========================================================

PROC	InitRLE		;internal

	mov	[ByteCnt],0
	mov	[DifCnt],1
	mov	[RLELen],0
	mov	[RLELen+2],0
	mov	SI,[SrcIdx]	;init Source Index (SrcIdx)
	mov	DI,[RLEIdx]	; & dest index (RLEIdx)

	ret
ENDP


;==========================================================
;
;	Output a compression string (repeated bytes)
;
;==========================================================


PROC	OutputRep

	push	SI
	push	ES	;save needed registers
	push	AX

	mov	ES,[RLESeg]	;get dest buffer seg
	mov	AX,[ByteCnt]
	sub	AL,2	;get byte count - 3 (already -1)
	stosb

	mov	AL,[LastByte]	; & output byte to repeat
	stosb

	mov	SI,[SrcIdx]
	inc	SI	;update source index (length + 1)
	add	SI,[ByteCnt]
	mov	[SrcIdx],SI	; & save for next output

	add	[RLELen],2	;update RLE length
	adc	[RLELen+2],0

	mov	[ByteCnt],0	;we output everything!

	pop	AX
	pop	ES	;restore registers

;	mov	BL,[ES:SI]
;	mov	[LastByte],BL	;this is 'last' byte!

	pop	SI

	ret

	ENDP

;==========================================================
;
;	Output a unique string with count byte preceeding
;
;==========================================================


PROC	OutputDif

	push	ES	;save important registers
	push	AX
	push	SI

	mov	ES,[RLESeg]	;get dest buffer seg
	mov	AX,[DifCnt]
	add	AL,7Fh	; & save byte count - 1 with hi bit set
	stosb

	mov	CX,[DifCnt]	;get # of bytes to copy

	mov	AX,CX	;# of bytes read (count is -1)
	inc	AX
	add	[RLELen],AX	;update RLE length
	adc	[RLELen+2],0

	mov	SI,[SrcIdx]	; & starting position in source
	push	DS
	mov	DS,[SrcSeg]	;set source seg
	REP	movsb
	pop	DS	; & copy the string

	mov	[DifCnt],0	;we output everything!

	mov	[SrcIdx],SI	;save for next output

	pop	SI
	pop	AX
	pop	ES	;restore registers

	ret

	ENDP

;==========================================================
;
;	EXPAND a RLE compressed file segment
;
;==========================================================


	PUBLIC	Expand
PROC	Expand

	call	InitRLE	;init our vars & buffers

	xchg	SI,DI	;swap SI and DI
	mov	DX,1
@@bank:
	mov	ES,[SrcSeg]	;point to source buffer
	xor	AH,AH
@@get:
	cmp	DI,0FFF0h	;don't bog us down!
	jb	@@ok

	mov	AX,DI
	shr	AX,1
	shr	AX,1	;offset / 16
	shr	AX,1
	shr	AX,1
	add	[SrcSeg],AX	; + seg reg = normalized ptr
;	and	DI,0Fh
	mov	DI,0

	mov	AX,SI
	shr	AX,1
	shr	AX,1	;offset / 16
	shr	AX,1
	shr	AX,1
	add	[RLESeg],AX	; + segment register = normalized ptr
	and	SI,0Fh
	jmp	SHORT @@bank
@@ok:
	sub	[SrcLen],DX	;subtract out # of chars
	sbb	[SrcLen+2],0
	jc	@@Done	;exit if we are done!
	xor	DX,DX
@@0:
	push	DS
	mov	DS,[RLESeg]	;point to RLE buffer
	lodsb		; & get code byte
	cmp	AL,80h
	jb	@@rpt	;repeat code?
	sub	AL,7Fh
	mov	CX,AX	; no- string length = code - $80 + 1
	mov	DX,AX

	REP	movsb	;copy string
	pop	DS
	jmp	SHORT @@get	; & do some more
@@rpt:
	add	AL,3	;# of bytes = code + 3
	mov	CX,AX
	mov	DX,AX
	lodsb		;get byte to repeat
	REP	stosb
	pop	DS	; & store that many bytes
	jmp	SHORT @@get
@@Done:
	ret

	ENDP

;==========================================================
;
;	COMPRESS data buffer using RLE
;
;==========================================================

	PUBLIC	Compress
PROC	Compress

	call	InitRLE	;init vars
@@bank:
	mov	ES,[SrcSeg]	;get first byte of data
	mov	AL,[ES:SI]

	mov	[LastByte],AL	; & set as last byte
@@get:
	cmp	SI,0FF00h	;don't bog us down!
	jb	@@ok

	jmp	@@Done	;send any patial strings!

	mov	AX,SI
	shr	AX,1
	shr	AX,1	;offset / 16
	shr	AX,1
	shr	AX,1
	add	[SrcSeg],AX	; + segment register = normalized
	and	SI,0Fh
	mov	[SrcIdx],0	;which is our new 'start'

	mov	AX,DI
	shr	AX,1
	shr	AX,1	;offset / 16
	shr	AX,1
	shr	AX,1
	add	[RLESeg],AX	; + segment register = normalized ptr
	and	DI,0Fh
	inc	[DifCnt]
	jmp	SHORT @@bank
@@ok:
	sub	[SrcLen],1	;subtract out # of chars
	sbb	[SrcLen+2],0
	jnc	@@0
	jmp	NEAR @@Done	;exit if we are done!
@@0:
	inc	SI	;get next byte
	mov	AL,[ES:SI]
	cmp	AL,[LastByte]
	jne	@@dif	;same as last byte?

	inc	[ByteCnt]	; yes- count it
	cmp	[ByteCnt],MinCnt
	jb	@@get

	cmp	[DifCnt],1	;ok, got a rep string...already have dif?
	jbe	@@same
	dec	[DifCnt]
	call	OutputDif	; yes- output that string first
@@same:
	mov	[DifCnt],0	;force even if not output!
	cmp	[ByteCnt],MaxCnt
	jb	@@get	;have we gotten too big?

	call	OutputRep	; yes, save string & continue

	cmp	AL,[ES:SI+1]	;another repeat?
	jne	@@gget
	cmp	AL,[ES:SI+2]
        jne	@@gget
	inc	SI	;need this for >127 byte strings!
@@gget:	jmp	NEAR @@get
;-------------------------------------------------------------
@@dif:
	mov	CX,[ByteCnt]	;any dups yet?
	jcxz	@@cnt

	cmp	CX,MinCnt	;repeats before now?
	jb	@@add

	call	OutputRep	; yes- save repeat bytes
	jmp	SHORT @@cnt
@@add:
	add	[DifCnt],CX	;add bad repeats to dif count
	mov	[ByteCnt],0	; & no more dupes!
@@cnt:
	inc	[DifCnt]	;count byte as different
	mov	[LastByte],AL

	cmp	[DifCnt],MaxCnt-MinCnt-1	;still in range?
	jb	@@goget

	call	OutputDif	; no- output string & continue

;	mov	AL,[ES:SI+1]
	cmp	AL,[ES:SI+1]
	jne	@@goget	;if repeat follows, specialize!
	cmp	AL,[ES:SI+2]
	jne	@@goget
	inc	SI	;why?
        inc	[DifCnt]
@@goget:
	jmp	NEAR @@get
;-------------------------------------------------------------
@@Done:
	mov	CX,[ByteCnt]	;any reps left?
	jcxz	@@1

	cmp	CX,MinCnt	; yes- enough to output?
	jb	@@2

	cmp	[DifCnt],1	; yes, any difs before these?
	jb	@@3

	call	OutputDif	; yes- output them
@@3:
	call	OutputRep	;output rep bytes
	jmp	SHORT @@4
@@2:
	add	[DifCnt],CX	;update dif count w/small rep
@@1:
	mov	CX,[DifCnt]	;anything to output?
	jcxz	@@4
	call	OutputDif	;output dif string
@@4:
	ret

	ENDP

;====================================================
;
;	C interface to compress file
;
;====================================================

	PUBLIC	RLECompress

PROC	RLECompress NEAR Source:DWORD,_Length:DWORD,Destination:DWORD

	mov	AX,[WORD Source]
	mov	[SrcIdx],AX
	mov	AX,[WORD Source+2]
	mov	[SrcSeg],AX	;set source buffer

	mov	AX,[WORD Destination]
	mov	[RLEIdx],AX
	mov	AX,[WORD Destination+2]
	mov	[RLESeg],AX	; & destination buffer

	mov	AX,[WORD _Length]
	mov	[SrcLen],AX	;save length
	mov	AX,[WORD _Length+2]
	mov	[SrcLen+2],AX

	call	Compress	;do the compression

	mov	AX,[RLELen]	;return length of compressed file
	mov	DX,[RLELen+2]

	ret

	ENDP

;====================================================
;
;	C interface to expand file
;
;====================================================

	PUBLIC	RLEExpand

PROC	RLEExpand NEAR Source:DWORD,Destination:DWORD,_Length:DWORD

	mov	AX,[WORD Source]
	mov	[RLEIdx],AX
	mov	AX,[WORD Source+2]
	mov	[RLESeg],AX	;set source buffer

	mov	AX,[WORD Destination]
	mov	[SrcIdx],AX
	mov	AX,[WORD Destination+2]
	mov	[SrcSeg],AX	; & destination buffer

	mov	AX,[WORD _Length]
	mov	[SrcLen],AX	;save length
	mov	AX,[WORD _Length+2]
	mov	[SrcLen+2],AX

	call	Expand	;do the expansion

	ret

	ENDP

end


;=============================================
;
; Load a *LARGE* file into a FAR buffer!
; by John Romero (C) 1990 PCRcade
; To seperate ASM code by Lane Roath
;
;=============================================

	PUBLIC	LoadFile

PROC	LoadFile NEAR filename:WORD,buffer:DWORD

	mov	AX,[WORD buffer]	;buf1=FP_OFF(buffer)
	mov	[buf1],AX
	mov	AX,[WORD buffer+2]	;buf2=FP_SEG(buffer)
	mov	[buf2],AX

	mov	[foff1],0	;file offset = 0 (start)
	mov	[foff2],0

	mov	dx,[filename]
	mov	ax,3d00h		;OPEN w/handle (read only)
	int	21h
	jc	@@out

	mov	[handle],ax
	mov	bx,ax
	xor	cx,cx
	xor	dx,dx
	mov	ax,4202h
	int	21h			;SEEK (find file length)
	jc	@@out

	mov	[flength1],ax
	mov	[flength2],dx

	mov	cx,[flength2]
	inc	cx			;<- at least once!
@@L_1:
	push	cx

	mov	cx,[foff2]
	mov	dx,[foff1]
	mov	ax,4200h
	int	21h			;SEEK from start

	push	ds
	mov	bx,[handle]
	mov	cx,-1
	mov	dx,[buf1]
	mov	ax,[buf2]
	mov	ds,ax
	mov	ah,3fh			;READ w/handle
	int	21h
	pop	ds

	pop	cx
	jc	@@out
	cmp	ax,-1
	jne	@@out

	push	cx			;need to read the last byte
	push	ds			;into the segment! IMPORTANT!
	mov	bx,[handle]
	mov	cx,1
	mov	dx,[buf1]
	add	dx,-1
	mov	ax,[buf2]
	mov	ds,ax
	mov	ah,3fh
	int	21h
	pop	ds
	pop	cx

	add	[buf2],1000h
	inc	[foff2]
	loop	@@L_1
@@out:
	mov	bx,[handle]		;CLOSE w/handle
	mov	ah,3eh
	int	21h

	mov	AX,[flength1]		;(flength2*0x10000+flength1)
	mov	DX,[flength2]
	ret

	ENDP

;=============================================
;
; Save a *LARGE* file from a FAR buffer!
; by John Romero (C) 1990 PCRcade
; To seperate ASM code by Lane Roath
;
;=============================================

	PUBLIC	SaveFile

PROC	SaveFile NEAR filename:WORD,buffer:DWORD,fsize:DWORD

	mov	AX,[WORD buffer]	;buf1=FP_OFF(buffer)
	mov	[buf1],AX
	mov	AX,[WORD buffer+2]	;buf2=FP_SEG(buffer)
	mov	[buf2],AX


	mov	[foff1],0  		;file offset = 0 (start)
	mov	[foff2],0

	mov	dx,[filename]
	mov	ax,3c00h		;CREATE w/handle (read only)
	xor	cx,cx
	int	21h
	jc	@@out

	mov	[handle],ax
	cmp	[WORD fsize+2],0	;larger than 1 segment?
	je	@@L2
@@L1:
	push	ds
	mov	bx,[handle]
	mov	cx,8000h
	mov	dx,[buf1]
	mov	ax,[buf2]
	mov	ds,ax
	mov	ah,40h			;WRITE w/handle
	int	21h
	pop	ds

	add	[buf2],800h		;bump ptr up 1/2 segment
	sub	[WORD fsize],8000h		;done yet?
	sbb	[WORD fsize+2],0
	cmp	[WORD Fsize+2],0
	ja	@@L1
	cmp	[WORD fsize],8000h
	jae	@@L1
@@L2:
	push	ds
	mov	bx,[handle]
	mov	cx,[WORD fsize]
	mov	dx,[buf1]
	mov	ax,[buf2]
	mov	ds,ax
	mov	ah,40h			;WRITE w/handle
	int	21h
	pop	ds
@@out:
	mov	bx,[handle]		;CLOSE w/handle
	mov	ah,3eh
	int	21h

	RET

	ENDP

	END