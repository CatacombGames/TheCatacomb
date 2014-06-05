/* The Catacomb Source Code
 * Copyright (C) 1993-2014 Flat Rock Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define CATALOG


#include "pcrlib.h"
#include <conio.h>

char	ch,str[80];	// scratch space

/*========================================================================*/

inputtype playermode[3] = {0,keyboard,joystick1};

boolean		keydown[128];

int JoyXlow [3], JoyXhigh [3], JoyYlow [3], JoyYhigh [3];

int MouseSensitivity;

char key[8],keyB1,keyB2;

void interrupt (*oldint9) ()=NULL;

char	demobuffer[5000];
char	*demoptr;
int	democount;
int	lastdemoval;		// so demo can be RLE compressed
enum demoenum indemo;

/*=======================================================================*/

/*
=======================
=
= SetupKBD
= Clears the keydown array and installs the INT 9 ISR if it isn't allready
= hooked up.
=
=======================
*/

void SetupKBD ()
{
 void far *vect = getvect (9);
 int i;

 for (i=0;i<128;i++)			/* clear our key down table */
   keydown[i]= false;

 poke (0x40,0x1c,peek(0x40,0x1a));	/* clear the bios key buffer */

 if ( &Int9ISR != vect ) 		/* is our handler allready set up? */
 {
   oldint9 = vect;
   setvect (9,Int9ISR);
 }
}


/*
=========================
=
= Int9ISR
= Called for every keypress.  Keeps track of which keys are down, and passes
= the key on to DOS after clearing the dos buffer (max 1 char in buffer).
=
=========================
*/

void interrupt Int9ISR ()
{
 int key = inportb (0x60);		/* get the key pressed */

 if (key>127)
   keydown [key-128] = false;		/* break scan code */
 else
 {
   keydown [key] = true;		/* make scan code */
   poke (0x40,0x1c,peek(0x40,0x1a));	/* clear the bios key buffer */
 }
asm {
   push ax
   push	bx
   push	cx
   push	dx
   push	si
   push	di
   push	bp
 }
 oldint9 ();				/* give it to DOS */
asm {
   pop	bp
   pop  di
   pop	si
   pop	dx
   pop	cx
   pop	bx
   pop	ax
 }
 outport (0x20,0x20);			/* tell the int manager we got it */
}



/*
===========================
=
= ShutdownKBD
= Sets the int 9 vector back to oldint 9
=
===========================
*/

void ShutdownKBD ()
{
 if (oldint9 != NULL)
   setvect (9,oldint9);
}


/*
===========================
=
= ControlKBD
=
===========================
*/

ControlStruct ControlKBD ()
{
 int xmove=0,
     ymove=0;
 ControlStruct action;

 if (keydown [key[north]])
  ymove=-1;
 if (keydown [key[east]])
  xmove=1;
 if (keydown [key[south]])
  ymove=1;
 if (keydown [key[west]])
  xmove=-1;

 if (keydown [key[northeast]])
 {
   ymove=-1;
   xmove=1;
 }
 if (keydown [key[northwest]])
 {
   ymove=-1;
   xmove=-1;
 }
 if (keydown [key[southeast]])
 {
   ymove=1;
   xmove=1;
 }
 if (keydown [key[southwest]])
 {
   ymove=1;
   xmove=-1;
 }

  switch (ymove*3+xmove)
 {
   case -4: action.dir = northwest; break;
   case -3: action.dir = north; break;
   case -2: action.dir = northeast; break;
   case -1: action.dir = west; break;
   case  0: action.dir = nodir; break;
   case  1: action.dir = east; break;
   case  2: action.dir = southwest; break;
   case  3: action.dir = south; break;
   case  4: action.dir = southeast; break;
 }

 action.button1 = keydown [keyB1];
 action.button2 = keydown [keyB2];

 return (action);
}


/*
============================
=
= ControlMouse
=
============================
*/

