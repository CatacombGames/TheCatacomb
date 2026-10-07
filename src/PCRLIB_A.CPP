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

//
// C++ translation of PCRLIB_A.ASM, routine by routine. Registers become
// locals of the same name, 16 bit arithmetic is kept where it matters and
// segment:offset accesses go through dos::x86 so video memory behaves like
// the real card (latches, map mask, read map).
//

#include "PCRLIB.H"

#include "dos/x86.hpp"

using namespace dos::x86;

//
//offsets into .SPK file at segment SPKRfile, offset 0
//
//sound records each take up 16 bytes, and start at $10, and continue to $3F0

#define snd_start	0
#define snd_priority	2
#define snd_samples	3
#define snd_name	4

//
// EGA registers
//
// SCindex, SCmapmask, GCindex, GCmode are in PCRLIB.H

//=========================================================================
//
// DATASEG
//
//=========================================================================

static word SPKactive = 0;	// set non zero when started

char huge *SoundData;		// pointer to SPKR file, PARA aligned
soundtype soundmode = spkr;	// 0=nosound, 1=SPKR, 2= adlib...
static void interrupt (*OldInt8)();	// StartupSPK saves here, Shutdown restores
static byte ExtraInts;		// number of PlaySPKR's to a regular int 8
static byte Intcount;		// counter for extraints, call OldInt8 at 0
static word sndspeed;		// timer count speed

static word SndPtr;		// Pointer to frequency of current sound
static byte SndPriority;	// current sound's priority

static word pausesndptr;
static byte pausepriority;
static byte pauseextraints;
static byte pauseintcount;
static word pausespeed;

int _dontplay = 0;		// set to 1 to avoid all interrupt and timer stuff

int xormask = 0;		// to invert characters
int _yshift = 0;		// to shift char lines

unsigned screenseg = 0xa000;	// changes with grmode, page flips, & scrolls

//========
//
// YLOOKUP has the offsets from screen mem of each screen line (multiples of 8)
//
//========

word CGAylookup[200], EGAylookup[256], VGAylookup[200];

static struct ylookupinit
{
  ylookupinit ()
  {
    for (int y=0;y<200;y++)
    {
      CGAylookup[y] = (word)((y&1)*8192 + (y>>1)*80);
      VGAylookup[y] = (word)(y*320);
    }
    for (int y=0;y<256;y++)
      EGAylookup[y] = (word)(y*40);
  }
} ylookupinit_;

// The tables are indexed with byte offsets (BX), sometimes odd ones
static inline word ylookup (const word *table,word bx)
{
  const byte *t = (const byte *)table;
  return (word)(t[bx] | (t[bx+1] << 8));
}

//=======================================================================

void interrupt UpdateSPKR (void);

//========
//
// StartupSound
//
// Sets up the new INT 8 ISR and various internal pointers.
// Assumes that the calling program has pointer SOUNDDATA to something
// meaningful...
//
//========

void StartupSound (void)
{
  if (_dontplay)
    return;

  if (!SPKactive)			// see if library is active
  {
    StopSound ();			// make sure nothing is playing

    disable ();
    OldInt8 = getvect (8);		// call bios to get int 8

    ExtraInts = 1;			// the timer is still going at the
    Intcount = 1;			// normal rate now

    setvect (8,UpdateSPKR);		// call bios to set int 8
    enable ();

    SPKactive++;			// sound routines are now active
  }

  soundmode = spkr;			// set soundmode to SPKR
}

//=======================================================================

//========
//
// ShutdownSound
//
//========

void ShutdownSound (void)
{
  if (_dontplay)
    return;

  outportb (0x43,0x36);		// tell the timer chip we are going to
  outportb (0x40,0);		// change the speed of timer 0
  outportb (0x40,0);		// system expects 0000 for rate

  if (!SPKactive)
    return;			// sound library wasn't started...

  setvect (8,OldInt8);		// call bios to set int 8

  SPKactive = 0;		// sound routines are now inactive

  outportb (0x61,inportb (0x61) & 0xfd);	// turn speaker off
}

