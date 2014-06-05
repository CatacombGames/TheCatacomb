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

	MASM

	locals
	.MODEL	SMALL,C

	.DATA

	EXTRN	xormask:WORD
	EXTRN	oldtiles, view, background, originx, originy: WORD
	EXTRN	priority: BYTE
	EXTRN	obj, altobj: objtype
	EXTRN	pics:WORD


objtype	STRUC

active	dw	?
class 	dw	?
x	db	?
y	db	?
stage	db	?
delay	db	?
dir	dw	?
hp	db	?
oldx	db	?
oldy	db	?
oldtile dw	?
filler1	db	?

think 	db	?
contact db	?
solid	db	?
firstchar dw	?
psize	db	?
stages	db	?
dirmask db	?
speed 	dw	?
hitpoints db	?
damage 	db	?
points 	dw	?
filler2	db	?,?

objtype	ENDS

obp	dw	?		;temporary for turbo's BP


	.CODE

;========================================================================

;{=========================================}
;{                                         }
;{ DRAWOBJ                                 }
;{ Draws the object to TILES in the proper }
;{ direction and state.                    }
;{                                         }
;{=========================================}
;
;Procedure DrawObj;
;var
;  objpri,bsize,tilenum:integer;
;Begin
;  tilenum:=obj.firstchar + obj.size * obj.size
;  *((integer(obj.dir) and obj.dirmask) * obj.stages + obj.stage);
;  obj.oldtile:= tilenum;
;  obj.oldx:=obj.x;
;  obj.oldy:=obj.y;
;  objpri:=priority[tilenum];
;  For y:=obj.y to obj.y+obj.size-1 do
;    for x:=obj.x to obj.x+obj.size-1 do
;      Begin
;	if priority[view[y,x]]<=objpri then
;	  view[y,x]:=tilenum;
;	inc(tilenum);
;      end;
;End;
;
;
squares	db	0,1,4,9,16,25,36,49,64

table86 dw    0,  86, 172, 258, 344, 430, 516, 602, 688, 774, 860, 946,1032,1118
    dw 1204,1290,1376,1462,1548,1634,1720,1806,1892,1978,2064,2150,2236,2322
    dw 2408,2494,2580,2666,2752,2838,2924,3010,3096,3182,3268,3354,3440,3526
    dw 3612,3698,3784,3870,3956,4042,4128,4214,4300,4386,4472,4558,4644,4730
    dw 4816,4902,4988,5074,5160,5246,5332,5418,5504,5590,5676,5762,5848,5934
    dw 6020,6106,6192,6278,6364,6450,6536,6622,6708,6794,6880,6966,7052,7138
    dw 7224,7310,7396


drawobj	PROC   NEAR
	USES	SI,DI
	PUBLIC drawobj

	mov	al,BYTE PTR obj.dir
	and	al,obj.dirmask
	mul	obj.stages
	add	al,obj.stage
	mov	cl,al

	mov	bl,obj.psize
	xor	bh,bh
	mov	al,cs:[squares+bx]

	mul	cl

	add	ax,obj.firstchar

	mov	SI,ax
	mov	obj.oldtile,SI		;SI holds the first tile to put in

	mov	dl,[priority+SI]	;entire object has same priority
					;priority is saved in DL

	mov	bl,obj.y
	mov	obj.oldy,bl
	xor	bh,bh
	shl	bx,1
	mov	ax,cs:[table86+bx] ;View is 86*86
	mov	bl,obj.x
	mov     obj.oldx,bl
	xor	bh,bh
	add	ax,bx		;calculate origin's offset in VIEW
	shl	ax,1		;becuase view is WORD width
	mov	di,ax		;DI will point into VIEW

	mov	al,obj.psize	;throughout loop
	xor	ah,ah

	mov	dh,al		;do this many lines
@@yloop:
	mov	cx,ax		;do this many characters / line