ControlStruct ControlMouse ()
{
 int newx,newy,		/* mickeys the mouse has moved */
     xmove = 0,
     ymove = 0;
 ControlStruct action;

 _AX = 3;
 geninterrupt (0x33);		/* mouse status */
 newx = _CX;
 newy = _DX;
 action.button1 = _BX & 1;
 action.button2 = (_BX & 2) >> 1;

 if ((newx-320)/2>MouseSensitivity)
 {
   xmove = 1;
   newx = newx - MouseSensitivity*2;
 }
 else if ((newx-320)/2<-MouseSensitivity)
 {
   xmove = -1;
   newx = newx + MouseSensitivity*2;
 }
 if ((newy-100)>MouseSensitivity)
 {
   ymove = 1;
   newy = newy - MouseSensitivity;
 }
 else if ((newy-100)<-MouseSensitivity)
 {
   ymove = -1;
   newy = newy + MouseSensitivity;
 }

  _AX = 4;
  _CX=newx;
  _DX=newy;
  geninterrupt (0x33);		/* set mouse status */

 switch (ymove*3+xmove)
 {
   case -4: action.dir = northwest; break;
   case -3: action.dir = north; break;
   case -2: action.dir = northeast; break;
   case -1: action.dir = west; break;
   case  0: action.dir = nodir; break;
   case  1: action.dir = east; break;
   case  2: action.dir = southwest; break;
   case  3: action.dir = south; break;
   case  4: action.dir = southeast; break;
 }

 return (action);
}


/*
===============================
=
= ReadJoystick
= Just return the resistance count of the joystick
=
===============================
*/

void ReadJoystick (int joynum,int *xcount,int *ycount)
{
 int portval,a1,a2,xbit,ybit;

 if (joynum==1)
 {
  xbit=1;
  ybit=2;
 }
 else
 {
  xbit=4;
  ybit=8;
 }

 *xcount = 0;
 *ycount = 0;

 outportb (0x201,inportb (0x201));	/* start the signal pulse */

 asm cli;

 do
 {
   portval = inportb (0x201);
   a1 = (portval & xbit) != 0;
   a2 = (portval & ybit) != 0;
   *xcount+=a1;
   *ycount+=a2;
 } while ((a1+a2!=0) && (*xcount<500) && (*ycount<500));

 asm sti;
}


/*
=============================
=
= ControlJoystick (joy# = 1 / 2)
=
=============================
*/

ControlStruct ControlJoystick (int joynum)
{
 int joyx = 0,joyy = 0,		/* resistance in joystick */
     xmove = 0,
     ymove = 0,
     buttons;
 ControlStruct action;

 ReadJoystick (joynum,&joyx,&joyy);
 if ( (joyx>500) | (joyy>500) )
 {
   joyx=JoyXlow [joynum] + 1;	/* no joystick connected, do nothing */
   joyy=JoyYlow [joynum] + 1;
 }

 if (joyx > JoyXhigh [joynum])
   xmove = 1;
 else if (joyx < JoyXlow [joynum])
   xmove = -1;
 if (joyy > JoyYhigh [joynum])
   ymove = 1;
 else if (joyy < JoyYlow [joynum])
   ymove = -1;

 switch (ymove*3+xmove)
 {
   case -4: action.dir = northwest; break;
   case -3: action.dir = north; break;
   case -2: action.dir = northeast; break;
   case -1: action.dir = west; break;
   case  0: action.dir = nodir; break;
   case  1: action.dir = east; break;
   case  2: action.dir = southwest; break;
   case  3: action.dir = south; break;
   case  4: action.dir = southeast; break;
 }

 buttons = inportb (0x201);	/* get all four button status */
 if (joynum == 1)
 {
   action.button1 = ((buttons & 0x10) == 0);
   action.button2 = ((buttons & 0x20) == 0);
 }
 else
 {
   action.button1 = ((buttons & 0x40) == 0);
   action.button2 = ((buttons & 0x80) == 0);
 }
 return (action);
}


/*
=============================
=
= ControlPlayer
=
= Expects a 1 or a 2
=
=============================
*/

ControlStruct ControlPlayer (int player)
{
 ControlStruct ret;
 int val;

 if (indemo == notdemo || indemo == recording)
 {
   switch (playermode[player])
   {
     case keyboard : ret = ControlKBD (); break;
     case mouse    : ret = ControlMouse (); break;
     case joystick1: ret = ControlJoystick(1); break;
     case joystick2: ret = ControlJoystick(2); break;
   }

   //
   // recording a demo?
   //
   if (indemo == recording)
   {
     val = (ret.dir << 2) | (ret.button2 << 1) | ret.button1;
     *demoptr++=val;
   }


 }

 else

 //
 // get the command from the demo buffer
 //
 {
   val = *demoptr++;

   ret.button1 = val & 1;
   ret.button2 = (val & 2) >> 1;
   ret.dir = (dirtype) ( (val & (4+8+16+32) ) >> 2);
 }

 return (ret);
}


