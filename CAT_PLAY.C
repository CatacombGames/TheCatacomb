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

/*=====================================*/
/*				       */
/* newobject                           */
/* returns the number of a free object */
/*				       */
/*=====================================*/

int newobject()
{
  int i;

  for (i=1; i<= numobj; i++)
    if (o[i].class==nothing)
	goto gotit;
  if (numobj<maxobj)
    numobj++;
  i=numobj;

gotit:

  o[i].oldtile=-1;
  o[i].oldx=0;
  o[i].oldy=0;

  return i;
}


/*=================================*/
/*				   */
/* printscore / printhighscore     */
/* prints the scores to the screen */
/*				   */
/*=================================*/

void printscore()
{
  sx=31;
  sy=3;
  printlong (score);
}

void printhighscore ()
{
  sx=31;
  sy=5;
  printlong (highscores[1].score);
}


/*======================================*/
/*				        */
/* printshotpower                       */
/* printbody                            */
/* draws the meter to the current value */
/*				        */
/*======================================*/

void printshotpower()
{
  sx=25;
  sy=13;
  if (shotpower == 13)
    print (altmeters[13]);
  else
    print (meters[shotpower]);
}


void printbody()
{
  sx=25;
  sy=16;
  if (o[0].hp>6)
    print (meters[o[0].hp]);
  else
    print (altmeters[o[0].hp]);
}


/*=============================*/
/*			       */
/* levelcleared                */
/* goes to the next level, or  */
/* }s game if all levels done  */
/* checks for warp teleporters */
/*			       */
/*=============================*/

void levelcleared()
{
  char warp[2];
  int value;

  leveldone=true;

  warp[0]=(char) background[altobj.y+2][altobj.x]-161;
  if ( (warp[0]<'0') || (warp[0]>'9') )
    warp[0]='0';
  warp[1]=(char) background[altobj.y+2][altobj.x+1]-161;
  if ( (warp[1]<'0') || (warp[1]>'9') )
    warp[2]=' ';
 value = atoi (warp);

  if (value>0)
    level=value;
  else
    level++;

  if (level>numlevels)
/*all levels have been completed*/
    {
      playdone=true;
      gamexit=victorious;
    }
}


/*==================================*/
/*				    */
/* give / take potion / key / bolt  */
/* / nuke                           */
/* increments the item quantity and */
/* draws an extra icon if it fits   */
/*				    */
/*==================================*/

void givekey()
{
  int i;

  i=items[1]+1;
  items[1]=i;
  if (i<11)
    drawchar (26+i,7,31);  /*key icon*/
}


void givepotion()
{
  int i;

  i=items[2]+1;
  items[2]=i;
  if (i<11)
    drawchar (26+i,8,29);  /*potion icon*/
}


void givebolt()
{
  int i;

  i=items[3]+1;
  items[3]=i;
  if (i<11)
    drawchar (26+i,9,30);  /*scroll icon*/
}



void givenuke()
{
  int i;

  i=items[5]+1;
  items[5]=i;
  if (i<11)
    drawchar (26+i,10,30);  /*scroll icon*/
}


/*	   */
/* takekey */
/*	   */
boolean takekey()
{
  int i;

  if (items[1]>0)
    {
      i=items[1]-1;
      items[1]=i;
      if (i<10)
	drawchar (27+i,7,32);
      PlaySound (OPENDOORSND);
      return true;
    }
  else
    {
      PlaySound (NOITEMSND);
      return false;
    }
}


/*	      */
/* takepotion */
/*	      */
void takepotion()
{
  int i;

  if (items[2]>0)
    {
      i=items[2]-1;
      items[2]=i;
      if (i<11)
	drawchar (27+i,8,32);
      PlaySound(POTIONSND);
      o[0].hp=13;
      obj.hp=13;
      printbody();           /*update the body meter*/
    }
  else
    PlaySound(NOITEMSND);
}


/*	    */
/* castbolt */
/*	    */
void castbolt()
{
  int i;

  if (items[3]>0)
    {
      i=items[3]-1;
      items[3]=i;
      if (i<11)
	drawchar (27+i,9,32);
      boltsleft=8;

      PlaySound(SPELLSND);
    }
  else
    PlaySound(NOITEMSND);
}



