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

/*
** Catacomb II -- The C translation...
*/

#include "PCRLIB.H"

#define maxpics 2047
#define numtiles 24*24-1   /*number of tiles displayed on screen*/
#define numlevels 30
#define maxobj 400           /*maximum possible active objects*/
#define solidwall 129
#define blankfloor 128
#define leftoff 11
#define topoff 11
#define tile2s 256          /*tile number where the 2*2 pictures start*/
#define tile3s tile2s+64*4
#define tile4s tile3s+19*9
#define tile5s tile4s+19*16
#define lasttile tile5s+19*25

typedef enum {nosnd,blockedsnd,itemsnd,treasuresnd,bigshotsnd,shotsnd,
    tagwallsnd,tagmonsnd,tagplayersnd,killmonsnd,killplayersnd,opendoorsnd,
    potionsnd,spellsnd,noitemsnd,gameoversnd,highscoresnd,leveldonesnd,
    foundsnd} soundenum;

typedef enum {playercmd,gargcmd,dragoncmd,ramstraight,ramdiag,straight,idle,
    fade,explode} thinktype;

typedef enum {benign,monster,pshot,mshot,nukeshot} tagtype;

typedef enum {nothing,player,goblin,skeleton,ogre,gargoyle,dragon,wallhit,
    shot,bigshot,rock,dead1,dead2,dead3,dead4,dead5,dead6,teleporter,
    torch,lastclass} classtype;

typedef struct {
  boolean active;	/*if false, the object has not seen the player yet*/
  classtype  class;
  byte  x,y,		/*location of upper left corner in world*/
    stage,		/*animation frame being drawn*/
    delay;		/*number of frames to pause without doing anything*/
  dirtype  dir;		/*direction facing*/
  byte hp,		/*hit points*/
    oldx,oldy;		/*position where it was last drawn*/
  int oldtile;		/*origin tile when last drawn*/
  char filler[1];	/*pad to 16 bytes*/
   } activeobj;

typedef struct {	/*holds a copy of ActiveObj, and its class info*/
  boolean  active;	/*if false, the object has not seen the player yet*/
  classtype  class;
  byte  x,y,		/*location of upper left corner in world*/
    stage,		/*animation frame being drawn*/
    delay;		/*number of frames to pause without doing anything*/
  dirtype  dir;		/*direction facing*/
  byte hp,		/*hit points*/
    oldx,oldy;		/*position where it was last drawn*/
  int oldtile;		/*origin tile when last drawn*/
  char filler[1];	/*pad to 16 bytes*/

  thinktype think;
  tagtype contact;
  boolean  solid;
  word  firstchar;
  byte  size;
  byte  stages;
  byte  dirmask;
  word  speed;
  byte  hitpoints;
  byte  damage;
  word  points;
  char filler2[1];	/*pad to 32 bytes*/
  } objdesc;


/*=================*/
/*		   */
/* typed constants */
/*     		   */
/*=================*/
  char altmeters[14][13] = {
 {0,0,0,0,0,0,0,0,0,0,0,0,0},
 {190,0,0,0,0,0,0,0,0,0,0,0,0},
 {190,192,0,0,0,0,0,0,0,0,0,0,0},
 {190,191,192,0,0,0,0,0,0,0,0,0,0},
 {190,191,191,192,0,0,0,0,0,0,0,0,0},
 {190,191,191,191,192,0,0,0,0,0,0,0,0},
 {190,191,191,191,191,192,0,0,0,0,0,0,0},
 {190,191,191,191,191,191,192,0,0,0,0,0,0},
 {190,191,191,191,191,191,191,192,0,0,0,0,0},
 {190,191,191,191,191,191,191,191,192,0,0,0,0},
 {190,191,191,191,191,191,191,191,191,192,0,0,0},
 {190,191,191,191,191,191,191,191,191,191,192,0,0},
 {190,191,191,191,191,191,191,191,191,191,191,192,0},
 {190,191,191,191,191,191,191,191,191,191,191,191,193} };

  char meters[14][13] = {
 {0,0,0,0,0,0,0,0,0,0,0,0,0},
 {194,0,0,0,0,0,0,0,0,0,0,0,0},
 {194,196,0,0,0,0,0,0,0,0,0,0,0},
 {194,195,196,0,0,0,0,0,0,0,0,0,0},
 {194,195,195,196,0,0,0,0,0,0,0,0,0},
 {194,195,195,195,196,0,0,0,0,0,0,0,0},
 {194,195,195,195,195,196,0,0,0,0,0,0,0},
 {194,195,195,195,195,195,196,0,0,0,0,0,0},
 {194,195,195,195,195,195,195,196,0,0,0,0,0},
 {194,195,195,195,195,195,195,195,196,0,0,0,0},
 {194,195,195,195,195,195,195,195,195,196,0,0,0},
 {194,195,195,195,195,195,195,195,195,195,196,0,0},
 {194,195,195,195,195,195,195,195,195,195,195,196,0},
 {194,195,195,195,195,195,195,195,195,195,195,195,193} };

 dirtype opposite[9] =
    {south,west,north,east,southwest,northwest,northeast,southeast,nodir};


