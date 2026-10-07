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

//==========================================================
// RLE compression in assemlby.
// Written by Lane Roath, Copyright (c) 1990 IFD & Softdisk
//
//----------------------------------------------------------
//
//09-Aug-90 0.00	Start coding, converting source C in Dr. Dobb's
//
// C++ translation of RLEASM.ASM. Pointers are flat, so the segment
// normalisation the assembly does every 64K is not needed.
//==========================================================

#include "PCRLIB.H"

#define MinCnt	(3-1)		// min count for repeat
#define MaxCnt	(127+MinCnt)	// max count (128+ = non repeat string)

// We store our memory allocation results here

static byte *RLESeg;		// RLE packed buf (src/dest)
static byte *SrcSeg;		// original source data
static std::uint32_t RLEIdx;	// indexes for read/write bufs
static std::uint32_t SrcIdx;
static std::uint32_t SrcLen;	// length of source & dest bufs
static std::uint32_t RLELen;
static word ByteCnt;		// # of sequential bytes
static byte LastByte;		// last byte found
static word DifCnt;		// # of different bytes

static std::uint32_t SI,DI;	// the registers the routines share

//==========================================================
//
//	Initialize our variables & buffers
//
//==========================================================

static void InitRLE (void)
{
  ByteCnt = 0;
  DifCnt = 1;
  RLELen = 0;
  SI = SrcIdx;			// init Source Index (SrcIdx)
  DI = RLEIdx;			// & dest index (RLEIdx)
}

//==========================================================
//
//	Output a compression string (repeated bytes)
//
//==========================================================

static void OutputRep (void)
{
  RLESeg[DI++] = (byte)(ByteCnt - 2);	// get byte count - 3 (already -1)
  RLESeg[DI++] = LastByte;		// & output byte to repeat
  SrcIdx = SrcIdx + 1 + ByteCnt;	// update source index (length + 1)
  RLELen += 2;				// update RLE length
  ByteCnt = 0;				// we output everything!
}

//==========================================================
//
//	Output a unique string with count byte preceeding
//
//==========================================================

static void OutputDif (void)
{
  RLESeg[DI++] = (byte)(DifCnt + 0x7f);	// & save byte count - 1 with hi bit set
  word cx = DifCnt;			// get # of bytes to copy
  RLELen += (word)(cx + 1);		// update RLE length
  std::uint32_t si = SrcIdx;		// & starting position in source
  while (cx--)				// copy the string
    RLESeg[DI++] = SrcSeg[si++];
  DifCnt = 0;				// we output everything!
  SrcIdx = si;				// save for next output
}

//==========================================================
//
//	EXPAND a RLE compressed file segment
//
//==========================================================

static void Expand (void)
{
  word ax,cx,dx;
  std::uint32_t si,di;

  InitRLE ();			// init our vars & buffers
  si = DI;			// swap SI and DI
  di = SI;
  dx = 1;

  for (;;)
  {
    if (SrcLen < dx)		// subtract out # of chars
      break;			// exit if we are done!
    SrcLen -= dx;

    ax = RLESeg[si++];		// & get code byte
    if (ax >= 0x80)		// repeat code?
    {
      ax -= 0x7f;		// no- string length = code - $80 + 1
      cx = ax;
      dx = ax;
      while (cx--)		// copy string
	SrcSeg[di++] = RLESeg[si++];
    }
    else
    {
      ax += 3;			// # of bytes = code + 3
      cx = ax;
      dx = ax;
      byte b = RLESeg[si++];	// get byte to repeat
      while (cx--)		// & store that many bytes
	SrcSeg[di++] = b;
    }
  }
}

//==========================================================
//
//	COMPRESS data buffer using RLE
//
//==========================================================

static void Compress (void)
{
  const std::uint32_t srcend = SrcIdx + SrcLen;
  // the loop reads up to 3 bytes past the source: read those as 0
  auto src = [srcend] (std::uint32_t i) -> byte { return i < srcend ? SrcSeg[i] : 0; };
  byte al;

  InitRLE ();			// init vars
  LastByte = src (SI);		// get first byte of data & set as last byte

  for (;;)
  {
// @@get:
    if ((SI & 0xffff) >= 0xff00)	// don't bog us down!
      break;			// send any patial strings!

    if (SrcLen == 0)		// subtract out # of chars
      break;			// exit if we are done!
    SrcLen--;

    SI++;			// get next byte
    al = src (SI);
    if (al == LastByte)		// same as last byte?
    {
      ByteCnt++;		// yes- count it
      if (ByteCnt < MinCnt)
	continue;
      if (DifCnt > 1)		// ok, got a rep string...already have dif?
      {
	DifCnt--;
	OutputDif ();		// yes- output that string first
      }
// @@same:
      DifCnt = 0;		// force even if not output!
      if (ByteCnt < MaxCnt)
	continue;		// have we gotten too big?
      OutputRep ();		// yes, save string & continue
      if (al == src (SI+1) && al == src (SI+2))	// another repeat?
	SI++;			// need this for >127 byte strings!
      continue;
    }

// @@dif:
    word cx = ByteCnt;		// any dups yet?
    if (cx)
    {
      if (cx >= MinCnt)		// repeats before now?
	OutputRep ();		// yes- save repeat bytes
      else
      {
	DifCnt += cx;		// add bad repeats to dif count
	ByteCnt = 0;		// & no more dupes!
      }
    }
// @@cnt:
    DifCnt++;			// count byte as different
    LastByte = al;
    if (DifCnt < MaxCnt-MinCnt-1)	// still in range?
      continue;
    OutputDif ();		// no- output string & continue
    if (al == src (SI+1) && al == src (SI+2))	// if repeat follows, specialize!
    {
      SI++;			// why?
      DifCnt++;
    }
  }

// @@Done:
  word cx = ByteCnt;		// any reps left?
  if (cx)
  {
    if (cx >= MinCnt)		// yes- enough to output?
    {
      if (DifCnt >= 1)		// yes, any difs before these?
	OutputDif ();		// yes- output them
      OutputRep ();		// output rep bytes
      return;
    }
    DifCnt += cx;		// update dif count w/small rep
  }
  if (DifCnt)			// anything to output?
    OutputDif ();		// output dif string
}

//====================================================
//
//	C interface to compress file
//
//====================================================

unsigned int RLECompress (char far *source, long sourcelen, char far *dest)
{
  SrcSeg = (byte *)source;	// set source buffer
  SrcIdx = 0;
  RLESeg = (byte *)dest;	// & destination buffer
  RLEIdx = 0;
  SrcLen = (std::uint32_t)sourcelen;	// save length
  Compress ();			// do the compression
  return RLELen;		// return length of compressed file
}

//====================================================
//
//	C interface to expand file
//
//====================================================

void RLEExpand (char far *source, char far *dest, long origlen)
{
  RLESeg = (byte *)source;	// set source buffer
  RLEIdx = 0;
  SrcSeg = (byte *)dest;	// & destination buffer
  SrcIdx = 0;
  SrcLen = (std::uint32_t)origlen;	// save length
  Expand ();			// do the expansion
}