//=========================================================================

//===========
//
// PlaySound (soundnum)
//
// If the sound's priority is >= the current priority, SoundPtr, SndPriority,
// and the timer speed are changed
//
//===========

void PlaySound (int playnum)
{
  word si,bx;
  byte bl;
  cseg es;

  if (_dontplay)
    return;

  disable ();

  si = (word)(playnum << 4);	// index into the sound headers
  es = segment (SoundData);	// point es: to the spkr file

  if (rdb (es,si+snd_priority) >= SndPriority)	// current sound has higher priority?
  {
    SndPriority = rdb (es,si+snd_priority);
    SndPtr = rdw (es,si+snd_start);	// offset in .SPK file

    bl = rdb (es,si+snd_samples);	// samples / regular timer tick (1-255)
    ExtraInts = bl;		// an OldInt8 will be called after this
    Intcount = bl;		// many UpdateSPKR times have been called

    if (bl <= 1)		// sample rate of 0 or 1 = 0000 for timer
      bx = 0;
    else
    {
      bx = (word)(0x10000l / bl);	// $10000 / samples = timer rate
      sndspeed = bx;		// save off the timer rate
    }

    outportb (0x43,0x36);	// tell the timer chip we are going to
    outportb (0x40,bx & 0xff);	// change the speed of timer 0
    outportb (0x40,bx >> 8);
  }

  enable ();
}

//======================================================================

//===========
//
// StopSound
//
//===========

void StopSound (void)
{
  if (_dontplay)
    return;

  disable ();

  SndPtr = 0;
  SndPriority = 0;
  sndspeed = 0;

  outportb (0x61,inportb (0x61) & 0xfd);	// turn speaker off

  outportb (0x43,0x36);		// tell the timer chip we are going to
  outportb (0x40,0);		// change the speed of timer 0
  outportb (0x40,0);		// back to normal clock speed

  ExtraInts = 1;		// one bios int / int8
  Intcount = 1;

  enable ();
}

//======================================================================

//===========
//
// PauseSound
//
//===========

void PauseSound (void)
{
  if (_dontplay)
    return;

  disable ();
  pausesndptr = SndPtr;		// save off the current values
  pausepriority = SndPriority;
  pauseextraints = ExtraInts;
  pauseintcount = Intcount;
  pausespeed = sndspeed;
  StopSound ();
  enable ();
}

//======================================================================

//===========
//
// ContinueSound
//
//===========

void ContinueSound (void)
{
  word bx;

  if (_dontplay)
    return;

  disable ();
  SndPtr = pausesndptr;		// restore the old values
  SndPriority = pausepriority;
  ExtraInts = pauseextraints;
  Intcount = pauseintcount;

  bx = pausespeed;

  outportb (0x43,0x36);		// tell the timer chip we are going to
  outportb (0x40,bx & 0xff);	// change the speed of timer 0
  outportb (0x40,bx >> 8);
  enable ();
}

//======================================================================

//========
//
// WaitendSound
// Just waits around until the current sound stops playing
//
//========

void WaitEndSound (void)
{
  if (_dontplay)
    return;

  disable ();
  UpdateSPKR ();		// in case a sound was just started and hasn't
  enable ();			// been hit by an INT yet

  for (;;)			// when the ptr is 0, nothing is on
  {
    disable ();
    word ptr = SndPtr;
    enable ();
    if (ptr == 0)
      break;
    dos::idle ();
  }
}

//=========================================================================

//========
//
// UpdateSPKR
// only called by interrupt $8!
//
//=========