/*==================*/
/*		    */
/* global variables */
/*		    */
/*==================*/
  enum {game,demogame,demosave,editor} playmode;
  enum {quited,killed,reseted,victorious} gamexit; /*determines what to do after playloop*/

  int oldtiles [numtiles];		/*tile displayed last refresh*/
  int background[87][86];		/*base map*/
  int view[87][86];			/*base map with objects drawn in*/
  int originx, originy;			/*current world location of UL corn*/
  byte priority [maxpics+1];		/*tile draw overlap priorities*/

  int items[6];
  int shotpower;			/*0-13 characters in power meter*/
  int side;	                        /*which side shots come from*/
  int boltsleft;			/*number of shots left in a bolt*/

  activeobj o[maxobj+1];		/*everything that moves is here*/
  objdesc obj , altobj;			/*total info about objecton and alt*/
  int altnum;				/*o[#] of altobj*/
  int numobj,objecton;			/*number of objects in O now*/

  struct {
    thinktype think;			/*some of these sizes are for the*/
    tagtype contact;			/*convenience of the assembly routines*/
    boolean solid;
    word firstchar;
    byte size;
    byte stages;
    byte dirmask;
    word speed;
    byte hitpoints;
    byte damage;
    word points;
    byte filler[2];
  } ObjDef [lastclass];


  int i,j,k,x,y,z;
  boolean playdone, leveldone;

  boolean tempb;
  char far *tempp;

  int chkx,chky,chkspot;		/*spot being checked by WALK*/

  word frameon;
  char far *grmem;
  classtype clvar;

/****************************************************************************/

//////////////////////////////////
//
// function prototypes
//
//////////////////////////////////

void extern DrawObj (void);
void extern EraseObj (void);
void extern DoAll (void);
void extern EGAmove (void);
void extern CGArefresh (void);
void extern EGArefresh (void);



/*==============================*/
/*			        */
/* xxxREFRESH                   */
/* Refresh the changed areas of */
/* the tiles map in the various */
/* graphics modes.              */
/*			        */
/*==============================*/

char demowin [5][16] = {
  {14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,16},
  {17,' ','-','-','-',' ','D','E','M','O',' ','-','-','-',' ',18},
  {17,'S','P','A','C','E',' ','T','O',' ','S','T','A','R','T',18},
  {17,'F','1',' ','T','O',' ','G','E','T',' ','H','E','L','P',18},
  {19,20,20,20,20,20,20,20,20,20,20,20,20,20,20,21} };

void refresh (void)
{
  int x,y,basex,basey;
  word underwin [5][16];

  basex=originx+4;
  basey=originy+17;
  if (playmode==demogame)
    for (y=0; x<=4; y++)
      for (x=0; x<=15; x++)
	{
	  underwin[y][x]=view[y+basey][x+basex];
	  view[y+basey][x+basex]=demowin[y][x+1];
	};

  WaitVBL ();
  if (grmode==CGAgr)
    CGArefresh();
  else
    EGArefresh();

  if (playmode==demogame)
    for (y=0; x<=4; y++)
      for (x=0; x<=15; x++)
	view[y+basey][x+basex]=underwin[y][x];
  WaitVBL ();
}


