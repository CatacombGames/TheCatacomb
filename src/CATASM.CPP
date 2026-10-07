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
// C++ translation of CATASM.ASM. The routines work on the game's globals
// (obj, o[], view, background, oldtiles...) through the same layouts the
// assembly used: obj is the 16 byte object record followed by the 16 byte
// class record, view/background are 86 columns wide.
//

// Compiled as part of CATACOMB.CPP (like CAT_PLAY.CPP and OBJECTS.CPP) so
// it sees the game's object types; the assembly accessed them through its
// own copy of the record layout.

#include "dos/x86.hpp"

using namespace dos::x86;

static_assert (sizeof(activeobj)==16 && sizeof(objdesc)==32);

void refresh (void);		// EXTRN refresh:NEAR
void doactive (void);		// EXTRN doactive:NEAR
void doinactive (void);		// EXTRN doinactive:NEAR

//========================================================================

//{=========================================}
//{                                         }
//{ DRAWOBJ                                 }
//{ Draws the object to TILES in the proper }
//{ direction and state.                    }
//{                                         }
//{=========================================}
//
//Procedure DrawObj;
//var
//  objpri,bsize,tilenum:integer;
//Begin
//  tilenum:=obj.firstchar + obj.size * obj.size
//  *((integer(obj.dir) and obj.dirmask) * obj.stages + obj.stage);
//  obj.oldtile:= tilenum;
//  obj.oldx:=obj.x;
//  obj.oldy:=obj.y;
//  objpri:=priority[tilenum];
//  For y:=obj.y to obj.y+obj.size-1 do
//    for x:=obj.x to obj.x+obj.size-1 do
//      Begin
//	if priority[view[y,x]]<=objpri then
//	  view[y,x]:=tilenum;
//	inc(tilenum);
//      end;
//End;
//

static const byte squares[9] = {0,1,4,9,16,25,36,49,64};

// View is 86*86 (87 rows) words. Objects partly outside of it (which
// happens with garbage oldx/oldy) touch the surrounding memory in the
// original; here those cells are just skipped.
static inline int *viewcell (word di)
{
  return di/2 < 87*86 ? &view[0][0] + di/2 : nullptr;
}

void drawobj (void)
{
  byte al,cl,dl,dh;
  word si,di,ax,cx;

  al = (byte)obj.dir;
  al &= obj.dirmask;
  ax = (word)(al * obj.stages);	// mul obj.stages
  al = (byte)(ax + obj.stage);	// add al,obj.stage
  cl = al;

  ax = (word)(squares[obj.size] * cl);

  ax += obj.firstchar;

  si = ax;
  obj.oldtile = (std::int16_t)si;	// SI holds the first tile to put in

  dl = priority[si];		// entire object has same priority
				// priority is saved in DL

  obj.oldy = obj.y;
  obj.oldx = obj.x;
  ax = (word)(obj.y*86 + obj.x);	// calculate origin's offset in VIEW
  di = (word)(ax*2);		// becuase view is WORD width

  ax = obj.size;		// throughout loop

  dh = (byte)ax;		// do this many lines
  do
  {
    cx = ax;			// do this many characters / line
    do
    {
      if (int *cell = viewcell (di))
	if (dl >= priority[(word)*cell])	// check tiles priority level
	  *cell = si;			// don't draw if lower than what's there
      si++;
      di += 2;
    } while (--cx);

    di -= ax;
    di -= ax;
    di += 86*2;			// position destination at start of next line
  } while (--dh);		// any more lines to do?
}

//========================================================================

//{=======================================}
//{                                       }
//{ ERASEOBJ                              }
//{ Erases the current object by copying  }
//{ the background onto the view where the}
//{ object is standing                    }
//{                                       }
//{=======================================}
//
//Procedure EraseObj;
//var
//  objpri,bsize,tilenum:integer;
//Begin
//  tilenum:=obj.oldtile;
//  For y:=obj.oldy to obj.oldy+obj.size-1 do
//    for x:=obj.oldx to obj.oldx+obj.size-1 do
//      Begin
//	if view[y,x]=tilenum then
//	  view[y,x]:=background[y,x];
//	inc(tilenum);
//      end;
//End;
//