////////////////////////
//
// RecordDemo
// Clears the demo buffer and starts capturing events
//
////////////////////////

void RecordDemo (void)
{
  demobuffer[0]=level;
  demoptr = &demobuffer[1];
  indemo = recording;
}


////////////////////////
//
// LoadDemo / SaveDemo
// Loads a demo from disk or
// saves the accumulated demo command string to disk
//
////////////////////////

void LoadDemo (int demonum)
{
  char st2[5];

  strcpy (str,"DEMO");
  itoa (demonum,st2,10);
  strcat (str,st2);
  strcat (str,".");
  strcat (str,_extension);

  LoadFile (str,MK_FP(_DS,&demobuffer));
  level=demobuffer[0];
  demoptr = &demobuffer[1];
  indemo = demoplay;
}

void SaveDemo (int demonum)
{
  char st2[5];

  strcpy (str,"DEMO");
  itoa (demonum,st2,10);
  strcat (str,st2);
  strcat (str,".");
  strcat (str,_extension);

  SaveFile (str,MK_FP(_DS,&demobuffer),(demoptr-&demobuffer[0]));
  indemo = notdemo;
}


////////////////////////
//
// StartDemo
//
////////////////////////

/*=========================================================================*/


/*
** Miscellanious library routines
*/


///////////////////////////////
//
// clearkeys
// Clears out the bios buffer and zeros out the keydown array
//
///////////////////////////////

void clearkeys (void)
{
  int i;
  while (bioskey (1))
    bioskey(0);

  for (i=0;i<128;i++)
    keydown [i]=0;
}

/*
===========================================
=
= Allocate a block aligned on a paragraph
=
===========================================
*/

void far *lastparalloc;	// global variable of the EXACT (not paralign)
				// last block, so it can be freed right

void huge *paralloc (long size)
{
 void huge *temp;
 word seg,ofs;
/* allocate a block with extra space */
 lastparalloc = (void far*)temp = farmalloc (size+15);
 if (temp == NULL)
 //
 // not enough memory!
 //
 {
   setscreenmode (text);
	_quit ("Out of memory!");
 }

 ofs=FP_OFF(temp);
 if (ofs!=0)			/* set offset to 0 and bump segment */
 {
  seg=FP_SEG(temp);
  seg++;
  ofs=0;
  temp=MK_FP (seg,ofs);
 }
 return (void huge *) temp;
}

//==========================================================================

/*
==============================================
=
= Load a *LARGE* file into a FAR buffer!
= by John Romero (C) 1990 PCRcade
=
==============================================
*/

unsigned long LoadFile(char *filename,char huge *buffer)
{
 unsigned int handle,flength1=0,flength2=0,buf1,buf2,foff1,foff2;

 buf1=FP_OFF(buffer);
 buf2=FP_SEG(buffer);

asm		mov	WORD PTR foff1,0  	// file offset = 0 (start)
asm		mov	WORD PTR foff2,0

asm		mov	dx,filename
asm		mov	ax,3d00h		// OPEN w/handle (read only)
asm		int	21h
asm		jc	out

asm		mov	handle,ax
asm		mov	bx,ax
asm		xor	cx,cx
asm		xor	dx,dx
asm		mov	ax,4202h
asm		int	21h			// SEEK (find file length)
asm		jc	out

asm		mov	flength1,ax
asm		mov	flength2,dx

asm		mov	cx,flength2
asm		inc	cx			// <- at least once!

L_1:

asm		push	cx

asm		mov	cx,foff2
asm		mov	dx,foff1
asm		mov	ax,4200h
asm		int	21h			// SEEK from start

asm		push	ds
asm		mov	bx,handle
asm		mov	cx,-1
asm		mov	dx,buf1
asm		mov	ax,buf2
asm		mov	ds,ax
asm		mov	ah,3fh			// READ w/handle
asm		int	21h
asm		pop	ds

asm		pop	cx
asm		jc	out
asm		cmp	ax,-1
asm		jne	out

asm		push	cx			// need to read the last byte
asm		push	ds			// into the segment! IMPORTANT!
asm		mov	bx,handle
asm		mov	cx,1
asm		mov	dx,buf1
asm		add	dx,-1
asm		mov	ax,buf2
asm		mov	ds,ax
asm		mov	ah,3fh
asm		int	21h
asm		pop	ds
asm		pop	cx

asm		add	buf2,1000h
asm		inc	WORD PTR foff2
asm		loop	L_1

out:

asm		mov	bx,handle		// CLOSE w/handle
asm		mov	ah,3eh
asm		int	21h

return (flength2*0x10000+flength1);

}