@@xloop:
	mov     bx,[view+DI]
	cmp	dl,[priority+bx] ;check tiles priority level
	jb	@@next		;don't draw if lower than what's there
	mov	[view+di],si

@@next:
	inc	si
	inc	di
	inc	di
	loop	@@xloop

	sub	di,ax
	sub	di,ax
	add	di,86*2		;position destination at start of next line

	dec	dh		;any more lines to do?
	jnz	@@yloop

	ret

DrawObj	ENDP

;========================================================================

;{=======================================}
;{                                       }
;{ ERASEOBJ                              }
;{ Erases the current object by copying  }
;{ the background onto the view where the}
;{ object is standing                    }
;{                                       }
;{=======================================}
;
;Procedure EraseObj;
;var
;  objpri,bsize,tilenum:integer;
;Begin
;  tilenum:=obj.oldtile;
;  For y:=obj.oldy to obj.oldy+obj.size-1 do
;    for x:=obj.oldx to obj.oldx+obj.size-1 do
;      Begin
;	if view[y,x]=tilenum then
;	  view[y,x]:=background[y,x];
;	inc(tilenum);
;      end;
;End;
;
eraseobj PROC   NEAR
	USES	SI,DI
	PUBLIC eraseobj

	mov	SI,obj.oldtile ;only erase chars that match what was
				;drawn by the last drawobj
	mov	bl,obj.oldy
	xor	bh,bh
	shl	bx,1
	mov	ax,cs:[table86+bx] ;View is 86*86
	mov	bl,obj.oldx
	xor	bh,bh
	add	ax,bx		;calculate origin's offset in VIEW
	shl	ax,1		;becuase view is WORD width
	mov	di,ax		;DI will point into VIEW

	mov	al,obj.psize	;throughout loop
	xor	ah,ah

	mov	dh,al		;do this many lines
@@yloop:
	mov	cx,ax		;do this many characters / line

@@xloop:
	cmp     si,[view+DI]
	jne	@@next		;don't erase if its not part of the shape
	mov	bx,[background+di]
	mov	[view+di],bx	;erase it

@@next:
	inc	si
	inc	di
	inc	di
	loop	@@xloop

	sub	di,ax
	sub	di,ax
	add	di,86*2		;position destination at start of next line

	dec	dh		;any more lines to do?
	jnz	@@yloop

	ret

EraseObj	ENDP

;========================================================================

;{====================}
;{                    }
;{ DoAll              }
;{ The main play loop }
;{                    }
;{====================}
;
;Procedure Doall;
;begin
;  Repeat  {until leveldone or playdone}
;    For objecton:=numobj downto 0 do
;      Begin
;	move (o[objecton],obj.active,sizeof(o[objecton]) );
;	if obj.class<>nothing then {class=nothing means it was killed}
;	  Begin
;	    move (ObjDef[obj.class],obj.think,sizeof(objdef[obj.class]) );
;	    if obj.active then
;	      DoActive
;	    else
;	      DoInactive;
;	  end;
;      end;
;   refresh;
;   inc (frameon);
;  until leveldone or playdone;
;end;

	.DATA

	EXTRN	o:objtype
	EXTRN   objdef:WORD	;actually the second half of objtype record
	EXTRN	frameon:WORD
	EXTRN	numobj:WORD
	EXTRN	objecton:WORD
	EXTRN	leveldone:BYTE
	EXTRN	playdone:BYTE

	.CODE

	EXTRN	refresh:NEAR
	EXTRN	doactive:NEAR
	EXTRN	doinactive:NEAR

doall	PROC	NEAR
	USES	SI,DI
	PUBLIC	doall

@@repeat:
	mov	ax,[numobj]
	mov	[objecton],ax
@@forobjecton:
	mov	si,[objecton]
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1		;o[] is 16 bytes wide
	add	si,OFFSET o
	mov	di,OFFSET obj
	push	ds
	pop	es
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw			;copy 16 bytes

	mov     al,[BYTE PTR obj.class]
	or	al,al
	jz	@@next
	xor	ah,ah
	mov	si,ax
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1		;objdef is 16 bytes wide
	add	si,OFFSET objdef
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw
	movsw			;copy second 16 bytes into obj

	mov	al,[BYTE PTR obj.active]
	or	al,al
	jnz	@@isactive
	call	DoInactive
	jmp	@@next