void eraseobj (void)
{
  byte dh;
  word si,di,ax,cx;

  si = (word)obj.oldtile;	// only erase chars that match what was
				// drawn by the last drawobj
  ax = (word)(obj.oldy*86 + obj.oldx);	// calculate origin's offset in VIEW
  di = (word)(ax*2);		// becuase view is WORD width

  ax = obj.size;		// throughout loop

  dh = (byte)ax;		// do this many lines
  do
  {
    cx = ax;			// do this many characters / line
    do
    {
      if (int *cell = viewcell (di))
	if ((word)*cell == si)		// don't erase if its not part of the shape
	  *cell = (&background[0][0])[di/2];	// erase it
      si++;
      di += 2;
    } while (--cx);

    di -= ax;
    di -= ax;
    di += 86*2;			// position destination at start of next line
  } while (--dh);		// any more lines to do?
}

//========================================================================

//{====================}
//{                    }
//{ DoAll              }
//{ The main play loop }
//{                    }
//{====================}
//
//Procedure Doall;
//begin
//  Repeat  {until leveldone or playdone}
//    For objecton:=numobj downto 0 do
//      Begin
//	move (o[objecton],obj.active,sizeof(o[objecton]) );
//	if obj.class<>nothing then {class=nothing means it was killed}
//	  Begin
//	    move (ObjDef[obj.class],obj.think,sizeof(objdef[obj.class]) );
//	    if obj.active then
//	      DoActive
//	    else
//	      DoInactive;
//	  end;
//      end;
//   refresh;
//   inc (frameon);
//  until leveldone or playdone;
//end;

void doall (void)
{
  do
  {
    objecton = numobj;
    do
    {
      std::memcpy (&obj,&o[objecton],16);	// copy 16 bytes

      if ((byte)obj.obclass)
      {
	std::memcpy (&obj.think,&objdef[(byte)obj.obclass],16);	// copy second 16 bytes into obj

	if ((byte)obj.active)
	  doactive ();
	else
	  doinactive ();
      }

      if ((byte)leveldone || (byte)playdone)	// check end
	return;

    } while (--objecton >= 0);	// END for

    refresh ();
    frameon++;

  } while (!(byte)leveldone && !(byte)playdone);	// UNTIL
}

//========================================================================


//===========================================================
//
// Move EGA tiles into EGA memory at "EGATileLoc"!
//
//===========================================================

void EGAmove (void)
{
  word si,di,bx;
  byte ah;

  outportb (0x3c4,2);		// turn MapMask register on!

  seg es = segment (MK_FP (0xa400,0));	// ES:DI = just after screen in latch memory
  di = 0;

  // DS:SI = start of tiles! (a NULL pics reads the interrupt table at 0:0)
  cseg ds = pics ? segment (pics) : segment (MK_FP (0,0));
  si = 0;

  for (int cx=2048;cx;cx--)
  {
    ah = 1;			// start at bitplane 0
    bx = di;

    do
    {
      outportb (0x3c5,ah);	// select new bitplane!

      for (int i=0;i<8;i++)
	movsb (es,di,ds,si);

      di = bx;

      ah <<= 1;
    } while (ah & 0xf);		// do all bitplanes in shape

    di += 8;
  }				// do all tiles

  outportb (0x3c5,0xf);		// select all bitplanes
}

//=======================================================================

//=========
//
// TILELOC has the offsets from $B800 of all 24*24 tiles
//
//=========

static word CGAtileloc[24*24], EGAtileloc[24*24];