void simplerefresh(void)
{
  WaitVBL ();
  if (grmode==CGAgr)
    CGArefresh();
  else
    EGArefresh();

}


/*======================================*/
/*				        */
/* RESTORE                              */
/* Redraws every tile on the tiled area */
/* by setting oldtiles to -1.  Used to  */
/* erase any temporary windows.         */
/*				        */
/*======================================*/

void clearold (void)
{
  memset (&oldtiles,0xff,sizeof(oldtiles)); /*clear all oldtiles*/
};


void restore (void)
{
  clearold ();
  SimpleRefresh ();
};


/*      */
/* Help */
/*      */
boolean wantmore (void)
{
  sx=2;
  sy=20;
  Print ("(SPACE for more/ESC)");
  sx=12;
  sy=21;
  ch = get ();
  if (ch==chr(27))
    return false;

  return true;
};


/*	   */
/* charpic */
/*	   */
void charpic(int x,int y, classtype c, dirtype dir, int stage)
{
  int xx,yy,size,tilenum;

  size=ObjDef[c].size;
  tilenum=ObjDef[c].firstchar+size*size
    * ((integer(dir) & ObjDef[c].dirmask)*ObjDef[c].stages+stage);

  for (yy=y;yy<=y+size-1;yy++)
    for (xx=x;xx<=x+size-1;xx++)
      {
	charout (xx,yy,tilenum);
	inc(tilenum);
      };
};

