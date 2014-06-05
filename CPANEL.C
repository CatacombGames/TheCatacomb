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

/////////////////////////////////
//
// The PC-Arcade control panel
//
// Takes global variables as input
//
// videocard		CGAcard,EGAcard,VGAcard
//
// Modifies global variables relating to graphics, sound, and controls:
//
// grmode		CGAgr,EGAgr,VGAgr
// soundmode		nosound,spkr,adlib
// player1mode		keyboard,mouse,joystick1,joystick2
// player2mode
//
// JoyXlow[2], JoyXhigh[2], JoyYlow[2], JoyYhigh[2]
// MouseSensitivity, key[8], keyB1, keyB2
//
//
// The calling game must have the following functions:
//
// loadgrfiles		loads the files needed for the new mode
// repaintscreen	Clears the screen and totally redoes everything,
//			including hardware tricks and EGA latched pic data
//
//
// To call, just have a line which checks for F2 being pressed:
//
// if (keydown[0x3c])
//   controlpanel ();
//
// This should allso be called right after the titlepage!
//
/////////////////////////////////

#include "PCRLIB.H"

int rowy[4] = {4,9,14,19};
int collumnx[4] = {14,20,26,32};
int spotok[4][5];

int row,collumn;
grtype oldgrmode, newgrmode;
soundtype oldsoundmode, newsoundmode;
inputtype oldplayermode[3], newplayermode[3];

int joy1ok,joy2ok,mouseok;

////////////////////
//
// prototypes
//
////////////////////

void calibratejoy (int joynum);
void calibratemouse (void);
void printscan (int sc);
void calibratekeys (void);
void drawpanel (void);
void drawpanel (void);
void getconfig (void);

void controlpanel (void);

//=========================================================================

////////////////
//
// calibratejoy
// Brings up a dialog and has the user calibrate
// either joystick1 or joystick2
//
////////////////

void calibratejoy (int joynum)
{
  int stage,dx,dy,xl,yl,xh,yh;
  ControlStruct ctr;

  expwin (24,9);

  print(" Joystick Configuration\n\r");
  print(" ----------------------\n\r");
  print("Hold the joystick in the\n\r");
  print("upper left\n\r");
  print("corner and hit fire:");
  stage=15;
  do				// wait for a button press
  {
    drawchar (sx,sy,stage);
    WaitVBL ();
    WaitVBL ();
    WaitVBL ();
    if (++stage==23)
      stage=15;
    ReadJoystick (joynum,&xl,&yl);
    ctr = ControlJoystick(joynum);
    if (keydown[1])
      goto done;
  } while (ctr.button1!= 1);
  drawchar (sx,sy,' ');
  do                  		// wait for the button release
  {
    ctr = ControlJoystick(joynum);
  } while (ctr.button1);
  WaitVBL ();
  WaitVBL ();			// so the button can't bounce

  print("\n\n\rHold the joystick in the\n\r");
  print("lower right\n\r");
  print("corner and hit fire:");
  do				// wait for a button press
  {
    drawchar (sx,sy,stage);
    WaitVBL ();
    WaitVBL ();
    WaitVBL ();
    if (++stage==23)
      stage=15;
    ReadJoystick (joynum,&xh,&yh);
    ctr = ControlJoystick(joynum);
    if (keydown[1])
      goto done;
  } while (ctr.button1!= 1);
  drawchar (sx,sy,' ');
  do                  		// wait for the button release
  {
    ctr = ControlJoystick(joynum);
  } while (ctr.button1);

  //
  // figure out good boundaries
  //

  dx=(xh-xl) / 4;
  dy=(yh-yl) / 4;
  JoyXlow[joynum]=xl+dx;
  JoyXhigh[joynum]=xh-dx;
  JoyYlow[joynum]=yl+dy;
  JoyYhigh[joynum]=yh-dy;

done:
  clearkeys ();
  erasewindow ();
}

