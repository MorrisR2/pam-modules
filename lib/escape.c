/* This file is part of pam-modules.
   Copyright (C) 2009 Sergey Poznyakoff
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3 of the License, or (at your
   option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License along
   with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <graypam.h>

void
gray_escape_string (gray_slist_t slist, const char *str, size_t len)
{
	const unsigned char *p;
#define ESCAPABLE_CHAR "\\'\""
#define FLUSH() \
 gray_slist_append(slist, str, p - (const unsigned char *)str); \
 str = p;
	
	for (p = (const unsigned char *) str; p < str + len; p++) {
		if (strchr(ESCAPABLE_CHAR, *p)) {
			FLUSH();
			str++;
			gray_slist_append_char(slist, '\\');
			gray_slist_append_char(slist, *p);
		}
	}
	FLUSH();
}


