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

#pragma once

#include "dos/borland.h"	// dos.h mem.h sys\stat.h fcntl.h alloc.h io.h

typedef std::int16_t boolean;	// was enum {false,true}: 16 bits like Borland's
typedef unsigned char byte;
typedef std::uint16_t word;

#pragma pack(push,1)		// Borland -a- (byte alignment)

char extern ch,str[80];

/*=========================================================================*/

/*
** Sound routines
** Tied into INT 8, with a speeded up timer!
*/

typedef enum : std::int16_t {off,spkr,sdlib} soundtype;

typedef struct {word start;
		byte priority;
		byte samplerate;
		char name[12];} spksndtype;

typedef struct {char id[4];
		word filelength;
		word filler[5];
		spksndtype sounds[63];
		word freqdata[1];} SPKRtable;


soundtype extern soundmode;
char extern huge *SoundData;

int extern _dontplay;

void StartupSound (void);
void ShutdownSound (void);
void PlaySound (int sound);
void PauseSound (void);
void ContinueSound (void);
void StopSound (void);
void WaitEndSound (void);

/*=========================================================================*/

/*
** Control routines
** Ties into INT 9 to intercept all key presses, but passes on to BIOS
** The control panel handles all this stuff!
*/

typedef enum : std::int16_t {north,east,south,west,northeast,southeast,southwest,
	      northwest,nodir} dirtype;

inline dirtype operator++ (dirtype &d,int) {dirtype o=d;d=(dirtype)(d+1);return o;}
inline dirtype operator-- (dirtype &d,int) {dirtype o=d;d=(dirtype)(d-1);return o;}

typedef struct {dirtype dir;
		boolean button1,button2;} ControlStruct;

typedef enum : std::int16_t {keyboard,mouse,joystick1,joystick2,demo} inputtype;

inputtype extern playermode[3];
std::atomic<boolean> extern keydown[128];	// written by the INT 9 handler
std::int16_t extern JoyXlow[3], JoyXhigh[3], JoyYlow [3], JoyYhigh [3];	// 1&2 are used
std::int16_t extern MouseSensitivity;
char extern key[8], keyB1, keyB2;

enum demoenum : std::int16_t {notdemo,demoplay,recording};
enum demoenum extern indemo;

void extern interrupt (*oldint9) ();

void SetupKBD ();
void interrupt Int9ISR ();
void ShutdownKBD ();

void ReadJoystick (int joynum,int *xcount,int *ycount);

ControlStruct ControlKBD ();
ControlStruct ControlMouse ();
ControlStruct ControlJoystick (int joynum);
ControlStruct ControlPlayer (int player);

void RecordDemo (void);
void LoadDemo (int demonum);
void SaveDemo (int demonum);

/*========================================================================*/

/*
** Miscellaneous library routines
*/

void extern far *lastparalloc;

void huge *paralloc (long size);
long unsigned int LoadFile(const char *filename,char huge *buffer);
void SaveFile(const char *filename,char huge *buffer, long size);
void huge *bloadin (const char *filename);
void huge *bloadinLZW (const char *filename);
long RLEcompress (void far *source, long length, void far *dest);

void initrndt (boolean randomize);
int rndt (void);
void initrnd (boolean randomize);
int random (int maxval);

void clearkeys (void);

void extern (*timecall)(void);
unsigned extern timeax,timebx,timecx,timedx,timesi,timedi,timebp,timees;
int timesub (int ticks);

int _MouseInit(void);
void _MouseHide(void);
void _MouseShow(void);
int _MouseButton(void);
void _MouseCoords(int *x,int *y);

long _Verify(const char *filename);

void _printhexb(unsigned char value);
void _printhex(unsigned value);
void _printbin(unsigned value);

/*========================================================================*/

/*
** Graphic routines
*/

#define SCindex 0x3C4
#define SCmapmask 2
#define GCindex 0x3CE
#define GCmode 5
#define CRTCLINECOMPARE 0x18
#define CRTCOVERFLOW 0x7
#define CRTCMAXSCANLINE 0x9
#define CRTCSTARTL 0xd
#define CRTCSTARTH 0xc