////////////////////////////
//
// calibratemouse
//
////////////////////////////
void calibratemouse (void)
{
  char ch;

  expwin (24,5);
  print ("  Mouse Configuration   \n\r");
  print ("  -------------------   \n\r");
  print ("Choose the sensitivity  \n\r");
  print ("of the mouse, 1 being   \n\r");
  print ("slow, 9 being fast:");
  do
  {  ch=get() % 256;
    if (ch==27)
      ch='5';
  } while (ch<'1' || ch>'9');
  MouseSensitivity = 15-(ch-'0');
  erasewindow ();
  _AX=0;
  geninterrupt (0x33);		// initialize the mouse
  _AX = 4;
  _CX=320;
  _DX=100;
  geninterrupt (0x33);		// set mouse status


}

/////////////////////////////
//
// print a representation of the scan code key
//
////////////////////////////
void printscan (int sc)
{
 char static chartable[128] =
 {'?','?','1','2','3','4','5','6','7','8','9','0','-','+','?','?',
  'Q','W','E','R','T','Y','U','I','O','P','[',']','|','?','A','S',
  'D','F','G','H','J','K','L',';','"','?','?','?','Z','X','C','V',
  'B','N','M',',','.','/','?','?','?','?','?','?','?','?','?','?',
  '?','?','?','?','?','?','?','?', 15,'?','-', 21,'5', 17,'+','?',
   19,'?','?','?','?','?','?','?','?','?','?','?','?','?','?','?',
  '?','?','?','?','?','?','?','?','?','?','?','?','?','?','?','?',
  '?','?','?','?','?','?','?','?','?','?','?','?','?','?','?','?'};

 sc = sc & 0x7f;

 if (sc==1)
   print ("ESC");
 else if (sc==0xe)
   print ("BKSP");
 else if (sc==0xf)
   print ("TAB");
 else if (sc==0x1d)
   print ("CTRL");
 else if (sc==0x2A)
   print ("LSHIFT");
 else if (sc==0x39)
   print ("SPACE");
 else if (sc==0x3A)
   print ("CAPSLK");
 else if (sc>=0x3b && sc<=0x44)
 {
   char str[3];
   print ("F");
   itoa (sc-0x3a,str,10);
   print (str);
 }
 else if (sc==0x57)
   print ("F11");
 else if (sc==0x59)
   print ("F12");
 else if (sc==0x46)
   print ("SCRLLK");
 else if (sc==0x1c)
   print ("ENTER");
 else if (sc==0x36)
   print ("RSHIFT");
 else if (sc==0x37)
   print ("PRTSC");
 else if (sc==0x38)
   print ("ALT");
 else if (sc==0x47)
   print ("HOME");
 else if (sc==0x49)
   print ("PGUP");
 else if (sc==0x4f)
   print ("END");
 else if (sc==0x51)
   print ("PGDN");
 else if (sc==0x52)
   print ("INS");
 else if (sc==0x53)
   print ("DEL");
 else if (sc==0x45)
   print ("NUMLK");
 else
   drawchar (sx++,sy,chartable[sc]);
}

/////////////////////////////
//
// calibratekeys
//
////////////////////////////
void calibratekeys (void)
{
  char ch;
  int hx,hy,i,select,new;

  expwin (22,15);
  print ("Keyboard Configuration\n\r");
  print ("----------------------");
  print ("\n\r0 north    :");
  print ("\n\r1 east     :");
  print ("\n\r2 south    :");
  print ("\n\r3 west     :");
  print ("\n\r4 northeast:");
  print ("\n\r5 southeast:");
  print ("\n\r6 southwest:");
  print ("\n\r7 northwest:");
  print ("\n\r8 button1  :");
  print ("\n\r9 button2  :");
  print ("\n\n\rModify which action:");
  hx=sx;
  hy=sy;
  for (i=0;i<8;i++)
  {
    sx=22;
    sy=7+i;
    printscan (key[i]);
  }
  sx=22;
  sy=15;
  printscan (keyB1);
  sx=22;
  sy=16;
  printscan (keyB2);

  do
  {
    sx=hx;
    sy=hy;
    ch=get() % 256;
    if (ch<'0' || ch>'9')
      continue;
    select = ch - '0';
    drawchar (sx,sy,ch);
    select = ch - '0';
    print ("\n\rPress the new key:");
    clearkeys ();
    new=-1;
    while (!keydown[++new])
      if (new==0x79)
	new=-1;
      else if (new==0x29)
	new++;				// skip STUPID left shifts!
    clearkeys ();
    print ("\r                  ");
    if (select<8)
      key[select]=new;
    if (select==8)
      keyB1=new;
    if (select==9)
      keyB2=new;
    sy=select+7;
    sx=22;
    print ("        ");
    sx=22;
    printscan (new);
    ch='0';				// so the loop continues
    clearkeys ();
  } while (ch>='0' && ch<='9');

  erasewindow ();
}