void interrupt UpdateSPKR (void)
{
  word si,bx;
  cseg es = segment (SoundData);	// es to sound file

  outportb (0x20,0x20);		// we got the interrupt here

  if (--Intcount == 0)		// see if it is time for a BIOS int 8...
  {
    Intcount = ExtraInts;	// reset interrupt counter
    OldInt8 ();			// call the old BIOS timer routine
  }

//
// play the speaker
//
  si = SndPtr;
  if (si == 0)
    goto nosound;		// nothing playing

  bx = rdw (es,si);
  SndPtr += 2;

  if (bx == 0)
    goto nosound;		// a zero frequency is no sound, but don't stop

  if (bx == 0xffff)		// a -1 frequency is end of sound
  {
    StopSound ();
    return;
  }

  if (!(dos::ld (soundmode) & 0xff))	// if soundon=0, don't play anything
    goto nosound;

  outportb (0x43,0xb6);		// write to channel 2 (speaker) timer
  outportb (0x42,bx & 0xff);	// low byte
  outportb (0x42,bx >> 8);	// high byte

  outportb (0x61,inportb (0x61) | 3);	// turn speaker on to timer
  return;

nosound:
  outportb (0x61,inportb (0x61) & 0xfc);	// turn speaker off
}

//==========================================================================

//========================================================================
//
// Random number generators
//
// random() indexes its variables with 16 bit offsets that walk out of
// RndArray (indexi/indexj are never initialised because initrnd isn't
// called), reading and writing the variables in front of it -- rndtable
// among them, which changes the sequence rndt returns after a few demo
// loops. To reproduce that, the variables live in a 64K block laid out like
// the original data segment.
//
//========================================================================

static byte rndseg[0x10000];

enum : word
{
  rndindex = 0x8000,
  rndtable = rndindex+2,	// 256 bytes
  indexi = rndtable+256,	// Rnd#Generator
  indexj = indexi+2,
  LastRnd = indexj+2,
  RndArray = LastRnd+2,		// 17 words
  baseRndArray = RndArray+17*2	// 17 words
};

static inline word rndw (word off)
{
  return (word)(rndseg[off] | (rndseg[(word)(off+1)] << 8));
}

static inline void setrndw (word off,word v)
{
  rndseg[off] = (byte)v;
  rndseg[(word)(off+1)] = (byte)(v >> 8);
}

static struct rndseginit
{
  rndseginit ()
  {
    static const byte table[256] = {
      0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66,
     74,  21, 211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36,
     95, 110,  85,  48, 212, 140, 211, 249,  22,  79, 200,  50,  28, 188,
     52, 140, 202, 120,  68, 145,  62,  70, 184, 190,  91, 197, 152, 224,
    149, 104,  25, 178, 252, 182, 202, 182, 141, 197,   4,  81, 181, 242,
    145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0,
    175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,   2, 235,
     25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113,
     94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75,
    136, 156,  11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196,
    135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113,
     80, 250, 108,   7, 255, 237, 129, 226,  79, 107, 112, 166, 103, 241,
     24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224,
    145, 224,  81, 206, 163,  45,  63,  90, 168, 114,  59,  33, 159,  95,
     28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226,
     71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36,
     17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106,
    197, 242,  98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136,
    120, 163, 236, 249};
    static const word base[17] = {1,1,2,3,5,8,13,21,54,75,129,204,
      323,527,850,1377,2227};

    for (int i=0;i<256;i++)
      rndseg[rndtable+i] = table[i];
    for (int i=0;i<17;i++)
      setrndw (baseRndArray+i*2,base[i]);
  }
} rndseginit_;

//=================================================
//
// Init RND generator
// if randomize is false, the counter is set to 0
//
// 11-Sep-90	LR	FIX initialization to use TIME!
//=================================================

void initrnd (boolean randomize)
{
  for (int i=0;i<17;i++)	// set up the table (which is constantly changed)
    setrndw (RndArray+i*2,rndw (baseRndArray+i*2));

  setrndw (LastRnd,0);
  setrndw (indexi,17*2);
  setrndw (indexj,5*2);

  if (randomize)		// if randomize is true, really random
  {
    _AH = 0x2c;
    geninterrupt (0x21);	// GetSystemTime

    setrndw (RndArray+34-2,_DX);
    _DX ^= _CX;			// init w/seconds values
    setrndw (RndArray+10-2,_DX);
  }

  random (0xffff);		// warm up generator!
}

//=================================================
//
// Return a random # between 0-?
// Exit : AX = 0-max value
//
// 11-Sep-90 LR -modify to save registers!
//=================================================