@@isactive:
	call	DoActive

@@next:
	mov	al,[leveldone]	; check end
	or	al,al
	jnz	@@done
	mov	al,[playdone]
	or	al,al
	jnz	@@done

	dec	[objecton]
	jns	@@forobjecton	; END for

	call	refresh
	inc	[frameon]

	mov	al,[leveldone]	; UNTIL
	or	al,al
	jnz	@@done
	mov	al,[playdone]
	or	al,al
	jz	@@repeat

@@done:
	ret

DoAll	ENDP

;========================================================================


;様様様様様様様様様様様様様様様様様様様様様様様様様様様様様様様
;
; Move EGA tiles into EGA memory at "EGATileLoc"!
;
;様様様様様様様様様様様様様様様様様様様様様様様様様様様様様様様
EGAmove	PROC	NEAR
	USES	SI,DI
	PUBLIC	EGAmove

	mov	dx,3c4h 	;turn MapMask register on!
	mov	al,2
	out	dx,al
	inc	dx

	mov	ax,0A400h
	mov	es,ax	;ES:DI = just after screen in latch memory
	xor	di,di

	mov	ax,WORD PTR [pics+2]
	mov	ds,ax
	xor	si,si	;DS:SI = start of tiles!

	cld
	mov	cx,2048

@@0:
	mov	ah,1b	;start at bitplane 0
	mov	bx,di

@@2:
	mov	al,ah
	out	dx,al	;select new bitplane!

	movsb
	movsb
	movsb
	movsb
	movsb
	movsb
	movsb
	movsb

	mov	di,bx

	shl	ah,1
	test	ah,1111b
	jnz	@@2	;do all bitplanes in shape

	add	di,8

	loop	@@0	;do all tiles

	mov	al,1111b
	out	dx,al	;select all bitplanes

	mov	ax,ss	;reset DATA segment
	mov	ds,ax

	ret

EGAMove	ENDP

;=======================================================================

;=========
;
; CGAREFRESH redraws the tiles that have changed in the tiled screen area
;
;=========

cgarefresh PROC	NEAR
	USES	SI,DI
	PUBLIC	cgarefresh

	mov	obp,bp		;save off turbo's BP

	mov	ax,0B800h	;start of CGA memory
	mov	es,ax
	cld			;just in case
	mov	cx,OFFSET @@next	;so it can be JMPd to

	mov	ax,originy
	mov	bl,86		;View is 86*86 so no clipping
	mul	bl
	add	ax,originx	;calculate origin's offset in VIEW
	shl	ax,1		;becuase view is WORD width
	mov	bp,ax		;BP will point into VIEW
	mov	dx,ax
	add	dx,48		;when BP=DX, one row has been filled

	xor	bx,bx		;fast mov bx,0

@@check:
	mov	ax,view[bp]	;load the current tile
	cmp	ax,oldtiles[bx]	;compare it with the old tile
	jne	@@drawone		;if different, redraw
@@next:
	add	bx,2		;next oldtile
	add	bp,2		;next view tile
	cmp	bp,dx		;has an entire row from VIEW been drawn?
	jne	@@check

	cmp	bx,24*24*2	;have all tiles been drawn?
	je	@@done
	add	bp,124		;point it to the start of the next row
	add	dx,172		;point to end of next row
	jmp	@@check

@@done:
	mov	bp,obp		;restore turbo's BP
	ret