/*	    */
/* castnuke */
/*	    */
void castnuke()
{
  int i,x,y,n;
  activeobj base;

  if (items[5]==0)
  {
    PlaySound(NOITEMSND);
    return;
  }

  i=items[5]-1;
  items[5]=i;
  if (i<11)
    drawchar (27+i,10,32);

  base.delay=0;
  base.stage=0;
  base.active=true;
  base.x=obj.x;
  base.y=obj.y;      /*start bigshot at same coordinate at player*/
  base.oldx=x;
  base.oldy=y;
  base.oldtile=-1;
  base.class=bigshot;

  for (x=-1; x<=1; x++)
    {	    /*make a whole buch of bigshots*/
      n=newobject();
      o[n]=base;
      o[n].x=o[n].x+x*2;
      o[n].dir=north;
      n=newobject();
      o[n]=base;
      o[n].x=o[n].x+x*2;
      o[n].dir=south;
      n=newobject();
      o[n]=base;
      o[n].y=o[n].y+x*2;
      o[n].dir=east;
      n=newobject();
      o[n]=base;
      o[n].y=o[n].y+x*2;
      o[n].dir=west;
    }

  PlaySound(SPELLSND);
  obj.stage=2;
  obj.delay=4;
}



/*=======================================*/
/*				         */
/* playshoot / playbigshoot              */
/* launches a missile of the proper type */
/* from the current object.  chooses from*/
/* smallshot, bigshot, and monshot.      */
/*				         */
/*=======================================*/

void playshoot()
{
  int new;

  obj.stage=2;
  obj.delay=4;
  PlaySound (SHOTSND);

  new=newobject();

  o[new].class=shot;
  side^=1;
  o[new].delay=0;
  o[new].stage=0;
  o[new].active=true;
  o[new].dir=obj.dir;  /*missile is allways facing same way as thrower*/
  switch ( o[new].dir )
  {
    case north:
	     o[new].x=obj.x+side;
	     o[new].y=obj.y;
	     break;
    case east:
	     o[new].x=obj.x+1;
	     o[new].y=obj.y+side;
	     break;
    case south:
	     o[new].x=obj.x+side;
	     o[new].y=obj.y+1;
	     break;
    case west:
	     o[new].x=obj.x;
	     o[new].y=obj.y+side;
	     break;
  }
}



void playbigshoot()
{
  int new;

  obj.stage=2;
  if (boltsleft==0)
    obj.delay=4;
  PlaySound (BIGSHOTSND);
  new=newobject();
  o[new].delay=0;
  o[new].stage=0;
  o[new].active=true;
  o[new].dir=obj.dir;  /*missile is allways facing same way as thrower*/
  o[new].x=obj.x;
  o[new].y=obj.y;      /*start bigshot at same coordinate at player*/
  o[new].class=bigshot;
}


/*============================*/
/*			      */
/* givescroll                 */
/* randomly gives a bolt/nuke */
/*			      */
/*============================*/

void givescroll()
{
  int r;

  if (rndt()<128)
    givebolt ();
  else
    givenuke ();
}


/*=========================================*/
/*					   */
/* opendoor                                */
/* open the door with a piece at chkx,chky */
/*				           */
/*=========================================*/

void opendoor()
{
  int x,y;

/*clears door icons both ways from the point contacted*/
 PlaySound (OPENDOORSND);
 x=chkx;
 y=chky;
 if (chkspot==165)
   {		      /*vertical doors*/
     do
     {
       view[y][x]=blankfloor;
       background[y][x]=blankfloor;
       y--;
     } while (view[y][x]==165);
     y=chky+1;
     while (view[y][x]==165)
     {
       view[y][x]=blankfloor;
       background[y][x]=blankfloor;
       y++;
     }
   }
 else
   {		     /*horizontal doors*/
     do
     {
       view[y][x]=blankfloor;
       background[y][x]=blankfloor;
       x--;
     } while (view[y][x]==166);
     x=chkx+1;
     while (view[y][x]==166)
     {
       view[y][x]=blankfloor;
       background[y][x]=blankfloor;
       x++;
     };
   }

}