typedef enum : std::int16_t {text,CGAgr,EGAgr,VGAgr} grtype;
typedef enum : std::int16_t {NOcard,MDAcard,CGAcard,EGAcard,MCGAcard,VGAcard,
	      HGCcard=0x80,HGCPcard,HICcard} cardtype;

grtype extern grmode;

cardtype extern _videocard;

int extern sx,sy,leftedge,xormask;	// stuff for screen text output

word extern CGAylookup [200],EGAylookup[256],VGAylookup[200];

unsigned extern crtcaddr;

void setscreenmode (grtype mode);
void WaitVBL (void);
void EGAplane (int plane);
void EGAlatch (void);
void EGAsplitscreen (int linenum);
void crtcstart (unsigned start);
cardtype VideoID (void);
void EGAmove (void);

/*=========================================================================*/

/*
** PC-Arcade graphic file format stuff
*/

#define NUMPICS 64
#define NUMSPRITES 10

unsigned extern EGADATASTART;

typedef struct {
		 std::int16_t width;
		 std::int16_t height;
		 dos::FarPtr shapeptr;		// reletive to spriteptr
		 dos::FarPtr maskptr;
		 std::int16_t xl,yl,xh,yh;		// death box pixel offsets
		 char name[12];
	       } spritetype;

typedef struct {
		 std::int16_t width;
		 std::int16_t height;
		 dos::FarPtr shapeptr;
		 char name[8];
	       } pictype;


int extern numchars,numtiles,numpics,numsprites;

spritetype extern image, spritetable[NUMSPRITES];	// grfile headers
pictype extern pictable[NUMPICS];

void extern huge *charptr;		// 8*8 tileset
void extern huge *tileptr;		// 16*16 tileset
void extern huge *picptr;		// any size picture set
void extern huge *spriteptr;		// any size masked and hit rect sprites
void extern huge *egaspriteptr[4];	// spriteptr for each plane

unsigned extern screenseg;		// loaded into ES in the draw routines
					// should be adjusted after grmode
					// switches, page flipping, and scrolls

void moveega (void);
void installgrfile (const char *filename,int unpack,void huge *inmem);

void drawchar (int x, int y, int charnum);
void drawtile (int x, int y, int picnum);
void drawpic (int x, int y, int picnum);

/*=========================================================================*/

/*
** higher level graphic routines
*/

int extern screencenterx ,screencentery, _yshift;

void controlpanel (void);
int get (void);
int _input(char *string,int max);
unsigned _inputint(void);
void print (const char *str);
void _printc(char *string);
void _printhexb(unsigned char value);
void _printhex(unsigned value);
void printint (int val);
void printlong (long val);
void drawwindow (int xl, int yl, int xh, int yh);
void erasewindow (void);
void bar (int xl,int yl, int xh, int yh, int ch);
void centerwindow (int width, int height);
void expwin (int width, int height);
void expwinh (int width, int height);
void expwinv (int width, int height);

/*=========================================================================*/

/*
** necessary routiones
*/

void loadgrfiles (void);    	// call installgrfile + any other files
void repaintscreen (void);	// do any screen wierdness and redraw all

/*========================================================================*/

/*
** game level routines
*/

std::int32_t extern score;
std::int16_t extern level;

typedef struct { int width;
		 int height;
		 int planes;
		 int screenx;
		 int screeny;
		 int screenw;
		 int screenh;
		 unsigned planesize;
	       } LevelDef;

int	extern	_numlevels, _maxplayers;
boolean	extern	_cgaok, _egaok, _vgaok;
const char	extern	*_extension;

struct scores {
	 std::int32_t score;
	 std::int16_t level;
	 char initials[4];
	};

struct scores extern scoreswap, highscores[5];


void _loadctrls (void);
void _savectrls (void);
void _loadhighscores (void);
void _savehighscores (void);
void _showhighscores (void);
void _checkhighscore (void);

void _setupgame (void);
void _quit (const char *);		// shuts everything down

/*=========================================================================*/


unsigned int extern RLECompress
  (char far *source, long sourcelen, char far *dest);

void extern RLEExpand
  (char far *source, char far *dest, long origlen);

#pragma pack(pop)



//NOLAN ADDED
extern boolean GODMODE;