//===========================================================================

/*
==============================================
=
= Save a *LARGE* file far a FAR buffer!
= by John Romero (C) 1990 PCRcade
=
==============================================
*/

void SaveFile(char *filename,char huge *buffer, long size)
{
 unsigned int handle,buf1,buf2,foff1,foff2;

 buf1=FP_OFF(buffer);
 buf2=FP_SEG(buffer);

asm		mov	WORD PTR foff1,0  		// file offset = 0 (start)
asm		mov	WORD PTR foff2,0

asm		mov	dx,filename
asm		mov	ax,3c00h		// CREATE w/handle (read only)
asm		xor	cx,cx
asm		int	21h
asm		jc	out

asm		mov	handle,ax
asm		cmp	word ptr size+2,0	// larger than 1 segment?
asm		je	L2

L1:

asm		push	ds
asm		mov	bx,handle
asm		mov	cx,8000h
asm		mov	dx,buf1
asm		mov	ax,buf2
asm		mov	ds,ax
asm		mov	ah,40h			// WRITE w/handle
asm		int	21h
asm		pop	ds

asm		add	buf2,800h		// bump ptr up 1/2 segment
asm		sub	WORD PTR size,8000h		// done yet?
asm		sbb	WORD PTR size+2,0
asm		cmp	WORD PTR size+2,0
asm		ja	L1
asm		cmp	WORD PTR size,8000h
asm		jae	L1

L2:

asm		push	ds
asm		mov	bx,handle
asm		mov	cx,WORD PTR size
asm		mov	dx,buf1
asm		mov	ax,buf2
asm		mov	ds,ax
asm		mov	ah,40h			// WRITE w/handle
asm		int	21h
asm		pop	ds
asm		jmp	out

out:

asm		mov	bx,handle		// CLOSE w/handle
asm		mov	ah,3eh
asm		int	21h

}

//==========================================================================


/*
====================================
=
= bloadin
= Paraligns just enough space and bloads in the
= specified file, returning a pointer to the start
=
====================================
*/

void huge *bloadin (char *filename)
{
 int handle;
 long length;
 char huge *location;

 if ( (handle = open (filename,O_BINARY)) != -1 )
   {
    length = filelength (handle);
    location = paralloc (length);
    close (handle);
    LoadFile (filename,location);
    return location;
   }
 else
   return NULL;
}


/*==================================================================================*/

/*
** Graphic routines
*/

grtype grmode;
cardtype _videocard;

void huge *charptr;		// 8*8 tileset
void huge *tileptr;		// 16*16 tileset
void huge *picptr;		// any size picture set
void huge *spriteptr;		// any size masked and hit rect sprites
void huge *egaspriteptr[4];	// spriteptr for each ega plane's data

unsigned crtcaddr;

int sx,sy,leftedge;

/*
========================
=
= setscreenmode
= Call BIOS to set TEXT / CGAgr / EGAgr / VGAgr
=
========================
*/

void setscreenmode (grtype mode)
{
  char extern VGAPAL;			// deluxepaint vga pallet .OBJ file
  void far *vgapal = &VGAPAL;

  switch (mode)
  {
    case text: _AX = 3;
	       geninterrupt (0x10);
	       screenseg=0xb800;
	       break;
    case CGAgr: _AX = 4;
		geninterrupt (0x10);
		screenseg=0xb800;
		break;
    case EGAgr: _AX = 0xd;
		geninterrupt (0x10);
		screenseg=0xa000;
		EGAmove ();
		moveega ();
		break;
    case VGAgr: _AX = 0x13;
		geninterrupt (0x10);
		screenseg=0xa000;
		_ES = FP_SEG(vgapal);
		_DX = FP_OFF(vgapal);
		_BX = 0;
		_CX = 0x100;
		_AX = 0x1012;
		geninterrupt(0x10);			// set the deluxepaint pallet
		break;
  }
  crtcaddr = 0x3d4;		//peek (0x40,0x63) if not for two monitors...
}