/****************************************************************************/


/*======================================*/
/*				        */
/* tagobject                            */
/* have the obj do its damage to altobj */
/*				        */
/*======================================*/

void tagobject ()

{
  int i=altobj.hp;

  if ((GODMODE) && (altobj.class == player))
		return;

  altobj.hp -= obj.damage;
  if (i<=obj.damage)

/*it died*/

	 {
/*      erasealtobj;		*/	/*because dead has lower priority*/

		if (altobj.class == player)
	{
	  altobj.hp = o[0].hp = 0;
	  printbody();
	  PlaySound (KILLPLAYERSND);
	  playdone=true;               /*gameover by way of death*/
	  gamexit=killed;
	}

      else

	{
	  score=score+altobj.points; /*give player points for a kill*/
	  printscore();
	  PlaySound (KILLMONSND);
	}
      /*change class to a deadthing of same size*/
      o[altnum].class=(classtype)(dead1-1+altobj.size);
      o[altnum].delay=2;
      o[altnum].stage=0; /*start of fade*/
    }

  else

/*wasn't killed*/

    {
      if ( o[altnum].class == guns || o[altnum].class == gune )
	return;
      o[altnum].hp=altobj.hp;	/*save the new hp status*/
      o[altnum].stage=3;	/*set it to ouch stage*/
      if (altnum==0)
	{
	  o[0].delay=2;
	  printbody();     /*update body bar on screen*/
	  PlaySound (TAGPLAYERSND);
	}
      else
	{
	  o[altnum].delay=4;      /*four frames for monsters*/
	  PlaySound (TAGMONSND);
	}
    }
}


/*==============================*/
/*			        */
/* intomonster                  */
/* obj contacted another object */
/*			        */
/*==============================*/

boolean intomonster()
{
  int i;
  boolean gotit;

/*figure out which object got hit*/

  altnum=0;
  gotit=false;
  do
  {
  /* make a copy of the objects info into a global varriable */

    memcpy (&altobj.active,&o[altnum],sizeof(o[altnum]) );
    if ( (altobj.class>nothing) && (altnum!=objecton) )
      {
	memcpy (&altobj.think,&objdef[altobj.class],sizeof(objdef[altobj.class]) );
	if ( (chkx>=altobj.x) && (chkx-altobj.x<altobj.size)
	&& (chky>=altobj.y) && (chky-altobj.y<altobj.size) )
	  if (altobj.solid)
	    gotit=true;
	  else
	    if ( (objecton==0) && (altobj.class==teleporter || altobj.class==secretgate) )
	    /*player got to the teleporter*/
	      levelcleared();
      }
    if (!gotit)
      altnum++;
  } while (!(gotit || altnum>numobj) );


  if (!gotit)
    return true;

/*resolve contact based on attacker and target*/

  switch (obj.contact)
  {

    case benign:
      return false;	/*benign objects just don't move through others*/

    case monster:
    case mshot:
      if (altnum==0)
	{
	  tagobject();
	  obj.stage=2;   /*set it to attack stage*/
	  obj.delay=20;   /*delay for several frames*/
	}
      else
	if (altobj.class==shot)  /*they can walk into shots*/
	  return true;
      return false;

    case pshot:
      if (altnum>0)
	tagobject();
      return false;

    case nukeshot:
      tagobject();
      return true;   /*nuke shots keep going*/
  }
  return false;
}


/*=======================================*/
/*  			   	         */
/* walkthrough                           */
/* obj is trying to walk through chkspot */
/* at chkx,chky, is it ok?               */
/*				         */
/*=======================================*/