void help (void)
{
  int x,y;

  CenterWindow (20,20);
  Print ("  C A T A C O M B   \n");
  Print ("   - - - - - - -    \n");
  Print (" By John Carmack &  \n");
  Print ("     PC Arcade      \n");
  Print ("\n");
  Print ("F1 = Help           \n");
  Print ("F2 = Sound on / off \n");
  Print ("F3 = Controls       \n");
  Print ("F4 = Game reset     \n");
  Print ("F9 = Pause          \n");
  Print ("F10= Quit           \n");
  Print ("\n");
  Print ("Watch the demo for  \n");
  Print ("a play example.     \n");
  Print ("\n");
  Print ("Hit fire at the demo\n");
  Print ("to begin playing.   \n");
  if (!Wantmore())
    return;

  CenterWindow (20,20);
  Print ("\nKeyboard controls:  \n\n");
  Print ("Move    : Arrows    \n");
  Print ("Button1 : Ctrl      \n");
  Print ("Button2 : Alt       \n");
  Print ("\nTo switch to mouse \n");
  Print ("or joystick control,\n");
  Print ("hit F3.             \n");

  if (!Wantmore())
    return;

  CenterWindow (20,20);
  Print ("Button 1 / CTRL key:\n");
  Print ("Builds shot power.  \n");
  Print ("If the shot power   \n");
  Print ("meter is full when  \n");
  Print ("the button is       \n");
  Print ("released, a super   \n");
  Print ("shot will be        \n");
  Print ("launched.           \n");
  Print ("\n");
  for (y=11; y<=18; y++)
    for (x=3; x<=20; x++)
      Charout (x,y,128);

  charpic (4,14,player,east,2);
  charpic (19,15,shot,east,1);
  charpic (17,14,shot,east,0);
  charpic (15,15,shot,east,1);
  charpic (8,14,bigshot,east,0);

  if (!Wantmore())
    return;

  CenterWindow (20,20);
  Print ("Button 2 / ALT key:\n");
  Print ("Allows you to move  \n");
  Print ("without changing the\n");
  Print ("direction you are   \n");
  Print ("facing.  Good for   \n");
  Print ("searching walls and \n");
  Print ("fighting retreats.  \n");
  for (y=11; y<=18; y++)
    for (x=3; x<=20; x++)
      if (y==15)
	charout (x,y,129);
      else if (y==16)
	charout (x,y,131);
      else
	charout (x,y,128);
  charpic (6,13,player,south,2);
  sx=6;
  sy=15;
  print ("\35\35\36\36\37\37");

  if (!Wantmore())
    return;

  CenterWindow (20,20);
  Print (""P"" or ""SPACE"" will \n");
  Print ("take a healing      \n");
  Print ("potion if you have  \n");
  Print ("one.  This restores \n");
  Print ("the body meter to   \n");
  Print ("full strength.  Keep\n");
  Print ("a sharp eye on the  \n");
  Print ("meter, because when \n");
  Print ("it runs out, you are\n");
  Print ("dead!               \n\n");
  Print ("""B"" will cast a bolt\n");
  Print ("spell if you have   \n");
  Print ("any.  You can mow   \n");
  Print ("down a lot of       \n");
  Print ("monsters with a bit \n");
  Print ("of skill.           \n");

  if (!Wantmore())
    return;

  CenterWindow (20,20);
  Print ("""N"" or ""ENTER"" will \n");
  Print ("cast a nuke spell.  \n");
  Print ("This usually wipes  \n");
  Print ("out all the monsters\n");
  Print ("near you.  Consider \n");
  Print ("it a panic button   \n");
  Print ("when you are being  \n");
  Print ("mobbed by monsters! \n\n");
  Print ("	         \200\200\200\n");
  Print ("Potions:       \200\242\200\n");
  Print ("	         \200\200\200\n");
  Print ("Scrolls:       \200\243\200\n");
  Print (" (bolts/nukes) \200\200\200\n");
  Print ("Treasure:      \200\247\200\n");
  Print (" (points)      \200\200\200\n");
  Print ("	         \200\200\200\n");

  Wantmore();

};

/*       */
/* Reset */
/*       */
void reset(void)
{
  CenterWindow (18,1);
  Print ("Reset game (Y/N)?");
  ch= get ();
  if (ch=='Y')
    {
      gamexit=killed;
      playdone=true;
    };
};


/*===========================*/
/*			     */
/* CHECKKEYS                 */
/* If a key has been pressed */
/* it will be assigned to CH/*/
/* altkey, and if it is an F */
/* key, it will be processed.*/
/*			     */
/*===========================*/

void checkkeys (void)

{
};


/*=========================================================================*/


/*==============================*/
/*			        */
/* LOADLEVEL / SAVELEVEL        */
/* Loads map LEVEL into memory, */
/* and sets everything up.      */
/*			        */
/*==============================*/

void loadlevel(void)
{

  classtype tokens[256-230]  =
    {player,teleporter,goblin,skeleton,ogre,gargoyle,dragon,nothing,
     nothing,nothing,nothing,nothing,nothing,nothing,nothing,nothing,
     nothing,nothing,nothing,nothing,nothing,nothing,nothing,nothing,
     nothing,nothing};

  char filename[64],st[64];
  int x,y,xx,yy,recs, btile;
  char sm[4096];

  strcpy (filename,"LEVEL");
  itoa (level,st,10);
  strcat (filename,level);

  LoadFile (filename,sm);

  numobj=0;
  o[0].x=13;          /*just defaults if no player token is found*/
  o[0].y=13;
  o[0].stage=0;
  o[0].delay=0;
  o[0].dir=east;
  o[0].oldx=0;
  o[0].oldy=0;
  o[0].oldtile=-1;


  for (yy=0; yy<64; yy++)
    for (xx=0; xx<64; xx++)
      {
	btile=sm[yy*64+xx];
	if (btile<230)
	  background[yy+topoff][xx+leftoff]=btile;
	else
	  {

/*hit a monster token*/
	    background[yy+topoff][xx+leftoff]=blankfloor;
	    if (tokens[btile-230]==player)

/*the player token determines where you start in level*/

	      {
		o[0].x=xx+topoff;
		o[0].y=yy+leftoff;
	      }
            else

/*monster tokens add to the object list*/

	      {
		numobj++;
		o[numobj].active=false;
		o[numobj].class=tokens[btile-230];
		o[numobj].x=xx+leftoff;
		o[numobj].y=yy+topoff;
		o[numobj].stage=0;
		o[numobj].delay=0;
		o[numobj].dir=(dirtype)(rndt()/64);  /*random 0-3*/
		o[numobj].hp=ObjDef[o[numobj].class].hitpoints;
		o[numobj].oldx=x;
		o[numobj].oldy=y;
		o[numobj].oldtile=-1;
	      };

	    };

	  };



  originx = o[0].x-11;
  originy = o[0].y-11;

  shotpower=0;
  for (y=topoff-1; y<65+topoff; y++)
    for (x=leftoff-1; x<64+leftoff; x++)
      view[y][x]=background[y][x];

  sx=33;                  /*print the new level number on the right window*/
  sy=1;
  printint (level);
  Print (" ");          /*in case it went from double to single digit*/
  restore();
};


/*==========================================================================*/


#include "cat_play.c"

#include "objects.c"



/*========================================*/
/*					  */
/* Finished                               */
/* SHows the } page...                    */
/*					  */
/*========================================*/

void finished()
{
  playsound (treasuresnd);
  WaitEndSound();
  playsound (treasuresnd);
  WaitEndSound();
  playsound (treasuresnd);
  WaitEndSound();
  playsound (treasuresnd);
  WaitEndSound();

  sx=20;
  sy=24;
  get ();

};


/*================================*/
/*				  */
/* PLAYSETUP                      */
/* Set up all data for a new game */
/* Does NOT start it playing      */
/*				  */
/*================================*/

void playsetup()
{
  int i;

  score=0;
  shotpower=0;
  level=1;
  if (keydown [0x2E] || keydown [0x14])   /*hold down 'C' and 'T' to CheaT!*/
  {
    CenterWindow (16,2);
    Print ("Warp to which\nlevel (1-99)?");
    inputint (level);
    if (level<1)
      level=1;
    if (level>30)
      level=30;
    restore();
  };

  for (i=1; i<6; i++)
    items[i]=0;

  o[0].active = true;
  o[0].class = player;
  o[0].hp = 13;
  o[0].dir=west;
  o[0].stage=0;
  o[0].delay=0;

  DrawWindow (24,0,38,23);  /*draw the right side window*/
  Print ("  Level\n\nScore:\n\nTop  :\n\nK:\nP:\nB:\nN:\n\n\n");
  Print (" Shot Power\n\n\n    Body\n\n\n");
  printhighscore();
  printbody();
  printshotpower();

/*give them a few items to start with*/

  givenuke();
  givenuke();
  givebolt();
  givebolt();
  givebolt();
  givepotion();
  givepotion();
  givepotion();

};



/*================================*/
/*			          */
/* GAMEOVER                       */
/* Do a game over bit,  check     */
/* for a high score,  return      */
/* to demo.                       */
/*				  */
/*================================*/

void gameover()
{
  int place,i,j;
  char st[64];

  WaitEndSound();
  simplerefresh();
};


/****************************************************************************/


#if 0

/*====================*/
/*		      */
/* EDITORLOOP         */
/* The editor mode... */
/*		      */
/*====================*/

void editorloop()

Label
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
/* LOADLEVEL                    */
/* Loads map LEVEL into memory, */
/* nothing more                 */
/*			      */
/*			      */

Procedure LoadLevel;
label
  tryopen,fileread;
Var
  filename : string;
  st: string[3];
  x,y,xx,yy,recs, btile : Integer;
  iofile: file;
  tile: byte;
  sm : array [0..4095] of byte;

{
  str(level:1,st);
  filename=concat ('LEVEL',st,'.CAT');

tryopen:

  Assign (iofile,filename);
  Reset (iofile,1);
  If ioresult<>0
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
	background [topoff,leftoff]=133;  /*perspective NW corner*/
        goto fileread;
      }

    else

  BlockRead (iofile,sm,4096,recs);
  Close (iofile);

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
  shortnum (level);
  Print (' ');          /*in case it went from double to single digit*/
  restore;
};



/*	    */
/* Save Level */
/*	    */
Procedure Saveit;
Var
  iofile : file;
  filename : string;
  x,y,recs : Integer;
  tile: byte;
  st: string[3];
  sm : array [0..4095] of byte;
{
  CenterWindow (9,1);
  Print ('Saving...');
  For y=0 to 63 do
    for x=0 to 63 do
      {
	tile=background[y+topoff,x+leftoff] and $00FF;

/*if the tile was an exploding block, change back to undetectable*/

	if (tile>=171) and (tile<=180)
	  tile=tile-35;
	sm[y*64+x]=tile;
      };
  str(level:1,st);
  filename=concat ('LEVEL',st,'.CAT');
  Assign (iofile,filename);
  Rewrite (iofile,1);
  BlockWrite (iofile,sm,4096,recs);
  Close (iofile);
  restore;
};



/*	      */
/* Select Level */
/*	      */
function SelectLevel:boolean;
Var
  err:integer;
  lv:string;
{
  selectlevel=false;              /*editor won't reload a level if false*/
  CenterWindow (16,2);
  Print ('Edit which level](1-99):');
  input (lv,2);
  if lv[1]=chr(27) 	       /*allow ESC to quit editor mode*/
    {
      leveldone=true;
      playdone=true;
    };
  val (lv,level,err);
  If level>=1
    selectlevel=true;
  restore;
};


/*	      */
/* Toggle Block */
/*	      */
Procedure ToggleBlock;
Var
  x,y,block:integer;
{
  x=originx+topoff;
  y=originy+leftoff;
  block=background [y,x];

  If block=blankfloor
    block=solidwall
  else
    block=blankfloor;

  background [y,x]=block;
  view [y,x]=block;
};

/*	   */
/* Print map */
/*	   */
Procedure PrintMap;
var
  x,y,block:integer;
  ch: char;
{
  writeln (lst);
  Writeln (lst,'CATACOMB level ',level);
  for y=0 to 65 do
    {
      for x=0 to 65 do
	{
	  block=background[topoff-1+y,leftoff-1+x];
	  case block of
	    0..127: ch=chr(block);	/*ASCII*/
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
	    230..238: ch=chr(ord('0')+block-229); /*tokens*/
	    else ch='?';
	  };
	  write (lst,ch);
      };
    writeln (lst);
  };
  writeln (lst,chr(12));
};

