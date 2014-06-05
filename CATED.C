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

#if 0

/*====================*/
/*		      */
/* editorloop         */
/* the editor mode... */
/*		      */
/*====================*/

void editorloop()

label
  cmdover;

const
  samplepics : array[1..12] of string[13] =
    ("\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80",
     "\x80\x80\x81\x81\x87\x80\x80\x80\x87\x87\xb1\x80\x80",
     "\x80\x81\x81\x81\x81\x87\x80\x87\x87\x87\x87\xb1\x80",
     "\x80\x81\x81\x81\x81\x82\x80\x87\xb2\xb3\xb4\xac\x80",
     "\x80\x86\x81\x81\x85\x84\x80\xb0\x87\x87\xaf\xae\x80",
     "\x80\x80\x86\x83\x84\x80\x80\x80\xb0\xad\xae\x80\x80",
     "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80",
     "\x80\xa2\x80\xa3\x80\xa4\x80\xa7\x80\xa5\x80\xa6\x80",
     "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80",
     "\x80\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\x80",
     "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80",
     "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80");

var
  drawtile:integer;
  ltx,lty,ltt,x,y,i:integer;
  dor: dirtype;
  b1,b2: boolean;
/*$i-*/

/*			      */
/*			      */
/* loadlevel                    */
/* loads map level into memory, */
/* nothing more                 */
/*			      */
/*			      */

procedure loadlevel;
label
  tryopen,fileread;
var
  filename : string;
  st: string[3];
  x,y,xx,yy,recs, btile : integer;
  iofile: file;
  tile: byte;
  sm : array [0..4095] of byte;

{
  str(level:1,st);
  filename=concat ('level',st,'.cat');

tryopen:

  assign (iofile,filename);
  reset (iofile,1);
  if ioresult<>0
/*create a blank level for the editor*/
      {
	for x=0 to 63 do
	  for y=0 to 63 do
	    background[y+topoff,x+leftoff]=blankfloor;
	for x=0 to 63 do
	  {
	    background[topoff,x]=131;     /*perspective north wall*/
	    background[topoff+63,x]=129;  /*solid south wall*/
	    background[x,leftoff]=130;    /*perspective west wall*/
	    background[x,leftoff+63]=129; /*solid east wall*/
	  };
	background [topoff,leftoff]=133;  /*perspective nw corner*/
        goto fileread;
      }

    else

  blockread (iofile,sm,4096,recs);
  close (iofile);

  numobj=0;

  for yy=0 to 63 do
    for xx=0 to 63 do
      {
	tile=sm[yy*64+xx];

/*if tile is an exploding block, change it to a special icon for editor*/

	if (tile>=136) and (tile<=145)
	  tile=tile+35;
	background[yy+topoff,xx+leftoff]=tile;
      };

fileread:

  for y=topoff to 63+topoff do
    for x=leftoff to 63+leftoff do
      view[y,x]=background[y,x];
  sx=33;                  /*print the new level number on the right window*/
  sy=1;
  printint (level);
  print (' ');          /*in case it went from double to single digit*/
  restore;
};



/*	    */
/* save level */
/*	    */
procedure saveit;
var
  iofile : file;
  filename : string;
  x,y,recs : integer;
  tile: byte;
  st: string[3];
  sm : array [0..4095] of byte;
{
  centerwindow (9,1);
  print ('saving...');
  for y=0 to 63 do
    for x=0 to 63 do
      {
	tile=background[y+topoff,x+leftoff] and $00ff;

/*if the tile was an exploding block, change back to undetectable*/

	if (tile>=171) and (tile<=180)
	  tile=tile-35;
	sm[y*64+x]=tile;
      };
  str(level:1,st);
  filename=concat ('level',st,'.cat');
  assign (iofile,filename);
  rewrite (iofile,1);
  blockwrite (iofile,sm,4096,recs);
  close (iofile);
  restore;
};



/*	      */
/* select level */
/*	      */
function selectlevel:boolean;
var
  err:integer;
  lv:string;
{
  selectlevel=false;              /*editor won't reload a level if false*/
  centerwindow (16,2);
  print ('edit which level](1-99):');
  input (lv,2);
  if lv[1]=27 	       /*allow esc to quit editor mode*/
    {
      leveldone=true;
      playdone=true;
    };
  val (lv,level,err);
  if level>=1
    selectlevel=true;
  restore;
};


/*	      */
/* toggle block */
/*	      */
procedure toggleblock;
var
  x,y,block:integer;
{
  x=originx+topoff;
  y=originy+leftoff;
  block=background [y,x];

  if block=blankfloor
    block=solidwall
  else
    block=blankfloor;

  background [y,x]=block;
  view [y,x]=block;
};