@@drawone:
	mov	oldtiles[bx],ax	;store the tile back to the oldtiles
	mov	di,word ptr cs:CGAtileloc[bx]	;set di to screen address
	shl	ax,1		;character number * 16 = start of data
	shl	ax,1
	shl	ax,1
	shl	ax,1
	mov	si,ax
	mov	ds,[pics+2]	;segment of pics (para aligned)
	movsw			;load in a row of the tile's picture
	add	di,1FFEh
	movsw
	sub	di,1FB2h
	movsw
	add	di,1FFEh
	movsw
	sub	di,1FB2h
	movsw
	add	di,1FFEh
	movsw
	sub	di,1FB2h
	movsw
	add	di,1FFEh
	movsw
	mov	ax,ss
	mov	ds,ax		;restore turbo's data segment
	jmp	CX		;CX holds OFFSET NEXT


CgaRefresh ENDP


;=======================================================================

;=========
;
; EGAREFRESH redraws the tiles that have changed in the tiled screen area
;
;=========

egarefresh PROC	NEAR
	USES	SI,DI
	PUBLIC	egarefresh

	mov	obp,bp		;save off turbo's BP

	mov	ax,105h
	mov	dx,3ceh		;set write mode 1
	out	dx,ax

	mov	ax,0A000h	;start of EGA memory
	mov	es,ax
	cld			;just in case
	mov	cx,OFFSET @@next	;so it can be JMPd to

	mov	ax,originy
	mov	bl,86		;View is 86*86 so no clipping
	mul	bl
	add	ax,originx	;calculate origin's offset in VIEW
	shl	ax,1		;becuase view is WORD width
	mov	bp,ax		;BP will point into VIEW
	mov	dx,ax
	add	dx,48		;when BP=DX, one row has been filled

	xor	bx,bx		;fast mov bx,0

@@check:
	mov	ax,view[bp]	;load the current tile
	cmp	ax,oldtiles[bx]	;compare it with the old tile
	jne	@@drawone		;if different, redraw
@@next:
	add	bx,2		;next oldtile
	add	bp,2		;next view tile
	cmp	bp,dx		;has an entire row from VIEW been drawn?
	jne	@@check

	cmp	bx,24*24*2	;have all tiles been drawn?
	je	@@done
	add	bp,124		;point it to the start of the next row
	add	dx,172		;point to end of next row
	jmp	@@check

@@done:
	mov	bp,obp		;restore turbo's BP
	ret

@@drawone:
	mov	oldtiles[bx],ax	;store the tile back to the oldtiles
	mov	di,word ptr cs:EGAtileloc[bx]	;set di to screen address
	shl	ax,1		;character number * 8 = start of data
	shl	ax,1
	shl	ax,1
	add	ax,4000h	;because the ega pics are in same bank as screen
	mov	si,ax

	mov	ax,es
	mov	ds,ax		;pics are just later in screen memory

	movsb			;load in a row of the tile's picture
	add	di,39
	movsb
	add	di,39
	movsb
	add	di,39
	movsb
	add	di,39
	movsb
	add	di,39
	movsb
	add	di,39
	movsb
	add	di,39
	movsb

	mov	ax,ss
	mov	ds,ax		;restore turbo's data segment
	jmp	CX		;CX holds OFFSET NEXT


EgaRefresh ENDP


dataseg

linetables	dw	0,0

blockingtiles	db 	128 dup (0)
		db	1,1,1,1,1,1,1
		db	2048 dup (0)
codeseg

;===========================================================================

if 0
;=========
;
; EGAREFRESH with hidden area removal
;
;=========

egarefresh PROC	NEAR
	USES	SI,DI
	PUBLIC	egarefresh

	mov	obp,bp		;save off turbo's BP

	mov	ax,105h
	mov	dx,3ceh		;set write mode 1
	out	dx,ax

	mov	ax,0A000h	;start of EGA memory
	mov	es,ax
	cld			;just in case
	mov	cx,OFFSET @@next	;so it can be JMPd to

	mov	ax,originy
	mov	bl,86		;View is 86*86 so no clipping
	mul	bl
	add	ax,originx	;calculate origin's offset in VIEW
	shl	ax,1		;becuase view is WORD width
	mov	bp,ax		;BP will point into VIEW
	mov	dx,ax
	add	dx,48		;when BP=DX, one row has been filled

	xor	bx,bx		;fast mov bx,0