/*==================================*/

{

  regs.ax=0;
  intr($33,regs);    /*show the mouse cursor*/

  DrawWindow (24,0,38,23);  /*draw the right side window*/
  Print ('  Level]] Map editor]]F4=exit]F7=Load]F8=Save]^P=Print');

  sx=25;
  leftedge=sx;
  sy=10;
  for i=1 to 12 do
    Print (samplepics[i]+']');

  drawtile=solidwall;
  ltx=28;
  lty=13;
  ltt=solidwall;
  xormask=$FFFF;
  charout (ltx,lty,ltt);         /*highlight the new block*/
  xormask=0;

  level=1;
  playdone=false;

  Repeat
    leveldone=false;
    originx=0;
    originy=0;

    If selectlevel  /*let them choose which level to edit*/
      loadlevel
    else
      goto cmdover;     /*so if they pressed ESC, they can leave*/

    repeat
      SimpleRefresh;

      regs.ax=1;
      intr($33,regs);    /*show the mouse cursor*/
      waitvbl;           /*make sure it gets seen*/
      waitvbl;

      Repeat
	regs.ax=3;
	intr($33,regs);  /*mouse status*/
      Until keypressed or (regs.bx and 3>0);

      sx=regs.cx div 16;   /*tile on screen mouse is over*/
      sy=regs.dx div 8;

      regs.ax=2;
      intr($33,regs);    /*hide the mouse cursor*/

      Checkkeys;       /*handles F keys and returns a keypress*/

      ch=chr(0);
      altkey=false;
      if keypressed
	{
	  ch=upcase(readkey);
	  if ch=chr(0)
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

/*left button places/deletes a DRAWTILE*/

		{
		  background[y,x]=drawtile;
		  view[y,x]=drawtile;
		};

	      if (regs.bx and 2>0)   /*right button places a BLANKFLOOR*/
		{
		  background[y,x]=blankfloor;
		  view[y,x]=blankfloor;
		};

	      if (not altkey) and ((ch>='A') and (ch<='Z')
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
	  charout (ltx,lty,ltt);         /*unhighlight the old DRAWTILE*/
	  drawtile=ord(samplepics[y][x]);
	  ltx=sx;
	  lty=sy;
	  ltt=drawtile;
	  xormask=$FFFF;
	  charout (ltx,lty,ltt);         /*highlight the new block*/
	  xormask=0;
	};


      Rd_Keyboard (dir,b1,b2);
      case dir of
	north: if originy>0
		originy=originy-1
              else
		playsound (blockedsnd);
	west: if originx>0

		originx=originx-1
              else
		playsound(blockedsnd);
	east: if originx<51+leftoff
		originx=originx+1
              else
		playsound(blockedsnd);
	south: if originy<51+topoff
		originy=originy+1
              else
		playsound(blockedsnd);
      };


      If keydown[$19] and keydown[$1d]  /*control-P*/
	PrintMap;

      If keydown[$42]
	{
	  keydown[$42]=false;
	  SaveIt;
	};

      If keydown[$41]
	{
	  keydown[$41]=false;
	  leveldone=true;        /*so SELECTLEVEL will be called*/
	};

cmdover:

    Until leveldone or playdone;
  Until playdone;

};