boolean walkthrough()
{
  int new;

  if (chkspot ==blankfloor)
    return true;

/* big object */

  if (chkspot>=tile2s && chkspot<=lasttile)
    return intomonster();

/* walls */

  if (chkspot>=129 && chkspot<=135)
  {
    if ( (obj.contact==pshot) || (obj.contact==nukeshot) || (obj.contact==mshot) )
  /*make an explosion over the wall*/
    {
      new = newobject();
      o[new].active=true;
      o[new].x=chkx;
      o[new].y=chky;
      o[new].stage=0;
      o[new].delay=2;
      o[new].class=wallhit;
      PlaySound (TAGWALLSND);
    }
    return false;
  }

/* exploding walls */

  if (chkspot>=136 && chkspot<=145)
  {
    if ((obj.contact==pshot) || (obj.contact==nukeshot))
    {
     PlaySound (TAGWALLSND);
     if (chkspot<143)
       background[chky][chkx]=blankfloor;
     else
       background[chky][chkx]=chkspot+19;
       /*hidden potion,scroll,key*/

     /*make an explosion over the wall*/
     new = newobject();
     o[new].active=true;
     o[new].x=chkx;
     o[new].y=chky;
     o[new].stage=0;
     o[new].delay=2;
     o[new].class=dead1;
     if (obj.contact==pshot)
       return false;
     else
       return true;             /*nuke shots keep going after blowing up one*/
    }

   else
     return false;	/*nothing else goes through exploding walls*/
  }

/* potion bottle */
  if (chkspot==162)
  {
    if (obj.class==player)
    {
      givepotion();
      view[chky][chkx]=blankfloor;  /*erase icon*/
      background[chky][chkx]=blankfloor;
      PlaySound(ITEMSND);
    }
    return true;       /*everything but player just walks over it*/
  }

/*scroll*/
  if (chkspot==163)
  {
    if (obj.class==player)
    {
      givescroll();
      view[chky][chkx]=blankfloor;  /*erase icon*/
      background[chky][chkx]=blankfloor;
      PlaySound(ITEMSND);
    }
    return true;       /*everything but player just walks over it*/
  }

/* key */
  if (chkspot==164)
  {
    if (obj.class==player)
    {
      givekey();
      view[chky][chkx]=blankfloor;  /*erase icon*/
      background[chky][chkx]=blankfloor;
      PlaySound(ITEMSND);
    }
    return true;       /*everything but player just walks over it*/
  }


/* doors */
   if (chkspot==165 || chkspot==166)
   {
    if (obj.class==player)
      {
	if (takekey())
	{
	  opendoor();
	  return true;
	}
      }
     return false;       /*blocks everything else*/
   }

/* treasure chest */

   if (chkspot==167)
   {
     if (obj.class==player)
     {
       score+=500;
       printscore();
       background[chky][chkx]=blankfloor;
       view[chky][chkx]=blankfloor;
       PlaySound(TREASURESND);
     }
     return true;       /*everything but player just walks over it*/
   }

/* blowing up walls */

  if (chkspot>=29 && chkspot<=31)
    return true;

  return false;
}


/*==========================================*/
/*                                          */
/* walk                                     */
/* tries to move the object forward.  if it */
/* touches another object, contact will be  */
/* resolved based on contact.  returns a    */
/* true / false whether the move is ok      */
/*                                          */
/*==========================================*/

boolean walk (void)

{
  int i,size,newx,newy,deltay,deltax;
  boolean try;


  switch (obj.dir)
    {
    case north: {
	     newx=obj.x;
	     newy=obj.y-1;
	     chkx=newx;
	     chky=newy;
	     deltax=1;
	     deltay=0;
       break;
	   }
    case east : {
	     newx=obj.x+1;
	     newy=obj.y;
	     chkx=obj.x+obj.size;
	     chky=newy;
	     deltax=0;
	     deltay=1;
       break;
	   }
    case south: {
	     newx=obj.x;
	     newy=obj.y+1;
	     chkx=newx;
	     chky=obj.y+obj.size;
	     deltax=1;
	     deltay=0;
       break;
	   }
    case west : {
	     newx=obj.x-1;
	     newy=obj.y;
	     chkx=newx;
	     chky=newy;
	     deltax=0;
	     deltay=1;
       break;
	   }
    default: return (false);   /*should never happen*/
  }

  for (i=1;i<=obj.size;i++)
    {
      chkspot=view[chky][chkx];
      if (chkspot!=blankfloor)
      {
	try=walkthrough();
        if (leveldone)      /*player hit the teleporter*/
	  return(true);
	if (obj.stage==2)    /*if they attacked something, its good*/
	  return(true);
	if (!try)    /*ran into something that's not ok*/
	  return(false);
      }
      chkx=chkx+deltax;
      chky=chky+deltay;
    }

  obj.x=newx;
  obj.y=newy;
  obj.stage ^= 1;       /*toggle walking frame*/
  return (true);
}


