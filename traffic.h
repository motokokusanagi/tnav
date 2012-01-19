/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include "attr.h"
#include "coord.h"
#include <glib.h>

typedef struct traffic_item
{
	double x1,x2,y1,y2;
	char ew1,sn1,ew2,sn2,speed;
}traffic_item;

struct map_priv {
	int id;
	char *charset;
	int flags;
};

#define SIZE 512

struct map_rect_priv {
	struct map_selection *sel;
	GList *traffic_list;
	long pos;///
	char line[SIZE];
	int attr_pos;
	enum attr_type attr_last;
	char attrs[SIZE];
	char attr[SIZE];
	char attr_name[SIZE];
	struct coord c;
	struct map_priv *m;
	struct item item;
	char *args;
};