#endif

/***************************************************************************/
/*=========================*/
/*			   */
/* M A I N   P R O G R A M */
/*			   */
/*=========================*/

void main (void)
{
  initobjects();

  memset (&priority,99,sizeof(priority));

  priority[blankfloor]=0;
  for (i=ObjDef[teleporter].firstchar; i<=ObjDef[teleporter].firstchar+20;i++)
    priority[i]=0;
  for (clvar=dead2; clvar<=dead5; clvar++)
    for (i=ObjDef[clvar].firstchar; i<=ObjDef[clvar].firstchar+
    ObjDef[clvar].size*ObjDef[clvar].size; i++)
      priority[i]=0;		/*deadthing*/
  for (i=152; i<=161; i++)
    priority[i]=2;		/*shots*/
  for (i=ObjDef[bigshot].firstchar; i<= ObjDef[bigshot].firstchar + 31; i++)
    priority[i]=2;		/*bigshot*/
  for (i=0; i<=tile2s-1; i++)
    if (priority [i]==99)
      priority[i]=3;		/*most 1*1 tiles are walls, etc*/
  for (i=tile2s; i<=maxpics; i++)
    if (priority[i]==99)
      priority[i]=4;		/*most bigger tiles are monsters*/
  for (i=ObjDef[player].firstchar; i<= ObjDef[player].firstchar + 63; i++)
    priority[i]=5;		/*player*/


  side=0;

  for (x=0; x<=85; x++)
    {
      for (y=0; y<=topoff-1; y++)
	{
	  view[x][y]=solidwall;
	  view[x][85-y]=solidwall;
	  background[x][y]=solidwall;
	  background[x][85-y]=solidwall;
	};
      view[86][x]=solidwall;
    };
  for (y=11; y<=74; y++)
    for (x=0; x<=leftoff-1; x++)
      {
	view[x][y]=solidwall;
	view[85-x][y]=solidwall;
	background[x][y]=solidwall;
	background[85-x][y]=solidwall;
      };


  playmode=demogame;


/*
    {
      playmode=demosave;
      playsound (bigshotsnd);
      WaitEndSound;
    };
*/

  while (1)			// keep going until _quit is called
  {
    switch (playmode)
    {
      case game:
	playsetup();
	playloop();
	If (gamexit==killed)
	  gameover();
	if (gamexit==victorious)
	{
	  finished();
	  gameover();
	};
	playmode= demogame;
	break;


      case demogame:
	PlaySetup();
	PlayLoop();
	if (playmode==demogame)
	{
	  score=0;	/*so demo doersn't get a high score*/
	  GameOver();	/*if entire demo has cycled, show highs*/
	};
	break;

      case editor :
	EditorLoop();
	playmode=demogame;
	break;

    };

  }

}