@@check:
	mov	cx,view[bp]	;load the current tile
;
; check for blocking tiles
;
	mov	ds,[linetables+2]
	mov	si,[bx]	;start of blocking table
@@checkblock:
	lodsw
	test	ax,ax
	jz	@@cansee	;0 is end of offset list
	add	si,bx		;si holds tile offset from map start
	mov	si,[ss:view+si]
	cmp	[BYTE PTR ss:blockingtiles+si],0
	jz	@@checkblock

	xor	cx,cx		;can't see it, draw black

@@cansee:
	mov	ax,ss
	mov	ds,ax		;restore data segment
	mov	ax,cx

	cmp	ax,oldtiles[bx]	;compare it with the old tile
	jne	@@drawone		;if different, redraw
@@next:
	add	bx,2		;next oldtile
	add	bp,2		;next view tile
	cmp	bp,dx		;has an entire row from VIEW been drawn?
	jne	@@check

	cmp	bx,24*24*2	;have all tiles been drawn?
	je	@@done
	add	bp,124		;point it to the start of the next row
	add	dx,172		;point to end of next row
	jmp	@@check

@@done:
	mov	bp,obp		;restore turbo's BP
	ret

@@drawone:
	mov	oldtiles[bx],ax	;store the tile back to the oldtiles
	mov	di,word ptr cs:EGAtileloc[bx]	;set di to screen address
	shl	ax,1		;character number * 8 = start of data
	shl	ax,1
	shl	ax,1
	add	ax,4000h	;because the ega pics are in same bank as screen
	mov	si,ax

	mov	ax,es
	mov	ds,ax		;pics are just later in screen memory

	movsb			;load in a row of the tile's picture
	add	di,39
	movsb
	add	di,39
	movsb
	add	di,39
	movsb
	add	di,39
	movsb
	add	di,39
	movsb
	add	di,39
	movsb
	add	di,39
	movsb

	mov	ax,ss
	mov	ds,ax		;restore turbo's data segment
	jmp	CX		;CX holds OFFSET NEXT


EgaRefresh ENDP

endif

;=======================================================================


;=========
;
; TILELOC has the offsets from $B800 of all 24*24 tiles
;
;=========