/****************************************************************************/

/*================*/
/*                */
/* playercmdthink */
/*                */
/*================*/

void playercmdthink(void)

{
  dirtype olddir;
  ControlStruct c;

  c=ControlPlayer (1);	/*see what the input device is doing*/
  obj.stage=obj.stage & 1;   /*cancle attack or damaged stage*/


/* cheat key... */

  if (c.button1 && c.button2 && keydown[0x10])   /*'q' + b1 + b2*/
    {
      givepotion();
      givescroll();
      givekey();
    }


/*                      */
/* carry out the action */
/*                      */

  if (c.dir<nodir && frameon%2)
  {
    if (c.button2)       /*if button 2 is down, the move will not*/
      olddir=obj.dir;   /*change the direction of the figure (strafe)*/

    if (c.dir>west)
    {
      if ((frameon/2)%2)
	switch (c.dir)
	{
	  case northeast:
	    obj.dir = east;
	    walk();
	    c.dir = north;
	    break;
	  case southeast:
		 obj.dir = south;
	    walk();
	    c.dir = east;
	    break;
	  case southwest:
		 obj.dir = west;
	    walk();
	    c.dir = south;
	    break;
	  case northwest:
	    obj.dir = north;
	    walk();
	    c.dir = west;
	    break;
	}
      else
	switch (c.dir)
	{
	  case northeast:
	    obj.dir = north;
	    walk();
	    c.dir = east;
	    break;
	  case southeast:
	    obj.dir = east;
	    walk();
	    c.dir = south;
	    break;
	  case southwest:
	    obj.dir = south;
	    walk();
	    c.dir = west;
	    break;
	  case northwest:
	    obj.dir = west;
		 walk();
	    c.dir = north;
	    break;
	}
    }

    obj.dir=c.dir;        /*set new direction*/
    if (!walk())
      PlaySound (BLOCKEDSND);
    if (c.button2)
      obj.dir=olddir;   /*restore original direction*/
  }
  else
    if (!c.button2)
      switch (c.dir)
      {
	case northwest:
	case north:
	  obj.dir = north;
	  break;
	case northeast:
	case east:
	  obj.dir = east;
	  break;
	case southeast:
	case south:
	  obj.dir = south;
	  break;
	case southwest:
	case west:
	  obj.dir = west;
	  break;
      }

/* move screen origin */

  originx = obj.x-11;
  originy = obj.y-11;

/* a bolt spell is still going off */

  if (boltsleft>0)
  {
    if (frameon % 3==0)
    {
      playbigshoot();   /*let off a bigshot*/
      boltsleft--;
    }
  }
  else

/* button 1 builds shot power / shoots */

  {
    if (c.button1)
	 {
      if (shotpower==0)
	shotpower=1;     /*give power one immediately*/
      else if (shotpower<13 && frameon%2)  /*give extra's only 2 frames*/
	shotpower++;
      printshotpower();
    }
    else if (shotpower>0)    /*player just released the fire button*/
	  {
      if (shotpower==13)
	playbigshoot();
	    else
	      playshoot();
	    shotpower=0;
	    printshotpower();
	  }

  }

/*                                       */
/* keys to cast spells and drink potions */
/*                                       */

  if (!indemo)
  {
    if (keydown [0x19] || keydown [0x39])     /*'p' or ' ' keys*/
    {
      if (obj.hp<13)  /*don't take a potion if not needed*/
      {
	takepotion();
	keydown [0x19]=false;
	keydown [0x39]=false;
      }
    }
    else if (keydown [0x30])        /*'b' key*/
	  {
	    castbolt();
	    keydown [0x30]=false;
	  }
    else if (keydown [0x31] || keydown [0x1c])   /*'n' or ret keys*/
    {
      castnuke();
      keydown [0x31]=false;
      keydown [0x1c]=false;
    }
  }

  dofkeys ();
  if (resetgame)
  {
	 resetgame = false;
    playdone = true;
    return;
  }

  switch (indemo)
  {
	 case notdemo:
		if (keydown[0x2e] && keydown[0x14] && keydown[0x39])	//'C-T-SPC'
		{
	centerwindow (16,2);
	print ("warp to which\nlevel (1-99)?");
	clearkeys ();
	level =_inputint ();
	if (level<1)
	  level=1;
	if (level>30)
	  level=30;
	restore();
	leveldone = true;
		}


//NOLAN ADDED
	if (keydown[0x2e] && keydown[0x14] && keydown[0x22])	// c-t-TAB == GODMODE
	{
		int i;
		if (GODMODE)
		{
			centerwindow (13,1);
			print("God Mode Off");
			GODMODE = false;
		}
		else
		{
			centerwindow (12,1);
			print("God Mode On");
			GODMODE = true;
		}

		clearkeys();
		i = getch ();

		restore();
		clearkeys();
	}
//NOLAN END

		break;

    case demoplay:
    //
    // check for player really hitting a button or space to start a game
    //
      indemo = notdemo;
      ctrl = ControlPlayer (1);
      if (ctrl.button1 || ctrl.button2 || keydown[0x39])
      {
	indemo = demoplay;
	exitdemo = true;
	leveldone = true;
	level = 0;
	return;
      }
      indemo = demoplay;
      break;

  }
}