/*
========================
=
= egasplitscreen
=
========================
*/

void egasplitscreen (int linenum)
{
  WaitVBL ();
  if (_videocard==VGAcard)
    linenum*=2;
  outportb (crtcaddr,CRTCLINECOMPARE);
  outportb (crtcaddr+1,linenum % 256);
  outportb (crtcaddr,CRTCOVERFLOW);
  outportb (crtcaddr+1, 1+16*(linenum/256));
  if (_videocard==VGAcard)
  {
    outportb (crtcaddr,CRTCMAXSCANLINE);
    outportb (crtcaddr+1,inportb(crtcaddr+1) & (255-64));
  }
}


/*
========================
=
= crtcstart
=
========================
*/

void crtcstart (unsigned start)
{
  WaitVBL ();
  outportb (crtcaddr,CRTCSTARTL);
  outportb (crtcaddr+1,start % 256);
  outportb (crtcaddr,CRTCSTARTH);
  outportb (crtcaddr+1,start / 256);
}


int win_xl,win_yl,win_xh,win_yh;

int screencenterx = 19,screencentery = 11;

//////////////////////////
//
// drawwindow
// draws a bordered window and homes the cursor
//
//////////////////////////

void drawwindow (int xl, int yl, int xh, int yh)
{
 int x,y;
 win_xl=xl;
 win_yl=yl;
 win_xh=xh;
 win_yh=yh;		// so the window can be erased

 drawchar (xl,yl,1);
 for (x=xl+1;x<xh;x++)
   drawchar (x,yl,2);
 drawchar (xh,yl,3);
 for (y=yl+1;y<yh;y++)
 {
   drawchar (xl,y,4);
   for (x=xl+1;x<xh;x++)
     drawchar (x,y,' ');
   drawchar (xh,y,5);
 }
 drawchar (xl,yh,6);
 for (x=xl+1;x<xh;x++)
   drawchar (x,yh,7);
 drawchar (xh,yh,8);

 sx = leftedge = xl+1;
 sy = yl+1;
}

////////////////////////////
//
// erasewindow
// clears out the last window and it's border to spaces
//
///////////////////////////

void bar (int xl, int yl, int xh, int yh, int ch)
{
  int x,y;

  for (y=yl;y<=yh;y++)
    for (x=xl;x<=xh;x++)
      drawchar (x,y,ch);
}


void erasewindow (void)
{
  bar (win_xl,win_yl,win_xh,win_yh,' ');
}

/////////////////////////////
//
// centerwindow
// Centers a drawwindow of the given size
//
/////////////////////////////

void centerwindow (int width, int height)
{
  int xl = screencenterx-width/2;
  int yl = screencentery-height/2;

  drawwindow (xl,yl,xl+width+1,yl+height+1);
}

///////////////////////////////
//
// expwin {h / v}
// Grows the window outward
//
///////////////////////////////
void expwin (int width, int height)
{
  if (width > 2)
  {
    if (height >2)
      expwin (width-2,height-2);
    else
      expwinh (width-2,height);
  }
  else
    if (height >2)
      expwinv (width,height-2);

  WaitVBL ();
  centerwindow (width,height);
}

void expwinh (int width, int height)
{
  if (width > 2)
    expwinh (width-2,height);

  WaitVBL ();
  centerwindow (width,height);
}

void expwinv (int width, int height)
{
  if (height >2)
    expwinv (width,height-2);

  WaitVBL ();
  centerwindow (width,height);
}


/////////////////////////
//
// get
// Flash a cursor at sx,sy and waits for a user bioskey
//
/////////////////////////

int get (void)
{
 int cycle,key;

 do
 {
   cycle = 9;
   while (!(key = bioskey(1)) && cycle<13)
   {
     drawchar (sx,sy,cycle++);
     WaitVBL ();
     WaitVBL ();
     WaitVBL ();
     WaitVBL ();
     WaitVBL ();
   }
 } while (key == 0);
 drawchar (sx,sy,' ');
 return bioskey(0);		// take it out of the buffer
}


/////////////////////////
//
// print
// Prints a string at sx,sy.  No clipping!!!
//
/////////////////////////