int random (int maxval)
{
  word ax,bx,cx,dx,si;
  bool carry;

  ax = (word)maxval;
  if (ax == 0)
    return 0;			// (the original never returns)

//
// create a mask to cut down on the # of SUBTRACTS!
//
  dx = 0xffff;			// full-mask
  for (;;)
  {
    carry = (ax & 0x8000) != 0;
    ax <<= 1;
    if (carry)
      break;
    dx >>= 1;
  }

  bx = rndw (indexi);		// this routine was converted from
  si = rndw (indexj);		// the Random macro on Merlin GS
  ax = rndw ((word)(RndArray-2+bx));
  ax = (word)(ax + rndw ((word)(RndArray-2+si)) + carry);	// adc
  setrndw ((word)(RndArray-2+bx),ax);
  ax = (word)(ax + rndw (LastRnd));
  setrndw (LastRnd,ax);
  bx -= 2;
  if (bx == 0)
    bx = 17*2;
  si -= 2;
  if (si == 0)
    si = 17*2;
  setrndw (indexi,bx);
  setrndw (indexj,si);

  cx = (word)maxval;		// loop -- returns value in range
  ax &= dx;			// AND our mask!
  if (ax > cx)			// SUBTRACT to be within range
    ax >>= 1;

  return ax;
}

//===========================================================================

//=================================================
//
// Init table based RND generator
// if randomize is false, the counter is set to 0
//
//=================================================

void initrndt (boolean randomize)
{
  (void)randomize;		// jne @@timeit is commented out:
  setrndw (rndindex,0);		// always set to a definate value
}

//=================================================
//
// Return a random # between 0-255
// Exit : AX = value
//
//=================================================

int rndt (void)
{
  word bx = rndw (rndindex);
  bx = (bx + 1) & 0xff;
  setrndw (rndindex,bx);
  return rndseg[(word)(rndtable+bx)];
}

//===========================================================================

//========
//
// WAITVBL
//
//========

void WaitVBL (void)
{
  unsigned dx = crtcaddr + 6;

  while (inportb (dx) & 8)	// look for vbl
    ;

  while (!(inportb (dx) & 8))	// look for vbl
    ;
}

//=======================================================================

//====================
//
// EGAplane
// Sets read/write mode 0 and selects the given plane (0-3)
// for reading and writing
//
//====================

void EGAplane (int plane)
{
  outport (GCindex,GCmode);		// set read / write mode 0

  outport (GCindex,4 | ((plane & 0xff) << 8));	// read from this plane number
  outport (SCindex,SCmapmask | ((1 << (plane & 0xff)) << 8));	// write to this plane only
}

//=======================================================================

//====================
//
// EGAlatch
// Sets write mode 1 with all planes selected
//
//====================

void EGAlatch (void)
{
  outport (GCindex,1*256 + GCmode);	// set read 0 / write mode 1

  outport (SCindex,15*256 + SCmapmask);	// write to all planes
}

//=======================================================================

static void CgaCharOut (word si,word di,word bx);
static void EgaCharOut (word si,word di,word bx);
static void VgaCharOut (word si,word di,word bx);

//============
//
// drawchar
// Calls CGAcharout / EGAcharout / VGAcharout
// based on grmode
//
//============

void drawchar (int xcoordinate, int ycoordinate, int ch)
{
  word si = (word)ch;		// low level routines get stuff in registers
  word di = (word)xcoordinate;
  word bx = (word)((ycoordinate << 4) + _yshift);	// allow printing on non-char lines

  if (grmode == CGAgr)
    CgaCharOut (si,di,bx);
  else if (grmode == EGAgr)
    EgaCharOut (si,di,bx);
  else if (grmode == VGAgr)
    VgaCharOut (si,di,bx);
}

//===========================================================================

//============
//
// CGAcharout
// SI= character; DI= xcoordinate; BX= ycoordinate*2
//
//============