/*===============================================*/
/*					         */
/* chasethink                                    */
/* have the current monster go after the player, */
/* either diagonally or straight on              */
/*					         */
/*===============================================*/

void chasethink (boolean diagonal)

{
  int deltax,deltay,i;
  dirtype d[3];
  dirtype tdir, olddir, turnaround;

  obj.stage=obj.stage & 1;   /*cancle attack or damaged stage*/
  olddir=obj.dir;
  turnaround=opposite[olddir];

  deltax=o[0].x-obj.x;
  deltay=o[0].y-obj.y;

    d[1]=nodir;
    d[2]=nodir;

  if (deltax>0)
    d[1]= east;
  if (deltax<0)
    d[1]= west;
  if (deltay>0)
    d[2]=south;
  if (deltay<0)
    d[2]=north;

  if (abs(deltay)>abs(deltax))
    {
      tdir=d[1];
      d[1]=d[2];
      d[2]=tdir;
    }

  if (d[1]==turnaround)
    d[1]=nodir;
  if (d[2]==turnaround)
    d[2]=nodir;


  if (diagonal)
  {                           /*ramdiagonals try the best dir first*/
    if (d[1]!=nodir)
      {
        obj.dir=d[1];
	if (walk() || (obj.stage==3))
          return;     /*either moved forward or attacked*/
      }

  if (d[2]!=nodir)
    {
      obj.dir=d[2];
      if (walk() || (obj.stage==3))
        return;
    }
  }
  else
  {                  /*ramstraights try the second best dir first*/

  if (d[2]!=nodir)
    {
      obj.dir=d[2];
      if (walk() || (obj.stage==3))
        return;
    }

  if (d[1]!=nodir)
    {
      obj.dir=d[1];
      if (walk() || (obj.stage==3))
        return;
    }
  }

/* there is no direct path to the player, so pick another direction */

  obj.dir=olddir;
  if (walk() || (obj.stage==3))
    return;

  if (rndt()>128) 	/*randomly determine direction of search*/
    {
      for (tdir=north;tdir<=west;tdir++)
      {
        if (tdir!=turnaround)
        {
          obj.dir=tdir;
	  if (walk() || (obj.stage==3))
            return;
        }
      }
    }
    else
    {
      for (tdir=west;tdir>=north;tdir--)
      {
        if (tdir!=turnaround)
        {
          obj.dir=tdir;
	  if (walk() || (obj.stage==3))
            return;
        }
      }
    }

  obj.dir=turnaround;
  walk();		/*last chance, don't worry about returned value*/
}


/*===========*/
/*           */
/* gargthink */
/*           */
/*===========*/

void gargthink(void)

