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
// What the DOS platform layer needs to know to run The Catacomb.
//

#include "PCRLIB.H"

#include "dos/app.hpp"

void dos_main (void);		// main() in CATACOMB.CPP

static const char *const requiredfiles[] =
{
  "EGAPICS.CA2",
  "EGACHARS.CA2",
  "SOUNDS.CA2",
  "LEVEL1.CA2",
  nullptr
};

extern const dos::AppInfo dos_app_info =
{
  "The Catacomb",		// window title
  "Catacomb",			// write directory: <pref path>/Catacomb/The Catacomb
  "The Catacomb",
  requiredfiles,
  [] { dos_main (); },
  [] { _quit (""); },		// window closed: quit like F10, saving scores and settings
};