/*	   */
/* print map */
/*	   */
procedure printmap;
var
  x,y,block:integer;
  ch: char;
{
  writeln (lst);
  writeln (lst,'catacomb level ',level);
  for y=0 to 65 do
    {
      for x=0 to 65 do
	{
	  block=background[topoff-1+y,leftoff-1+x];
	  case block of
	    0..127: ch=block;	/*ascii*/
	    128: ch=' ';		/*floor*/
	    129..135: ch='#';		/*walls*/
	    171..177: ch='*';		/*exploding*/
	    178..180: ch='!';		/*hidden stuff*/
	    162: ch='p';		/*potion*/
	    163: ch='s';		/*scroll*/
	    164: ch='k';		/*key*/
	    165: ch='|';		/*door*/
	    166: ch='-';		/*door*/
	    167: ch='$';		/*treasure*/
	    230..238: ch='0'+block-229; /*tokens*/
	    else ch='?';
	  };
	  write (lst,ch);
      };
    writeln (lst);
  };
  writeln (lst,12);
};

/*==================================*/

{

  regs.ax=0;
  intr($33,regs);    /*show the mouse cursor*/

  drawwindow (24,0,38,23);  /*draw the right side window*/
  print ('  level]] map editor]]f4=exit]f7=load]f8=save]^p=print');

  sx=25;
  leftedge=sx;
  sy=10;
  for i=1 to 12 do
    print (samplepics[i]+']');

  drawtile=solidwall;
  ltx=28;
  lty=13;
  ltt=solidwall;
  xormask=$ffff;
  drawchar (ltx,lty,ltt);         /*highlight the new block*/
  xormask=0;

  level=1;
  playdone=false;

  repeat
    leveldone=false;
    originx=0;
    originy=0;

    if selectlevel  /*let them choose which level to edit*/
      loadlevel
    else
      goto cmdover;     /*so if they pressed esc, they can leave*/

    repeat
      simplerefresh;

      regs.ax=1;
      intr($33,regs);    /*show the mouse cursor*/
      WaitVBL();           /*make sure it gets seen*/
      WaitVBL();

      repeat
	regs.ax=3;
	intr($33,regs);  /*mouse status*/
      until keypressed or (regs.bx and 3>0);

      sx=regs.cx div 16;   /*tile on screen mouse is over*/
      sy=regs.dx div 8;

      regs.ax=2;
      intr($33,regs);    /*hide the mouse cursor*/

      checkkeys;       /*handles f keys and returns a keypress*/

      ch=0;
      altkey=false;
      if keypressed
	{
	  ch=upcase(readkey);
	  if ch=0
	    {
	      altkey=true;
	      ch=readkey;
	    }
	};

      if (sx<24) and (sy<24)
/*buttons pressed in tile map*/
	{
	  x=originx+sx;
	  y=originy+sy;
          if (x>=leftoff) and (x<leftoff+64) and
	     (y>=topoff) and (y<topoff+64)
	    {
	      if (regs.bx and 1>0)

/*left button places/deletes a drawtile*/

		{
		  background[y,x]=drawtile;
		  view[y,x]=drawtile;
		};

	      if (regs.bx and 2>0)   /*right button places a blankfloor*/
		{
		  background[y,x]=blankfloor;
		  view[y,x]=blankfloor;
		};

	      if (not altkey) and ((ch>='a') and (ch<='z')
	      or ((ch>='0') and (ch<='9') ) )
		{
		  if (ch>='0') and (ch<='9')
		    background[y,x]=ord(ch)+161   /*map numbers are later*/
		  else
		    background[y,x]=ord(ch)+32; /*map letters are lowercase*/
		  view[y,x]=background[y,x];
		  regs.ax=4;
		  regs.cx=regs.cx+16;
		  intr ($33,regs);        /*move the mouse over*/
		};

	      if (not altkey) and (ch=' ')   /*space makes a solidwall*/
		{
		  background[y,x]=solidwall;
		  view[y,x]=solidwall;
		  regs.ax=4;
		  regs.cx=regs.cx+16;
		  intr ($33,regs);        /*move the mouse over*/
		};

	    };
	};


      x=sx-24;
      y=sy-9;
      if  (regs.bx and 1>0) and (x>0) and (x<14) and (y>0) and (y<13)and
       (samplepics[y][x]<>#128)
/*button pressed in samplepics*/
	{
	  drawchar (ltx,lty,ltt);         /*unhighlight the old drawtile*/
	  drawtile=ord(samplepics[y][x]);
	  ltx=sx;
	  lty=sy;
	  ltt=drawtile;
	  xormask=$ffff;
	  drawchar (ltx,lty,ltt);         /*highlight the new block*/
	  xormask=0;
	};


      rd_keyboard (dir,b1,b2);
      case dir of
	north: if originy>0
		originy=originy-1
              else
		PlaySound (blockedsnd);
	west: if originx>0

		originx=originx-1
              else
		PlaySound(blockedsnd);
	east: if originx<51+leftoff
		originx=originx+1
              else
		PlaySound(blockedsnd);
	south: if originy<51+topoff
		originy=originy+1
              else
		PlaySound(blockedsnd);
      };


      if keydown[$19] and keydown[$1d]  /*control-p*/
	printmap;

      if keydown[$42]
	{
	  keydown[$42]=false;
	  saveit;
	};

      if keydown[$41]
	{
	  keydown[$41]=false;
	  leveldone=true;        /*so selectlevel will be called*/
	};

cmdover:

    until leveldone or playdone;
  until playdone;

};

#endif