{
  int n,deltax,deltay;

  if (rndt ()>220)   /*only shoot once in a while*/
  {
    obj.stage=2;
    obj.delay=6;

/*soundon=false;*/

    PlaySound (SHOTSND);
    n=newobject();

// with o[n] do  /*get a free spot*/

    o[n].class=rock;
    o[n].delay=0;
    o[n].stage=0;
    o[n].active=true;
    o[n].dir=obj.dir;  /*missile is allways facing same way as thrower*/

    switch (obj.dir)
    {
      case north: {
         o[n].x=obj.x+1+side;
         o[n].y=obj.y;
         break;
		   }
      case east:  {
         o[n].x=obj.x+3;
         o[n].y=obj.y+1+side;
         break;
		   }
      case south: {
         o[n].x=obj.x+1+side;
         o[n].y=obj.y+3;
         break;
		   }
      case west:  {
         o[n].x=obj.x;
         o[n].y=obj.y+1+side;
         break;
		   }
    }
    return;
  }
  else
    chasethink (false);   /*otherwise chase straight*/
}


/*=============*/
/*             */
/* dragonthink */
/*             */
/*=============*/

void dragonthink(void)

{
  int n;

  if (rndt()>220) 	/*only shoot once in a while*/
    {
      obj.stage=2;
      obj.delay=6;

/*soundon=false;*/

      PlaySound (SHOTSND);
      n=newobject();

// with o[n]. do  /*get a free spot*/

    o[n].class=bigshot;
    o[n].delay=0;
    o[n].stage=0;
    o[n].active=true;
    o[n].dir=obj.dir;  /*missile is allways facing same way as thrower*/

    switch (o[n].dir)
    {
      case north: {
         o[n].x=obj.x+1+side;
         o[n].y=obj.y;
         break;
		   }
      case east:  {
         o[n].x=obj.x+3;
         o[n].y=obj.y+1+side;
         break;
		   }
      case south: {
         o[n].x=obj.x+1+side;
         o[n].y=obj.y+3;
         break;
		   }
      case west:  {
         o[n].x=obj.x;
         o[n].y=obj.y+1+side;
         break;
		   }
	  }
    return;
  }
  else
    chasethink (false);   /*otherwise chase straight*/
}


/*==========*/
/*          */
/* gunthink */
/*          */
/*==========*/

void gunthink(int dir)
{
  int n;

  PlaySound (SHOTSND);
  obj.stage=0;

  n=newobject();

// with o[n]. do  /*get a free spot*/

  o[n].class=bigshot;
  o[n].delay=0;
  o[n].stage=0;
  o[n].active=true;
  o[n].dir=dir;  /*missile is allways facing same way as thrower*/
  o[n].x=obj.x;
  o[n].y=obj.y;
}



/*==============*/
/*              */
/* shooterthink */
/*              */
/*==============*/

void shooterthink(void)

{
  if ((obj.x<originx-1) || (obj.y<originy-1)
  || (obj.x>originx+22) || (obj.y>originy+22)
  || !walk() || (obj.stage==2))
    {
      obj.class=nothing;
    }
}


/*===========*/
/*           */
/* idlethink */
/*           */
/*===========*/

void idlethink(void)

{
  obj.stage++;
  obj.delay=2;
  if (obj.stage==obj.stages)
    obj.stage=0;
}


/*===========*/
/*           */
/* fadethink */
/*           */
/*===========*/

void fadethink(void)

{
  obj.stage++;
  obj.delay=2;
  if (obj.stage==obj.stages)
    {
      obj.class=nothing;
    }
}


/*==============*/
/*              */
/* explodethink */
/*              */
/*==============*/

void killnear(int chkx,int chky)

{
  int spot,new;

  spot=background[chky][chkx];

  if (spot<136 || spot>145)     /* not an exploding wall */
    return;

  PlaySound (TAGWALLSND);
  if (spot<143)
    background[chky][chkx]=blankfloor;
  else
    background[chky][chkx]=spot+19;  /*hidden potion,scroll,key*/

  /*make an explosion over the wall*/

// with o[newobject]. do

  new = newobject ();

  o[new].active=true;
  o[new].x=chkx;
  o[new].y=chky;
  o[new].stage=0;
  o[new].delay=2;
  o[new].class=dead1;
}