static void CgaCharOut (word si,word di,word bx)
{
  seg es = segment (MK_FP (screenseg,0));
  cseg ds = segment (charptr);	// segment of tile pictures (PARA'd)
  word ax;

  si <<= 4;			// char * 16 = tile's offset in PICS
  di <<= 1;			// x * 2 + ylookup[y] = screen location
  di += ylookup (CGAylookup,bx);	// BX is pointer into YLOOKUP

  bx = (word)xormask;		// so chars can be inverted

  for (int row=0;row<4;row++)
  {
    ax = lodsw (ds,si);		// load in a row of the tile's picture
    stosw (es,di,ax ^ bx);
    di += 0x1ffe;
    ax = lodsw (ds,si);
    stosw (es,di,ax ^ bx);
    if (row<3)
      di -= 0x1fb2;
  }
}

//=======================================================================

//============
//
// EGAcharout
// SI= character; DI= xcoordinate; BX= ycoordinate
//
//============

static void EgaCharOut (word si,word di,word bx)
{
  seg es = segment (MK_FP (screenseg,0));
  cseg ds = segment (charptr);	// segment of tile pictures (PARA'd)
  byte dl;
  word cx;

  si <<= 3;			// char * 8 = tile's offset in PICS

  di += ylookup (EGAylookup,bx);	// BX is pointer into YLOOKUP

  outport (GCindex,GCmode);	// set read / write mode 0

  bx = si;			// so the planes can be drawn the same
  cx = di;

  dl = (byte)xormask;		// so chars can be inverted

  for (int plane=0;plane<4;plane++)
  {
    si = bx;
    di = cx;

    outport (GCindex,plane*256+4);		// read map select
    outport (SCindex,(1<<plane)*256+SCmapmask);	// write mask

    for (int row=0;row<8;row++)
    {
      stosb (es,di,lodsb (ds,si) ^ dl);
      di += 39;
    }
  }

  outport (SCindex,15*256 + SCmapmask);	// write to all planes
}

//=======================================================================

//============
//
// VGAcharout
// SI= character; DI= xcoordinate; BX= ycoordinate
//
//============

static void VgaCharOut (word si,word di,word bx)
{
  seg es = segment (MK_FP (screenseg,0));
  cseg ds = segment (charptr);	// segment of tile pictures (PARA'd)

  si <<= 6;			// char * 64 = tile's offset in PICS

  di <<= 3;			// x * 8 + ylookup[y] = screen location

  di += ylookup (VGAylookup,bx);	// BX is pointer into YLOOKUP

  bx = (word)xormask;		// so chars can be inverted

  for (int row=0;row<8;row++)
  {
    for (int i=0;i<4;i++)	// load in a row of the tile's picture
      stosw (es,di,lodsw (ds,si) ^ bx);
    if (row<7)
      di += 312;
  }
}

//=======================================================================

static void CGAtileout (cseg ds,seg es,word si,word bx,word di);
static void EGAtileout (cseg ds,seg es,word si,word bx,word di);
static void VGAtileout (cseg ds,seg es,word si,word bx,word di);

//============
//
// drawtile
// Just drops a 16*16 tile onto the screen
// Calls CGAtileout / EGAtileout / VGAtileout
// based on grmode
//
//============

void drawtile (int xcoordinate, int ycoordinate, int tile)
{
  seg es = segment (MK_FP (screenseg,0));
  cseg ds = segment (tileptr);	// segment of pictures (PARA'd)

  word si = (word)tile;
  word di = (word)xcoordinate;	// low level routines get stuff in registers
  word bx = (word)(ycoordinate << 1);

  if (grmode == CGAgr)
    CGAtileout (ds,es,si,bx,di);
  else if (grmode == EGAgr)
    EGAtileout (ds,es,si,bx,di);
  else if (grmode == VGAgr)
    VGAtileout (ds,es,si,bx,di);
}


//=======================
//
// CGAtileout
// SI= tile #, BX=y, DI=x
//
//=======================