static struct tilelocinit
{
  tilelocinit ()
  {
    for (int i=0;i<24*24;i++)
    {
      CGAtileloc[i] = (word)((i/24)*320 + (i%24)*2);
      EGAtileloc[i] = (word)((i/24)*320 + (i%24));
    }
  }
} tilelocinit_;

//=========
//
// CGAREFRESH redraws the tiles that have changed in the tiled screen area
//
//=========

void cgarefresh (void)
{
  seg es = segment (MK_FP (0xb800,0));	// start of CGA memory
  cseg ds = segment (pics);		// segment of pics (para aligned)
  int *vw = &view[0][0];
  word ax,bx,bp,dx,si,di;

  ax = (word)(originy*86 + originx);	// View is 86*86 so no clipping
  bp = (word)(ax*2);		// BP will point into VIEW
  dx = (word)(bp + 48);		// when BP=DX, one row has been filled

  bx = 0;
  for (;;)
  {
    ax = (word)vw[bp/2];	// load the current tile
    if (ax != (word)oldtiles[bx/2])	// compare it with the old tile
    {
      oldtiles[bx/2] = ax;	// store the tile back to the oldtiles
      di = CGAtileloc[bx/2];	// set di to screen address
      si = (word)(ax << 4);	// character number * 16 = start of data
      movsw (es,di,ds,si);	// load in a row of the tile's picture
      di += 0x1ffe;
      movsw (es,di,ds,si);
      di -= 0x1fb2;
      movsw (es,di,ds,si);
      di += 0x1ffe;
      movsw (es,di,ds,si);
      di -= 0x1fb2;
      movsw (es,di,ds,si);
      di += 0x1ffe;
      movsw (es,di,ds,si);
      di -= 0x1fb2;
      movsw (es,di,ds,si);
      di += 0x1ffe;
      movsw (es,di,ds,si);
    }
    bx += 2;			// next oldtile
    bp += 2;			// next view tile
    if (bp != dx)		// has an entire row from VIEW been drawn?
      continue;

    if (bx == 24*24*2)		// have all tiles been drawn?
      break;
    bp += 124;			// point it to the start of the next row
    dx += 172;			// point to end of next row
  }
}

//=======================================================================

//=========
//
// EGAREFRESH redraws the tiles that have changed in the tiled screen area
//
//=========

void egarefresh (void)
{
  seg es = segment (MK_FP (0xa000,0));	// start of EGA memory
  int *vw = &view[0][0];
  word ax,bx,bp,dx,si,di;

  outport (0x3ce,0x105);	// set write mode 1

  ax = (word)(originy*86 + originx);	// View is 86*86 so no clipping
  bp = (word)(ax*2);		// BP will point into VIEW
  dx = (word)(bp + 48);		// when BP=DX, one row has been filled

  bx = 0;
  for (;;)
  {
    ax = (word)vw[bp/2];	// load the current tile
    if (ax != (word)oldtiles[bx/2])	// compare it with the old tile
    {
      oldtiles[bx/2] = ax;	// store the tile back to the oldtiles
      di = EGAtileloc[bx/2];	// set di to screen address
      si = (word)((ax << 3) + 0x4000);	// character number * 8 = start of data
				// because the ega pics are in same bank as screen
      cseg ds = es;		// pics are just later in screen memory

      for (int row=0;row<8;row++)	// load in a row of the tile's picture
      {
	movsb (es,di,ds,si);
	if (row<7)
	  di += 39;
      }
    }
    bx += 2;			// next oldtile
    bp += 2;			// next view tile
    if (bp != dx)		// has an entire row from VIEW been drawn?
      continue;

    if (bx == 24*24*2)		// have all tiles been drawn?
      break;
    bp += 124;			// point it to the start of the next row
    dx += 172;			// point to end of next row
  }
}

//=======================================================================

byte linetables[4];		// (only used by the disabled hidden area
byte blockingtiles[128+7+2048];	// removal version of egarefresh)