void explodethink(void)

{
  obj.stage++;
  if (obj.stage==1)    /*on first stage do a recursive flood explosion*/
  {
    killnear(obj.x-1,obj.y);
    killnear(obj.x,obj.y-1);
    killnear(obj.x+1,obj.y);
    killnear(obj.x,obj.y+1);
  }
  obj.delay=2;
  if (obj.stage==obj.stages)
    {
      obj.class=nothing;
    }
}


/*========================================*/
/*                                        */
/* think                                  */
/* decides what the object is going to do */
/* and does it. the object will definately*/
/* be redrawn, if nothing else            */
/*                                        */
/*========================================*/

void think(void)

{
  if (obj.delay>0)
    obj.delay--;      /*if the object has a delay, it will do nothing*/
  else
    {
      if (rndt () < obj.speed)
      {
        switch (obj.think)
        {
	  case playercmd : playercmdthink(); break;
	  case ramstraight : chasethink (false); break;
	  case ramdiag : chasethink (true); break;
	  case gargcmd: gargthink(); break;
	  case dragoncmd: dragonthink(); break;
	  case straight : shooterthink(); break;
	  case idle : idlethink(); break;
	  case fade : fadethink(); break;
	  case explode : explodethink(); break;
	  case gunthinke : gunthink(west); break;
	  case gunthinks : gunthink(north); break;
        }
      }
    }
}


/*==========*/
/*          */
/* doactive */
/*          */
/*==========*/

void doactive(void)

{
/*see if it is way off screen, so deactivate*/

  if ((obj.class!=dead1) && ( (obj.x<originx-10) || (obj.x>originx+34)
  || (obj.y<originy-10) || (obj.y>originy+34)))
    {
      o[objecton].active=false;
    }
  else
    {
      think();
      eraseobj();
      if (playdone)
        return;

   /*redraw it even if it hasn't moved, in case it was hit*/

      if (obj.class>nothing)
	drawobj();

      /*write the temporary info back into the array*/

      memcpy(&o[objecton],&obj,sizeof(o[objecton]) );
    }
}

/*============*/
/*            */
/* doinactive */
/*            */
/*============*/

void doinactive()
{
  /*if the object just became visable, make it active*/

  if ((obj.x+obj.size>=originx) && (obj.x<originx+24)
   && (obj.y+obj.size>=originy) && (obj.y<originy+24))
    {
      obj.active=true;
      obj.dir=north;

      /*write the temporary info back into the array*/

      memcpy (&o[objecton],&obj.active,sizeof(o[objecton]) );
    }
}


/*======================================*/
/*				      */
/* playloop                             */
/* all the action is directed from here */
/*				      */
/*======================================*/

void playloop(void)

{
  int i,j;

  screencenterx = 11;

  do {   /*until playdone*/

    if (!indemo)
    {
      centerwindow (11,2);
      print (" Entering\nlevel ");
      printint (level);
      print ("...");
      PlaySound (LEVELDONESND);
      WaitEndSound ();
    }

    clearold();    /*don't refresh the window yet*/

    loadlevel(); /*load the level to play*/
    leveldone= false;

    if (keydown[0x41] && keydown[0x20]) // 'D+F7' to record a demo
    {
      clearold ();
      refresh ();
      refresh ();
      clearkeys ();
      centerwindow (12,1);
      print ("RECORD DEMO");
      do
      {
	ch=get ();
      } while (ch!=13);
      RecordDemo ();
      clearold ();
      clearkeys ();
    }

    playdone=false;
    frameon=0;
    boltsleft=0;
    shotpower=0;
    initrndt (false);
    printshotpower();


    doall();

    if (indemo == recording)
    {
      clearkeys ();
      centerwindow (15,1);
      print ("SAVE AS DEMO#:");
      do
      {
	ch = get ();
      } while (ch<'0' || ch>'9');
      SaveDemo (ch-'0');
      clearold ();
      refresh ();
      refresh ();
    }


    if (indemo)
      playdone=true;

  } while (!playdone);

}