CGAtileloc	label	word
    dw    0,   2,   4,   6,   8,  10,  12,  14,  16,  18,  20,  22,  24,  26
    dw   28,  30,  32,  34,  36,  38,  40,  42,  44,  46, 320, 322, 324, 326
    dw  328, 330, 332, 334, 336, 338, 340, 342, 344, 346, 348, 350, 352, 354
    dw  356, 358, 360, 362, 364, 366, 640, 642, 644, 646, 648, 650, 652, 654
    dw  656, 658, 660, 662, 664, 666, 668, 670, 672, 674, 676, 678, 680, 682
    dw  684, 686, 960, 962, 964, 966, 968, 970, 972, 974, 976, 978, 980, 982
    dw  984, 986, 988, 990, 992, 994, 996, 998,1000,1002,1004,1006,1280,1282
    dw 1284,1286,1288,1290,1292,1294,1296,1298,1300,1302,1304,1306,1308,1310
    dw 1312,1314,1316,1318,1320,1322,1324,1326,1600,1602,1604,1606,1608,1610
    dw 1612,1614,1616,1618,1620,1622,1624,1626,1628,1630,1632,1634,1636,1638
    dw 1640,1642,1644,1646,1920,1922,1924,1926,1928,1930,1932,1934,1936,1938
    dw 1940,1942,1944,1946,1948,1950,1952,1954,1956,1958,1960,1962,1964,1966
    dw 2240,2242,2244,2246,2248,2250,2252,2254,2256,2258,2260,2262,2264,2266
    dw 2268,2270,2272,2274,2276,2278,2280,2282,2284,2286,2560,2562,2564,2566
    dw 2568,2570,2572,2574,2576,2578,2580,2582,2584,2586,2588,2590,2592,2594
    dw 2596,2598,2600,2602,2604,2606,2880,2882,2884,2886,2888,2890,2892,2894
    dw 2896,2898,2900,2902,2904,2906,2908,2910,2912,2914,2916,2918,2920,2922
    dw 2924,2926,3200,3202,3204,3206,3208,3210,3212,3214,3216,3218,3220,3222
    dw 3224,3226,3228,3230,3232,3234,3236,3238,3240,3242,3244,3246,3520,3522
    dw 3524,3526,3528,3530,3532,3534,3536,3538,3540,3542,3544,3546,3548,3550
    dw 3552,3554,3556,3558,3560,3562,3564,3566,3840,3842,3844,3846,3848,3850
    dw 3852,3854,3856,3858,3860,3862,3864,3866,3868,3870,3872,3874,3876,3878
    dw 3880,3882,3884,3886,4160,4162,4164,4166,4168,4170,4172,4174,4176,4178
    dw 4180,4182,4184,4186,4188,4190,4192,4194,4196,4198,4200,4202,4204,4206
    dw 4480,4482,4484,4486,4488,4490,4492,4494,4496,4498,4500,4502,4504,4506
    dw 4508,4510,4512,4514,4516,4518,4520,4522,4524,4526,4800,4802,4804,4806
    dw 4808,4810,4812,4814,4816,4818,4820,4822,4824,4826,4828,4830,4832,4834
    dw 4836,4838,4840,4842,4844,4846,5120,5122,5124,5126,5128,5130,5132,5134
    dw 5136,5138,5140,5142,5144,5146,5148,5150,5152,5154,5156,5158,5160,5162
    dw 5164,5166,5440,5442,5444,5446,5448,5450,5452,5454,5456,5458,5460,5462
    dw 5464,5466,5468,5470,5472,5474,5476,5478,5480,5482,5484,5486,5760,5762
    dw 5764,5766,5768,5770,5772,5774,5776,5778,5780,5782,5784,5786,5788,5790
    dw 5792,5794,5796,5798,5800,5802,5804,5806,6080,6082,6084,6086,6088,6090
    dw 6092,6094,6096,6098,6100,6102,6104,6106,6108,6110,6112,6114,6116,6118
    dw 6120,6122,6124,6126,6400,6402,6404,6406,6408,6410,6412,6414,6416,6418
    dw 6420,6422,6424,6426,6428,6430,6432,6434,6436,6438,6440,6442,6444,6446
    dw 6720,6722,6724,6726,6728,6730,6732,6734,6736,6738,6740,6742,6744,6746
    dw 6748,6750,6752,6754,6756,6758,6760,6762,6764,6766,7040,7042,7044,7046
    dw 7048,7050,7052,7054,7056,7058,7060,7062,7064,7066,7068,7070,7072,7074
    dw 7076,7078,7080,7082,7084,7086,7360,7362,7364,7366,7368,7370,7372,7374
    dw 7376,7378,7380,7382,7384,7386,7388,7390,7392,7394,7396,7398,7400,7402
    dw 7404,7406