//=========================================================================

////////////////////
//
// getconfig
// Checks video cards, mouse, and joysticks
//
////////////////////

void getconfig (void)
{
  int x,y;
  int far *vect;

  spotok [0][0] = 1;
  spotok [0][1] = _egaok;
  spotok [0][2] = _vgaok;
  spotok [0][3] = 0;
  spotok [0][4] = 0;

  spotok [1][0] = 1;
  spotok [1][1] = 1;
  spotok [1][2] = 0;
  spotok [1][3] = 0;
  spotok [1][4] = 0;

  joy1ok = 0;
  joy2ok = 0;

  ReadJoystick (1,&x,&y);
  if (x<500)
    joy1ok = 1;
  ReadJoystick (2,&x,&y);
  if (x<500)
    joy1ok = 2;

  mouseok = 1;
  vect = (int far *) getvect (0x33);

  if (vect == NULL)
    mouseok = 0;		// vecter is NULL, calling would be bad...
  else
    if ((*vect & 255) == 0xcf)	// points to an IRET
      mouseok = 0;

  spotok [2][0] = 1;
  spotok [2][1] = mouseok;
  spotok [2][2] = joy1ok;
  spotok [2][3] = joy2ok;
  spotok [2][4] = 0;

}

//=========================================================================


////////////////
//
// drawpanel
// Draws everything inside the control panel window.
// Used to refresh under dialogs.
//
////////////////

void drawpanel (void)
{
  leftedge=1;

  xormask = 0;

  sx=8;
  sy=2;
  print ("       Control Panel      \n\r");

  getconfig ();

  sy=rowy[0]+2;
  sx=2;
  print ("VIDEO:");
  drawpic (collumnx[0]*8,rowy[0]*8,0);
  if (_egaok)
    drawpic (collumnx[1]*8,rowy[0]*8,1);
  else
    drawpic (collumnx[1]*8,rowy[0]*8,3);

  sy=rowy[1]+2;
  sx=2;
  print ("SOUND:");
  drawpic (collumnx[0]*8,rowy[1]*8,5);
  drawpic (collumnx[1]*8,rowy[1]*8,6);

  sy=rowy[2]+2;
  sx=2;
  print ("CONTROL:");
  drawpic (collumnx[0]*8,rowy[2]*8,7);
  if (mouseok)
    drawpic (collumnx[1]*8,rowy[2]*8,10);
  else
    drawpic (collumnx[1]*8,rowy[2]*8,12);
  if (joy1ok)
    drawpic (collumnx[2]*8,rowy[2]*8,8);
  else
    drawpic (collumnx[2]*8,rowy[2]*8,11);
  if (joy2ok)
    drawpic (collumnx[3]*8,rowy[2]*8,9);
  else
    drawpic (collumnx[3]*8,rowy[2]*8,11);

  drawchar(collumnx[(int) newgrmode -1]+1,rowy[0]+3,15);
  drawchar(collumnx[(int) newsoundmode]+1,rowy[1]+3,15);
  drawchar(collumnx[(int) newplayermode[1]]+1,rowy[2]+3,15);

  sy=21;
  sx=1;
  print ("  Move the cursor with the arrow keys \n\r");
  print ("   Make decisions with the ENTER key  \n\r");
  print ("       ESC to return to your game     \n\r");
}