static void CGAtileout (cseg ds,seg es,word si,word bx,word di)
{
  si <<= 6;

  bx <<= 1;

  di += ylookup (CGAylookup,bx);	// destination on screen

  for (int row=0;row<8;row++)
  {
    movsw (es,di,ds,si);
    movsw (es,di,ds,si);
    di += 8188;
    movsw (es,di,ds,si);
    movsw (es,di,ds,si);
    if (row<7)
      di -= 8116;
  }
}

//=======================
//
// EGAtileout
// SI= tile #, BX=y, DI=x
//
//=======================

static void EGAtileout (cseg ds,seg es,word si,word bx,word di)
{
  si <<= 5;
  di += ylookup (EGAylookup,bx);	// destination on screen

  outportb (SCindex,SCmapmask);
  outportb (SCindex+1,0xf);

  outportb (GCindex,GCmode);
  outportb (GCindex+1,1);

  for (int row=0;row<16;row++)
  {
    movsb (es,di,ds,si);
    movsb (es,di,ds,si);
    if (row<15)
      di += 38;
  }
}


//=======================
//
// VGAtileout
// SI= tile #, BX=y, DI=x
//
//=======================

static void VGAtileout (cseg ds,seg es,word si,word bx,word di)
{
  // "mov ah,al / xor al,al ;fast multiply by 256" works on AX (grmode),
  // not on SI: the tile number is used unscaled, as in the original.
  di += ylookup (VGAylookup,bx);	// destination on screen

  for (int row=0;row<16;row++)
  {
    for (int i=0;i<8;i++)
      movsw (es,di,ds,si);
    if (row<15)
      di += 304;
  }
}



//=======================================================================

static word picwidth;		// used by all modes
static word picxcoord;
static word picheight;

static void CgaDrawpic (cseg ds,seg es,word si,word di,word bx);
static void EgaDrawpic (cseg ds,seg es,word si,word di,word bx);
static void VgaDrawpic (cseg ds,seg es,word si,word di,word bx);

//============
//
// drawpic
// Just drops a width/depth block onto the screen with no masking
// Calls CGAdrawpic / EGAdrawpic / VGAdrawpic
// based on grmode
//
//============

void drawpic (int xcoordinate, int ycoordinate, int pic)
{
  word si;

  picwidth = (word)pictable[pic].width;
  picheight = (word)pictable[pic].height;
  si = pictable[pic].shapeptr.off;

  seg es = segment (MK_FP (screenseg,0));

  word di = (word)xcoordinate;	// low level routines get stuff in registers
  word bx = (word)(ycoordinate << 1);	// because it will be indexed into a table

  cseg ds = segment (picptr);	// segment of pictures (PARA'd)

  if (grmode == CGAgr)
    CgaDrawpic (ds,es,si,di,bx);
  else if (grmode == EGAgr)
    EgaDrawpic (ds,es,si,di,bx);
  else if (grmode == VGAgr)
    VgaDrawpic (ds,es,si,di,bx);
}

//===========================================================================

//============
//
// CgaDrawpic
// SI= source; DI= xcoordinate; BX= ycoordinate*2
//
//============

static void CgaDrawpic (cseg ds,seg es,word si,word di,word bx)
{
  picxcoord = di >> 2;		// x / 4 + ylookup[y] = screen location

  do
  {
    word cx = picwidth;		// number of words to write
    di = ylookup (CGAylookup,bx);
    bx += 2;
    di += picxcoord;
    do
      movsb (es,di,ds,si);
    while (--cx);
  } while (--picheight);
}



//=======================================================================

//============
//
// EgaDrawpic
// SI= source; DI= xcoordinate; BX= ycoordinate
//
//============

static void EgaDrawpic (cseg ds,seg,word si,word di,word bx)
{
  outport (0x3ce,0x105);	// set write mode 1

  picxcoord = di >> 3;		// x / 8 + ylookup[y] = screen location

  seg es = segment (MK_FP (0xa000,0));	// segment of screen memory

  do
  {
    word cx = picwidth;		// number of bytes to write
    di = ylookup (EGAylookup,bx);
    bx += 2;
    di += picxcoord;
    do
      movsb (es,di,ds,si);
    while (--cx);
  } while (--picheight);
}