void print (const char *str)
{
  char ch;

  while ((ch=*str++) != 0)
    if (ch == '\n')
    {
      sy++;
      sx=leftedge;
    }
    else if (ch == '\r')
      sx=leftedge;
    else
      drawchar (sx++,sy,ch);
}


///////////////////////////
//
// printint / printlong
// Converts the value to a string and prints it
//
///////////////////////////

void printint (int val)
{
  itoa(val,str,10);
  print (str);
}

void printlong (long val)
{
  ltoa(val,str,10);
  print (str);
}

/*========================================================================*/

int _MouseStatus;

////////////////////////////////////////////////////////////////////
//
// Mouse Routines
//
////////////////////////////////////////////////////////////////////
int _MouseInit(void)
{
 union REGS regs;
 unsigned char far *vector;

 if ((vector=MK_FP(peek(0,0x33*4+2),peek(0,0x33*4)))==NULL) return 0;

 if (*vector == 207)
   return _MouseStatus = 0;

 _AX=0;
 geninterrupt(0x33);
 return _MouseStatus = 1;
}



void _MouseHide(void)
{
 if (!_MouseStatus) return;

 _AX=2;
 geninterrupt(0x33);
}



void _MouseShow(void)
{
 if (!_MouseStatus) return;

 _AX=1;
 geninterrupt(0x33);
}



int _MouseButton(void)
{
 union REGS regs;

 if (!_MouseStatus) return 0;

 regs.x.ax=3;
 int86(0x33,&regs,&regs);
 return(regs.x.bx);
}



void _MouseCoords(int *x,int *y)
{
 union REGS regs;

 if (!_MouseStatus) return;

 regs.x.ax=3;
 int86(0x33,&regs,&regs);
 *x=regs.x.cx;
 *y=regs.x.dx;

 *x/=2;
}




////////////////////////////////////////////////////////////////////
//
// Verify a file's existence
//
////////////////////////////////////////////////////////////////////
long _Verify(char *filename)
{
 int handle;
 long size;

 if ((handle=open(filename,O_BINARY))==-1) return 0;
 size=filelength(handle);
 close(handle);
 return size;
}





////////////////////////////////////////////////////////////////////
//
// print hex byte
//
////////////////////////////////////////////////////////////////////
void _printhexb(unsigned char value)
{
 int loop;
 char hexstr[16]="0123456789ABCDEF",str[2]="";

 for (loop=0;loop<2;loop++)
   {
    str[0]=hexstr[(value>>(1-loop)*4)&15];
    print(str);
   }
}




////////////////////////////////////////////////////////////////////
//
// print hex
//
////////////////////////////////////////////////////////////////////
void _printhex(unsigned value)
{
 print("$");
 _printhexb(value>>8);
 _printhexb(value&0xff);
}




////////////////////////////////////////////////////////////////////
//
// print bin
//
////////////////////////////////////////////////////////////////////
void _printbin(unsigned value)
{
 int loop;

 print("%");
 for (loop=0;loop<16;loop++)
    if ((value>>15-loop)&1) print("1"); else print("0");
}




////////////////////////////////////////////////////////////////////
//
// center print
//
////////////////////////////////////////////////////////////////////
void _printc(char *string)
{
 sx=1+screencenterx-(strlen(string)/2);
 print(string);
}




////////////////////////////////////////////////////////////////////
//
// input unsigned
//
////////////////////////////////////////////////////////////////////
unsigned _inputint(void)
{
 char string[18]="",digit,hexstr[16]="0123456789ABCDEF";
 unsigned value,loop,loop1;

 _input(string,17);
 if (string[0]=='$')
   {
    int digits;

    digits=strlen(string)-2;
    if (digits<0) return 0;

    for (value=0,loop1=0;loop1<=digits;loop1++)
      {
       digit=toupper(string[loop1+1]);
       for (loop=0;loop<16;loop++)
	  if (digit==hexstr[loop])
	    {
	     value|=(loop<<(digits-loop1)*4);
	     break;
	    }
      }
   }
 else if (string[0]=='%')
   {
    int digits;

    digits=strlen(string)-2;
    if (digits<0) return 0;

    for (value=0,loop1=0;loop1<=digits;loop1++)
      {
       if (string[loop1+1]<'0' || string[loop1+1]>'1') return 0;
       value|=(string[loop1+1]-'0')<<(digits-loop1);
      }
   }
 else value=atoi(string);
 return value;
}