//=========================================================================

////////////////
//
// controlpanel
//
////////////////

void controlpanel (void)
{
  int chf;
  char chl,chh;
  int oldcenterx,oldcentery;

  clearkeys ();			// clear out the F2 and other crap

  PauseSound ();		// pause any sound that is playing

//
// save off current settings so we can tell what changed
//
  newgrmode = oldgrmode = grmode;
  newsoundmode = oldsoundmode = soundmode;
  newplayermode[1] = oldplayermode[1] = playermode[1];
  newplayermode[2] = oldplayermode[2] = playermode[2];
  oldcenterx=screencenterx;
  oldcentery=screencentery;

  screencenterx = 19;
  screencentery = 11;

  setscreenmode (grmode);

//
// draw the screen
//
  drawwindow (0,0,39,24);
  drawpanel ();

  row = 0;
  collumn = (int) grmode-1;

  do
  {
//////////////////////////////////////

    sx=collumnx[collumn]+2;
    sy=rowy[row]+3;
    chf = get ();
    chl = chf % 256;
    chh = chf / 256;

    if (chh==0x48)	// up arrow
      if (--row<0)
	row = 2;

    if (chh==0x50)	// down arrow
      if (++row>2)
	row = 0;

    //
    // see if up or down took you to a bad spot
    //
    while (!spotok[row][collumn])
      collumn--;

    if (chh==0x4b)	// left arrow
    {
      if (collumn==0)
	collumn=4;
      while (!spotok[row][--collumn]);
    }

    if (chh==0x4d)	// right arrow
    {
      while (!spotok[row][++collumn] || collumn>3)
	if (collumn==4)
	  collumn=-1;
    }


    if (chl==13)		// return
    {
      switch (row)
      {
	case 0:
	       if ( (int) newgrmode == collumn+1 )
		 break;			// don't reload if not needed
	       drawchar(collumnx[(int) newgrmode-1]+1,rowy[row]+3,32);
	       grmode = newgrmode = (grtype) collumn + 1;	// becuase TEXT is 0
	       loadgrfiles ();
	       setscreenmode (grmode);
	       if (grmode==EGAgr)
		 moveega();
	       drawwindow (0,0,39,24);
	       drawpanel ();
	       break;
	case 1:
	       drawchar(collumnx[(int) newsoundmode]+1,rowy[row]+3,32);
	       newsoundmode = (soundtype) collumn;
	       break;
	case 2:
	       drawchar(collumnx[(int) newplayermode[1]]+1,rowy[row]+3,32);
	       newplayermode[1] = (inputtype) collumn;
	       if (newplayermode[1]==keyboard)
		 calibratekeys ();
	       else if (newplayermode[1]==mouse)
		 calibratemouse ();
	       else if (newplayermode[1]==joystick1)
		 calibratejoy (1);
	       else if (newplayermode[1]==joystick2)
		 calibratejoy (2);
	       drawpanel ();
	       break;
      }
      drawchar(collumnx[collumn]+1,rowy[row]+3,15);
    }

//////////////////////////////////////

  } while (chl!=27);	// ESC to quit

//
// done, so return to game
//
  playermode[1] = newplayermode[1];
  playermode[2] = newplayermode[2];

  grmode = newgrmode;

  screencenterx = oldcenterx;
  screencentery = oldcentery;

  soundmode = newsoundmode;
  repaintscreen ();		// have the game redraw everything
  ContinueSound ();		// continue any sound that was playing
}


//==========================================================================

/*
===================
=
= installgrfile
=
= Loads a PC-arcade graphic file
= grmode must be set so it knows if it is an EGA planed file
=
= Can be a normal uncompressed file or an RLE'd file
=
===================
*/

unsigned egaplane[4];			// main memory paragraph of plane image
spritetype image, spritetable[NUMSPRITES];	// grfile headers
pictype pictable[NUMPICS];
void far *lastgrpic;

