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
// initialize object info
//

void initobjects()
{

// with objdef[player] do

    objdef[player].think = playercmd;
    objdef[player].contact = benign;
    objdef[player].solid = true;
    objdef[player].firstchar = tile2s;
    objdef[player].size  = 2;
    objdef[player].stages  = 4;
    objdef[player].dirmask = 3;
    objdef[player].speed = 256;
    objdef[player].hitpoints = 12;
    objdef[player].damage  = 0;
    objdef[player].points  = 0;


// with objdef[goblin]. do

    objdef[goblin].think = ramstraight;
    objdef[goblin].contact = monster;
    objdef[goblin].solid = true;
    objdef[goblin].firstchar = tile2s+64;
    objdef[goblin].size  = 2;
    objdef[goblin].stages  = 4;
    objdef[goblin].dirmask = 3;
    objdef[goblin].speed = 75;
    objdef[goblin].hitpoints = 1;
    objdef[goblin].damage  = 1;
    objdef[goblin].points  = 50;


// with objdef[skeleton]. do

    objdef[skeleton].think = ramdiag;
    objdef[skeleton].contact = monster;
    objdef[skeleton].solid = true;
    objdef[skeleton].firstchar = tile2s+128;
    objdef[skeleton].size  = 2;
    objdef[skeleton].stages  = 4;
    objdef[skeleton].dirmask = 3;
    objdef[skeleton].speed = 130;
    objdef[skeleton].hitpoints = 1;
    objdef[skeleton].damage  = 1;
    objdef[skeleton].points  = 150;


// with objdef[ogre]. do

    objdef[ogre].think = ramstraight;
    objdef[ogre].contact = monster;
    objdef[ogre].solid = true;
    objdef[ogre].firstchar = tile3s;
    objdef[ogre].size  = 3;
    objdef[ogre].stages  = 4;
    objdef[ogre].dirmask = 3;
    objdef[ogre].speed = 75;
    objdef[ogre].hitpoints = 5;
    objdef[ogre].damage  = 2;
    objdef[ogre].points  = 250;


// with objdef[gargoyle]. do

    objdef[gargoyle].think = gargcmd;
    objdef[gargoyle].contact = monster;
    objdef[gargoyle].solid = true;
    objdef[gargoyle].firstchar = tile4s;
    objdef[gargoyle].size  = 4;
    objdef[gargoyle].stages  = 4;
    objdef[gargoyle].dirmask = 3;
    objdef[gargoyle].speed = 175;
    objdef[gargoyle].hitpoints = 10;
    objdef[gargoyle].damage  = 3;
    objdef[gargoyle].points  = 500;


// with objdef[dragon]. do

    objdef[dragon].think = dragoncmd;
    objdef[dragon].contact = monster;
    objdef[dragon].solid = true;
    objdef[dragon].firstchar = tile5s;
    objdef[dragon].size  = 5;
    objdef[dragon].stages  = 4;
    objdef[dragon].dirmask = 3;
    objdef[dragon].speed = 100;
    objdef[dragon].hitpoints = 75;
    objdef[dragon].damage  = 5;
    objdef[dragon].points  = 1000;


// with objdef[turbogre]. do

    objdef[turbogre].think = ramstraight;
    objdef[turbogre].contact = monster;
    objdef[turbogre].solid = true;
    objdef[turbogre].firstchar = tile3s+19*9;
    objdef[turbogre].size  = 3;
    objdef[turbogre].stages  = 4;
    objdef[turbogre].dirmask = 3;
    objdef[turbogre].speed = 255;
    objdef[turbogre].hitpoints = 5;
    objdef[turbogre].damage  = 2;
    objdef[turbogre].points  = 500;



// with objdef[wallhit]. do

    objdef[wallhit].think = fade;
    objdef[wallhit].contact = benign;
    objdef[wallhit].solid = true;
    objdef[wallhit].firstchar = 26;
    objdef[wallhit].size  = 1;
    objdef[wallhit].stages  = 3;
    objdef[wallhit].dirmask = 0;
    objdef[wallhit].speed = 80;
    objdef[wallhit].hitpoints = 0;
    objdef[wallhit].damage  = 0;
    objdef[wallhit].points  = 0;


// with objdef[dead1]. do

    objdef[dead1].think = explode;
    objdef[dead1].contact = benign;
    objdef[dead1].solid = false;
    objdef[dead1].firstchar = 29;
    objdef[dead1].size  = 1;
    objdef[dead1].stages  = 3;
    objdef[dead1].dirmask = 0;
    objdef[dead1].speed = 80;
    objdef[dead1].hitpoints = 0;
    objdef[dead1].damage  = 0;
    objdef[dead1].points  = 0;


// with objdef[dead2]. do

    objdef[dead2].think = fade;
    objdef[dead2].contact = benign;
    objdef[dead2].solid = false;
    objdef[dead2].firstchar = tile2s+224;
    objdef[dead2].size  = 2;
    objdef[dead2].stages  = 3;
    objdef[dead2].dirmask = 0;
    objdef[dead2].speed = 80;
    objdef[dead2].hitpoints = 0;
    objdef[dead2].damage  = 0;
    objdef[dead2].points  = 0;


// with objdef[dead3]. do

    objdef[dead3].think = fade;
    objdef[dead3].contact = benign;
    objdef[dead3].solid = false;
    objdef[dead3].firstchar = tile3s + 9*16;
    objdef[dead3].size  = 3;
    objdef[dead3].stages  = 3;
    objdef[dead3].dirmask = 0;
    objdef[dead3].speed = 80;
    objdef[dead3].hitpoints = 0;
    objdef[dead3].damage  = 0;
    objdef[dead3].points  = 0;


// with objdef[dead4]. do

    objdef[dead4].think = fade;
    objdef[dead4].contact = benign;
    objdef[dead4].solid = false;
    objdef[dead4].firstchar = tile4s + 16*16;
    objdef[dead4].size  = 4;
    objdef[dead4].stages  = 3;
    objdef[dead4].dirmask = 0;
    objdef[dead4].speed = 80;
    objdef[dead4].hitpoints = 0;
    objdef[dead4].damage  = 0;
    objdef[dead4].points  = 0;


// with objdef[dead5]. do

    objdef[dead5].think = fade;
    objdef[dead5].contact = benign;
    objdef[dead5].solid = false;
    objdef[dead5].firstchar = tile5s + 25*16;
    objdef[dead5].size  = 5;
    objdef[dead5].stages  = 3;
    objdef[dead5].dirmask = 0;
    objdef[dead5].speed = 80;
    objdef[dead5].hitpoints = 0;
    objdef[dead5].damage  = 0;
    objdef[dead5].points  = 0;


// with objdef[shot]. do

    objdef[shot].think = straight;
    objdef[shot].contact = pshot;
    objdef[shot].solid = false;
    objdef[shot].firstchar = 154;
    objdef[shot].size  = 1;
    objdef[shot].stages  = 2;
    objdef[shot].dirmask = 3;
    objdef[shot].speed = 256;
    objdef[shot].hitpoints = 0;
    objdef[shot].damage  = 1;
    objdef[shot].points  = 0;


// with objdef[guns]. do

    objdef[guns].think = gunthinks;
    objdef[guns].contact = benign;
    objdef[guns].solid = true;
    objdef[guns].firstchar = tile3s-8;
    objdef[guns].size  = 2;
    objdef[guns].stages  = 1;
    objdef[guns].dirmask = 0;
    objdef[guns].speed = 10;
    objdef[guns].hitpoints = 255;
    objdef[guns].damage  = 0;
    objdef[guns].points  = 0;


// with objdef[gune]. do

    objdef[gune].think = gunthinke;
    objdef[gune].contact = benign;
    objdef[gune].solid = true;
    objdef[gune].firstchar = tile3s-4;
    objdef[gune].size  = 2;
    objdef[gune].stages  = 1;
    objdef[gune].dirmask = 0;
    objdef[gune].speed = 10;
    objdef[gune].hitpoints = 255;
    objdef[gune].damage  = 0;
    objdef[gune].points  = 0;


// with objdef[rock]. do

    objdef[rock].think = straight;
    objdef[rock].contact = mshot;
    objdef[rock].solid = false;
    objdef[rock].firstchar = 153;
    objdef[rock].size  = 1;
    objdef[rock].stages  = 2;
    objdef[rock].dirmask = 0;
    objdef[rock].speed = 256;
    objdef[rock].hitpoints = 0;
    objdef[rock].damage  = 1;
    objdef[rock].points  = 0;


// with objdef[bigshot]. do

    objdef[bigshot].think = straight;
    objdef[bigshot].contact = nukeshot;
    objdef[bigshot].solid = false;
    objdef[bigshot].firstchar = tile2s+192;
    objdef[bigshot].size  = 2;
    objdef[bigshot].stages  = 2;
    objdef[bigshot].dirmask = 3;
    objdef[bigshot].speed = 256;
    objdef[bigshot].hitpoints = 0;
    objdef[bigshot].damage  = 1;
    objdef[bigshot].points  = 0;


// with objdef[teleporter]. do

    objdef[teleporter].think = idle;
    objdef[teleporter].contact = benign;
    objdef[teleporter].solid = false;
    objdef[teleporter].firstchar = tile2s+236;
    objdef[teleporter].size  = 2;
    objdef[teleporter].stages  = 5;
    objdef[teleporter].dirmask = 0;
    objdef[teleporter].speed = 200;
    objdef[teleporter].hitpoints = 0;
    objdef[teleporter].damage  = 0;
    objdef[teleporter].points  = 0;

// with objdef[secretgate]. do

    objdef[secretgate].think = idle;
    objdef[secretgate].contact = benign;
    objdef[secretgate].solid = false;
    objdef[secretgate].firstchar = tile3s-12;
    objdef[secretgate].size  = 2;
    objdef[secretgate].stages  = 1;
    objdef[secretgate].dirmask = 0;
    objdef[secretgate].speed = 200;
    objdef[secretgate].hitpoints = 0;
    objdef[secretgate].damage  = 0;
    objdef[secretgate].points  = 0;
}