//=======================================================================

//============
//
// VgaDrawpic
// SI= source; DI= xcoordinate; BX= ycoordinate
//
//============

static void VgaDrawpic (cseg ds,seg es,word si,word di,word bx)
{
  picxcoord = di;

  do
  {
    word cx = picwidth;		// number of words to write
    di = ylookup (VGAylookup,bx);
    bx += 2;
    di += picxcoord;
    do
      movsb (es,di,ds,si);
    while (--cx);
  } while (--picheight);
}

//===========================================================================

//=========================================================
//
// Name:	VideoID
//
// Function:	Detects the presence of various video subsystems
//
// Subsystem ID values:
// 	 0  = (none)
// 	 1  = MDA
// 	 2  = CGA
// 	 3  = EGA
// 	 4  = MCGA
// 	 5  = VGA
// 	80h = HGC
// 	81h = HGC+
// 	82h = Hercules InColor
//
// Same tests in the same order as the original (FindPS2, FindEGA, FindCGA),
// against the emulated BIOS and CRTC. There is no monochrome adapter.
//
//=========================================================

cardtype VideoID (void)
{
  static const byte DCCtable[13][2] = {	// translate table for INT 10h func 1Ah
    {0,0},{MDAcard,1},{CGAcard,2},{0,0},{EGAcard,3},{EGAcard,1},{0,0},
    {VGAcard,4},{VGAcard,5},{0,0},{MCGAcard,3},{MCGAcard,4},{MCGAcard,5}};

//
// FindPS2: INT 10h function 1Ah returns the display combination code
//
  _AX = 0x1a00;
  geninterrupt (0x10);		// call video BIOS for info
  if (_AL == 0x1a && _BL < 13 && DCCtable[_BL][0])
    return (cardtype)DCCtable[_BL][0];

//
// FindEGA: an EGA BIOS function which doesn't exist on MDA/CGA
//
  _BL = 0x10;			// BL := 10h (return EGA info)
  _AH = 0x12;			// AH := INT 10H function number
  geninterrupt (0x10);		// call EGA BIOS for info
  if (_BL != 0x10)		// if EGA BIOS is present, BL <> 10H
    return EGAcard;

//
// FindCGA: look for the CGA's 6845 CRTC at I/O port 3D4H
//
  outportb (0x3d4,0x0f);	// select 6845 reg 0Fh (Cursor Low)
  byte original = inportb (0x3d5);
  outportb (0x3d5,0x66);	// try to write to 6845
  byte answer = inportb (0x3d5);
  outportb (0x3d5,original);	// restore original value
  if (answer == 0x66)
    return CGAcard;

  return NOcard;
}

//============================================================================

static int tickcount;

void (*timecall)(void);
unsigned timeax,timebx,timecx,timedx,timesi,timedi,timebp,timees;

static void interrupt (*old1c)();
static std::atomic<word> mark;	// set to 1 when the int has occured

static void interrupt intermark (void)
{
  mark++;
  old1c ();
}

//=============
//
// TIMESUB
// Returns the number of iterations of the given subroutine
// are possible in 1/18 second
//
//=============

int timesub (int ticks)
{
  word counter;

  tickcount = ticks;

  old1c = getvect (0x1c);	// call bios to get int 1c
  setvect (0x1c,intermark);	// call bios to set int 1c

  StopSound ();			// make sure the timer is going normal speed
  counter = 0;			// set counter to 0
  mark = 0;			// will be inc to 1 after int 1c

  while (mark == 0)		// wait for the interrupt to go off once
    dos::poll ();

  do
  {
    mark = 0;			// will be inc to 1 after int 1c
    do
    {
      _AX = timeax;
      _BX = timebx;
      _CX = timecx;
      _DX = timedx;
      _SI = timesi;
      _DI = timedi;
      _BP = timebp;
      _ES = timees;
      timecall ();
      counter++;
    } while (mark == 0);	// has the interrupt gone off again?
  } while (--tickcount);

  setvect (0x1c,old1c);		// call bios to set int 1c

  return counter;		// return value
}