int numchars,numtiles,numpics,numsprites;

void installgrfile (char *filename, int unpack,void huge *inmem)
{
  int i;
  unsigned long a,b,c,d;
  typedef pictype ptype[NUMPICS];
  typedef spritetype stype[NUMSPRITES];

  typedef struct {void huge *charptr;
		  void huge *tileptr;
		  void huge *picptr;
		  void huge *spriteptr;
		  ptype huge *pictableptr;
		  stype huge *spritetableptr;
		  void huge *plane[4];
		  int numchars,numtiles,numpics,numsprites;
		 } picfiletype;

  picfiletype huge *picfile;

  stype huge *(spriteinfile);
  ptype huge *(picinfile);


  if (!filename[0])
	{
	 picfile=(picfiletype huge *)inmem;
	}
  else
	{
	 if ( (long)lastgrpic )
	   farfree ((void far *)lastgrpic); // so new graphics modes will free it up

	 if (unpack)
	   picfile = (picfiletype huge *) bloadin /* LZW */ (filename);
	 else
	   picfile = (picfiletype huge *) bloadin (filename);

	 lastgrpic = (void far *) lastparalloc;
	}

  numchars = picfile->numchars;
  numtiles = picfile->numtiles;
  numpics = picfile->numpics;
  numsprites = picfile->numsprites;


  if (grmode==EGAgr)		// EGA is special because of bit plane grief!
  {
    charptr = MK_FP(EGADATASTART,0);
    tileptr = MK_FP(EGADATASTART+FP_SEG(picfile->tileptr)-FP_SEG(picfile->charptr),0);
    picptr = MK_FP(EGADATASTART+FP_SEG(picfile->picptr)-FP_SEG(picfile->charptr),0);
    spriteptr = MK_FP(EGADATASTART+FP_SEG(picfile->spriteptr)-FP_SEG(picfile->charptr),0);

    for (i=0;i<4;i++)
    {
      egaplane[i] = FP_SEG(picfile->plane[i])+FP_SEG(picfile);
      egaspriteptr[i] = MK_FP(FP_SEG(picfile)+FP_SEG(picfile->plane[i]) +
	FP_SEG(picfile->spriteptr) - FP_SEG(picfile->charptr),0);
    }
  }
  else
  {
    charptr = MK_FP(FP_SEG(picfile)+FP_SEG(picfile->charptr),0);
    tileptr = MK_FP(FP_SEG(picfile)+FP_SEG(picfile->tileptr),0);
    picptr = MK_FP(FP_SEG(picfile)+FP_SEG(picfile->picptr),0);
    spriteptr = MK_FP(FP_SEG(picfile)+FP_SEG(picfile->spriteptr),0);
  }
  //
  // copy tables into data segment
  //
  picinfile = MK_FP(FP_SEG(picfile->pictableptr)+FP_SEG(picfile)
    ,FP_OFF(picfile->pictableptr)+FP_OFF(picfile));
  spriteinfile = MK_FP(FP_SEG(picfile->spritetableptr)+FP_SEG(picfile)
    ,FP_OFF(picfile->spritetableptr)+FP_OFF(picfile));
  for (i=0; i<NUMPICS; i++)
    pictable[i] = (*picinfile)[i];
  for (i=0; i<NUMSPRITES; i++)
    spritetable[i] = (*spriteinfile)[i];

}




/*=========================================================================*/

//////////////////////////
//
// moveega
// moves the standard stuff into EGA memory
// needs to be called after each setgrmode to ega to refill memory
//
//////////////////////////
void moveega (void)
{
  int plane;

  for (plane=0;plane<4;plane++)
  {
    outportb (SCindex,SCmapmask);
    outportb (SCindex+1,1<<plane);	// write plane #

    movedata (egaplane[plane],0,EGADATASTART,0,0xffff-EGADATASTART);
  }
  outportb (SCindex,SCmapmask);		// read map select
  outportb (SCindex+1,15);	// all planes
}