////////////////////////////////////////////////////////////////////
//
// line input routine
//
////////////////////////////////////////////////////////////////////
int _input(char *string,int max)
{
 char key;
 int count=0,loop;

 do {
     key=toupper(get()&0xff);
     if ((key==127 || key==8)&&count>0)
       {
	count--;
	drawchar(sx,sy,' ');
	sx--;
       }

     if (key>=' ' && key<='z' && count<max)
       {
	*(string+count++)=key;
	drawchar(sx++,sy,key);
       }

    } while (key!=27 && key!=13);

 for (loop=count;loop<max;loop++) *(string+loop)=0;

 if (key==13) return 1;
 return 0;
}

/*========================================================================*/

/*
** Game routines
*/

struct scores scoreswap, highscores[5];

long score;
int level;
int _numlevels, _maxplayers;

char *_extension = "PCR";
boolean	_cgaok, _egaok, _vgaok;

////////////////////////
//
// _loadctrls
// Tries to load the control panel settings
// creates a default if not present
//
////////////////////////

void _loadctrls (void)
{
  int handle;

  strcpy (str,"CTLPANEL.");
  strcat (str,_extension);
  if ((handle = open(str, O_RDONLY | O_BINARY, S_IWRITE | S_IREAD)) == -1)
  //
  // set up default control panel settings
  //
  {
    grmode=VGAgr;
    soundmode=spkr;
    playermode[1] = keyboard;
    playermode[2] = joystick1;

    JoyXlow [1] = JoyXlow [2] = 20;
    JoyXhigh[1] = JoyXhigh[2] = 60;
    JoyYlow [1] = JoyYlow [2] = 20;
    JoyYhigh[1] = JoyYhigh[2] = 60;
    MouseSensitivity = 5;

    key[north] = 0x48;
    key[northeast] = 0x49;
    key[east] = 0x4d;
    key[southeast] = 0x51;
    key[south] = 0x50;
    key[southwest] = 0x4f;
    key[west] = 0x4b;
    key[northwest] = 0x47;
    keyB1 = 0x1d;
    keyB2 = 0x38;
  }
  else
  {
    read(handle, &grmode, sizeof(grmode));
    read(handle, &soundmode, sizeof(soundmode));
    read(handle, &playermode, sizeof(playermode));
    read(handle, &JoyXlow, sizeof(JoyXlow));
    read(handle, &JoyYlow, sizeof(JoyYlow));
    read(handle, &JoyXhigh, sizeof(JoyXhigh));
    read(handle, &JoyYhigh, sizeof(JoyYhigh));
    read(handle, &MouseSensitivity, sizeof(MouseSensitivity));
    read(handle, &key, sizeof(key));
    read(handle, &keyB1, sizeof(keyB1));
    read(handle, &keyB2, sizeof(keyB2));

    close(handle);
  }
}

void _savectrls (void)
{
  int handle;

  strcpy (str,"CTLPANEL.");
  strcat (str,_extension);

  if ((handle = open(str, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE)) == -1)
    return;

  write(handle, &grmode, sizeof(grmode));
  write(handle, &soundmode, sizeof(soundmode));
  write(handle, &playermode, sizeof(playermode));
  write(handle, &JoyXlow, sizeof(JoyXlow));
  write(handle, &JoyYlow, sizeof(JoyYlow));
  write(handle, &JoyXhigh, sizeof(JoyXhigh));
  write(handle, &JoyYhigh, sizeof(JoyYhigh));
  write(handle, &MouseSensitivity, sizeof(MouseSensitivity));
  write(handle, &key, sizeof(key));
  write(handle, &keyB1, sizeof(keyB1));
  write(handle, &keyB2, sizeof(keyB2));

  close(handle);
}


////////////////////////
//
// loadhighscores
// Tries to load the score file
// creates a default if not present
//
////////////////////////
void _loadhighscores (void)
{
  int i;

  strcpy (str,"SCORES.");
  strcat (str,_extension);
  if (LoadFile(str,(char huge *)highscores) == 0 )
    for (i=0;i<5;i++)
    {
      highscores[i].score = 100;
      highscores[i].level = 1;
      strcpy(highscores[i].initials,"PCR");
    }
}

void _savehighscores (void)
{
  strcpy (str,"SCORES.");
  strcat (str,_extension);
  SaveFile(str,(char huge *)highscores,sizeof (highscores));
}