EGAtileloc	label	word
    dw    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13
    dw   14,  15,  16,  17,  18,  19,  20,  21,  22,  23, 320, 321, 322, 323
    dw  324, 325, 326, 327, 328, 329, 330, 331, 332, 333, 334, 335, 336, 337
    dw  338, 339, 340, 341, 342, 343, 640, 641, 642, 643, 644, 645, 646, 647
    dw  648, 649, 650, 651, 652, 653, 654, 655, 656, 657, 658, 659, 660, 661
    dw  662, 663, 960, 961, 962, 963, 964, 965, 966, 967, 968, 969, 970, 971
    dw  972, 973, 974, 975, 976, 977, 978, 979, 980, 981, 982, 983,1280,1281
    dw 1282,1283,1284,1285,1286,1287,1288,1289,1290,1291,1292,1293,1294,1295
    dw 1296,1297,1298,1299,1300,1301,1302,1303,1600,1601,1602,1603,1604,1605
    dw 1606,1607,1608,1609,1610,1611,1612,1613,1614,1615,1616,1617,1618,1619
    dw 1620,1621,1622,1623,1920,1921,1922,1923,1924,1925,1926,1927,1928,1929
    dw 1930,1931,1932,1933,1934,1935,1936,1937,1938,1939,1940,1941,1942,1943
    dw 2240,2241,2242,2243,2244,2245,2246,2247,2248,2249,2250,2251,2252,2253
    dw 2254,2255,2256,2257,2258,2259,2260,2261,2262,2263,2560,2561,2562,2563
    dw 2564,2565,2566,2567,2568,2569,2570,2571,2572,2573,2574,2575,2576,2577
    dw 2578,2579,2580,2581,2582,2583,2880,2881,2882,2883,2884,2885,2886,2887
    dw 2888,2889,2890,2891,2892,2893,2894,2895,2896,2897,2898,2899,2900,2901
    dw 2902,2903,3200,3201,3202,3203,3204,3205,3206,3207,3208,3209,3210,3211
    dw 3212,3213,3214,3215,3216,3217,3218,3219,3220,3221,3222,3223,3520,3521
    dw 3522,3523,3524,3525,3526,3527,3528,3529,3530,3531,3532,3533,3534,3535
    dw 3536,3537,3538,3539,3540,3541,3542,3543,3840,3841,3842,3843,3844,3845
    dw 3846,3847,3848,3849,3850,3851,3852,3853,3854,3855,3856,3857,3858,3859
    dw 3860,3861,3862,3863,4160,4161,4162,4163,4164,4165,4166,4167,4168,4169
    dw 4170,4171,4172,4173,4174,4175,4176,4177,4178,4179,4180,4181,4182,4183
    dw 4480,4481,4482,4483,4484,4485,4486,4487,4488,4489,4490,4491,4492,4493
    dw 4494,4495,4496,4497,4498,4499,4500,4501,4502,4503,4800,4801,4802,4803
    dw 4804,4805,4806,4807,4808,4809,4810,4811,4812,4813,4814,4815,4816,4817
    dw 4818,4819,4820,4821,4822,4823,5120,5121,5122,5123,5124,5125,5126,5127
    dw 5128,5129,5130,5131,5132,5133,5134,5135,5136,5137,5138,5139,5140,5141
    dw 5142,5143,5440,5441,5442,5443,5444,5445,5446,5447,5448,5449,5450,5451
    dw 5452,5453,5454,5455,5456,5457,5458,5459,5460,5461,5462,5463,5760,5761
    dw 5762,5763,5764,5765,5766,5767,5768,5769,5770,5771,5772,5773,5774,5775
    dw 5776,5777,5778,5779,5780,5781,5782,5783,6080,6081,6082,6083,6084,6085
    dw 6086,6087,6088,6089,6090,6091,6092,6093,6094,6095,6096,6097,6098,6099
    dw 6100,6101,6102,6103,6400,6401,6402,6403,6404,6405,6406,6407,6408,6409
    dw 6410,6411,6412,6413,6414,6415,6416,6417,6418,6419,6420,6421,6422,6423
    dw 6720,6721,6722,6723,6724,6725,6726,6727,6728,6729,6730,6731,6732,6733
    dw 6734,6735,6736,6737,6738,6739,6740,6741,6742,6743,7040,7041,7042,7043
    dw 7044,7045,7046,7047,7048,7049,7050,7051,7052,7053,7054,7055,7056,7057
    dw 7058,7059,7060,7061,7062,7063,7360,7361,7362,7363,7364,7365,7366,7367
    dw 7368,7369,7370,7371,7372,7373,7374,7375,7376,7377,7378,7379,7380,7381
    dw 7382,7383


	END