////////////////////////
//
// _showhighscores
// Brings up a dialog box with the high score lists and returns immediately
//
////////////////////////
void _showhighscores (void)
{
  int i;
  long h;
  char st2[10];

  centerwindow (17,17);
  print ("\n   HIGH SCORES\n\n");
  print (" #  SCORE LV  BY\n");
  print (" - ------ -- ---\n");
  for (i=0;i<5;i++)
  {
    sx++;
    drawchar (sx,sy,'1'+i);
    sx+=2;
    h=highscores[i].score;
    if (h<100000l)
      sx++;
    if (h<10000l)
      sx++;
    if (h<1000l)
      sx++;
    if (h<100l)
      sx++;
    if (h<10l)
      sx++;
    ltoa(h,str,10);
    print (str);
    sx++;
    if (highscores[i].level<10)
      sx++;
    itoa(highscores[i].level,str,10);
    print (str);
    sx++;
    print (highscores[i].initials);
    print ("\n\n");
  }
  strcpy (str,"SCORE:");
  ltoa (score,st2,10);
  strcat (str,st2);

  _printc (str);
}


//////////////////////////
//
// _checkhighscore
// Compares score to highscores, and inserts place if needed.
// calls showhighscores in any case
//
//////////////////////////
void _checkhighscore (void)
{
  int i,j,k;

  for (i=0;i<5;i++)
    if (score>highscores[i].score)
    {
      for (j=4;i<j;j--)
      {
	k=j-1;
	highscores[j] = highscores[k];
      }
      highscores[i].score = score;
      highscores[i].level = level;
      strcpy(highscores[i].initials,"   ");
      break;
    }

  _showhighscores ();

  //
  // did get a high score
  //
  if (i<5)
  {
    PlaySound (16);
    clearkeys ();
    sx = screencenterx-17/2+14;
    sy = screencentery-17/2+6+i*2;
    j=0;
    do
    {
      ch = k = get();
      if (ch>=' ' && j<3)
      {
	drawchar (sx,sy,ch);
	sx++;
	highscores[i].initials[j]=ch;
	j++;
      }
      if (ch==8 || k==19200)
	if (j>0)
	{
	  sx--;
	  j--;
	}
    } while (ch != 13);
  }
}


////////////////////
//
// _setupgame
//
////////////////////

void _setupgame (void)
{
//
// set up game's library routines
//
  _videocard = VideoID ();

  grmode = text;

  if (!(_videocard == VGAcard || _videocard == MCGAcard))
    _vgaok = false;

  if (!(_videocard == EGAcard || _videocard == VGAcard))
    _egaok = false;

  // allways assume CGA compatability for simCGA garbage

  _loadctrls ();

  if (grmode==VGAgr && _vgaok)
    grmode=VGAgr;
  else if (grmode>=EGAgr && _egaok)
    grmode=EGAgr;
  else
    grmode=CGAgr;

  strcpy (str,"SOUNDS.");
  strcat (str,_extension);

  SoundData = (char huge *) bloadin (str);

  StartupSound ();

  SetupKBD ();

  initrndt (1);		// setup random routines
//  initrnd (1);

  _loadhighscores ();

  loadgrfiles ();	// load the graphic files

}


////////////////////
//
// _quit
//
////////////////////

char extern far PIRACY;

void _quit (char *error)
{
  setscreenmode (text);
  if (!(*error))
  {
	 _savehighscores ();
	 _savectrls ();
  }
  else
  {
	puts (error);
	puts("\n");
	puts("\n");
	puts("For techinical assistance with running this software\n");
	puts("    call Softdisk Publishing at 1-318-221-8311\n");
	puts("\n");
	exit(1);
  }

  ShutdownKBD ();	// shut down the interrupt driven stuff if needed
  ShutdownSound ();

#ifndef CATALOG
	_argc = 2;
	_argv[1] = "LAST.SHL";
	_argv[2] = "ENDSCN.SCN";
	_argv[3] = NULL;
	if (execv("LOADSCN.EXE", _argv) == -1)
		_quit("Couldn't find executable LOADSCN.EXE.\n");
#endif



//  movedata (FP_SEG(&PIRACY),FP_OFF(&PIRACY),0xb800,0,4000);

//  clearkeys ();

//  bioskey (0);

  clrscr ();

  exit (0);		// quit to DOS
}


