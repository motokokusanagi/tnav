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



#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include "config.h"
#include "debug.h"
#include "plugin.h"
#include "projection.h"
#include "item.h"
#include "map.h"
#include "maptype.h"
#include "attr.h"
#include "transform.h"
#include "file.h"
//#include <dbus/dbus.h>
#include "traffic.h"

void
transformation_to_geo (struct coord_geo *g, struct coord *c)
{
	g->lng=c->x/6371000.0/M_PI*180;
	g->lat=navit_atan(exp(c->y/6371000.0))/M_PI*360-90;
}

static int map_id;

static void
map_destroy_traffic(struct map_priv *m)
{
	dbg(1,"map_destroy_traffic\n");
	if(m->charset) {
		g_free(m->charset);
	}
	g_free(m);
}

static void
traffic_coord_rewind(void *priv_data)
{
}


static int
traffic_coord_get(void *priv_data, struct coord *c, int count)
{
	dbg(1,"Count is %d\n",count);
	struct map_rect_priv *mr=priv_data;
	int ret=0;
	dbg(1,"traffic_coord_get %d\n",count);
	traffic_item *r = (traffic_item*)mr->traffic_list->prev->data;
	if(count==1)
		c[0] = r->coords[mr->coord_flag];
	if(mr->coord_flag<2) {
		ret = 1;
	} else {
		ret = 0;
	}
	mr->coord_flag++;

	return ret;
}

static void
traffic_attr_rewind(void *priv_data)
{
	struct map_rect_priv *mr=priv_data;
	mr->attr_pos=0;
	mr->attr_last=attr_none;
}


static int
traffic_attr_get(void *priv_data, enum attr_type attr_type, struct attr *attr)
{	
	struct map_rect_priv *mr=priv_data;
	char *str=NULL;
	dbg(1,"not found\n");
	return 0;
}

static struct item_methods methods_traffic = {
        traffic_coord_rewind,
        traffic_coord_get,
        traffic_attr_rewind,
        traffic_attr_get,
};

static struct map_rect_priv *
map_rect_new_traffic(struct map_priv *map, struct map_selection *sel)
{
	struct map_rect_priv *mr;
	dbg(1,"map_rect_new_traffic\n");
	mr=g_new0(struct map_rect_priv, 1);
	mr->m=map;
	mr->sel=sel;
	if (map->flags & 1)
		mr->item.id_hi=1; // seems to be must statment
	else
		mr->item.id_hi=0;
	mr->item.id_lo=0;
	mr->item.meth=&methods_traffic; // too 
	mr->item.priv_data=mr; //too
	mr->traffic_list=NULL;
	query (mr->traffic_list);
	mr->traffic_first = g_list_first(mr->traffic_list);
	return mr;
}


static void
map_rect_destroy_traffic(struct map_rect_priv *mr)
{










	g_list_free (mr->traffic_list);
    g_free(mr);
}

static struct item *
map_rect_get_item_traffic(struct map_rect_priv *mr)
{

	if(mr->traffic_list ) {
		traffic_item* iterator = (traffic_item*)mr->traffic_list->data;
		mr->traffic_list = g_list_next(mr->traffic_list);
		mr->item.type = item_from_name("street_traffic");
		mr->coord_flag = 0;
		return &mr->item;
	}else {
		mr->traffic_list = g_list_first(mr->traffic_list);
		return NULL;
	}

//	for(;;) {
//		if (feof(mr->f)) {
//			dbg(1,"map_rect_get_item_traffic: eof %d\n",mr->item.id_hi);
//			if (mr->m->flags & 1) {
//				if (!mr->item.id_hi)
//					return NULL;
//				mr->item.id_hi=0;
//			} else {
//				if (mr->item.id_hi)
//					return NULL;
//				mr->item.id_hi=1;
//			}

//				fseek(mr->f, 0, SEEK_SET);
//				clearerr(mr->f);

//			get_line(mr);
//		}
//		if ((p=strchr(mr->line,'\n')))
//			*p='\0';
//		if (mr->item.id_hi) {
//			mr->attrs[0]='\0';
//			if (!parse_line(mr, 1)) {
//				get_line(mr);
//				continue;
//			}

//			mr->eoc=0;
//			mr->item.id_lo=mr->pos;
//		} else {
//			if (parse_line(mr, 1)) {
//				get_line(mr);
//				continue;
//			}

//			if (! mr->line[0]) {
//				get_line(mr);
//				continue;
//			}
//			mr->item.id_lo=mr->pos;
//			strcpy(mr->attrs, mr->line);
//			get_line(mr);
//			dbg(1,"mr=%p attrs=%s\n", mr, mr->attrs);
//		}
//		dbg(1,"get_attrs %s\n", mr->attrs);
//		if (attr_from_line(mr->attrs,"type",NULL,type,NULL)) {

//			mr->item.type=item_from_name(type);
//			if (mr->item.type == type_none)
//				printf("Warning: type '%s' unknown\n", type);
//		} else {
//			get_line(mr);
//			continue;
//		}
//		mr->attr_last=attr_none;
//		mr->more=1;

		return &mr->item;
	}
}

static struct item *
map_rect_get_item_byid_traffic(struct map_rect_priv *mr, int id_hi, int id_lo)
{
	return map_rect_get_item_traffic(mr);
}

static struct map_methods map_methods_traffic = {
	projection_mg,
	"iso8859-1",
	map_destroy_traffic,
	map_rect_new_traffic,
	map_rect_destroy_traffic,
	map_rect_get_item_traffic,
	map_rect_get_item_byid_traffic,
};

// initializes new map instance and returns map methods struct to navit
static struct map_priv *
map_new_traffic(struct map_methods *meth, struct attr **attrs, struct callback_list *cbl)
{
	struct map_priv *m;
	struct attr *data=attr_search(attrs, NULL, attr_data); // look for data 
	struct attr *charset=attr_search(attrs, NULL, attr_charset); // look for charset
	struct attr *flags=attr_search(attrs, NULL, attr_flags); // we can configure plugin via flags

	//if (! data)
	//	return NULL;
	// TODO: do something with data
	*meth=map_methods_traffic;
	m=g_new0(struct map_priv, 1);
	m->id=++map_id;

	if (flags)  
		m->flags=flags->u.num; // 

	if (charset) {
		m->charset=g_strdup(charset->u.str);
		meth->charset=m->charset;
	}
	return m;
}

/*
int  ParseJsonData (struct TraffCoord *TraffData, char * strJson)
{
	struct json_object *JsonData;
	struct json_object *JsonTraffCoordData, *JsonTraffCoord, *JsonTmp;
	int Length, i;

	JsonData = json_tokener_parse(strJson);

	JsonTraffCoordData = json_object_object_get(JsonData, "TraffCoordData");
	Length = json_object_array_length(JsonTraffCoordData);

	for (i=0; i<Length; i++)
	{
		JsonTraffCoord = json_object_array_get_idx(JsonTraffCoordData, i);
		JsonTmp = json_object_object_get(JsonTraffCoord, "FirstCoord");
		TraffData[i].FirstCoord = json_object_get_double(JsonTmp);
		JsonTmp = json_object_object_get(JsonTraffCoord, "FirstDirection");
		TraffData[i].Direction[0] = *json_object_get_string(JsonTmp);
		JsonTmp = json_object_object_get(JsonTraffCoord, "SecondCoord");
		TraffData[i].SecondCoord = json_object_get_double(JsonTmp);
		JsonTmp = json_object_object_get(JsonTraffCoord, "SecondDirection");
		TraffData[i].Direction[1] = *json_object_get_string(JsonTmp);
		JsonTmp = json_object_object_get(JsonTraffCoord, "ThirdCoord");
		TraffData[i].ThirdCoord = json_object_get_double(JsonTmp);
		JsonTmp = json_object_object_get(JsonTraffCoord, "ThirdDirection");
		TraffData[i].Direction[2] = *json_object_get_string(JsonTmp);
		JsonTmp = json_object_object_get(JsonTraffCoord, "FourthCoord");
		TraffData[i].FourthCoord = json_object_get_double(JsonTmp);
		JsonTmp = json_object_object_get(JsonTraffCoord, "FourthDirection");
		TraffData[i].Direction[3] = *json_object_get_string(JsonTmp);
		JsonTmp = json_object_object_get(JsonTraffCoord, "MaxSpeed");
		TraffData[i].MaxSpeed = json_object_get_int(JsonTmp);
	}
	return Length;
}
*/


#define debug1

#ifdef debug1
void query(GList *traffic_list)
{
	traffic_item item_1 = (struct traffic_item *)malloc(sizeof(struct traffic_item));
	item_1.coords[0].x=transform_from_geo(46.4978);
	item_1.coords[0].y=transform_from_geo(30.6277);

    item_1.speed=0.0;
    traffic_item item_2 = (struct traffic_item *)malloc(sizeof(struct traffic_item));
    item_2.coords[1].x=transform_from_geo(46.3986);
    item_2.coords[1].y=transform_from_geo(30.7716);

    item_2.speed=0.0;
    traffic_list = g_list_append (traffic_list, item_1);
    traffic_list = g_list_append (traffic_list, item_2);


}
#else
void query(GList *traffic_list)
{
	traffic_item item_1 = (struct traffic_item *)malloc(sizeof(struct traffic_item));
	item_1.x1=4629.868000;
	item_1.y1=3037.662000;
	item_1.x2=4624.916000;
	item_1.y2=3046.296000;
	item_1.sn1='N';
	item_1.sn2='N';
    item_1.ew1='E';
    item_1.ew2='E';
    item_1.speed=0.0;

    traffic_item item_2 = (struct traffic_item *)malloc(sizeof(struct traffic_item));
    item_2.x2=4623.916000;
    item_2.y2=3046.296000;
    item_2.x1=4624.916000;
    item_2.y1=3046.296000;
   	item_2.sn1='N';
    item_2.sn2='N';
    item_2.ew1='E';
    item_2.ew2='E';
    item_2.speed=0.0;

    traffic_list = g_list_append (traffic_list, item_1);
    traffic_list = g_list_append (traffic_list, item_2);

//   DBusMessage* msg;
//   DBusMessageIter args;
//   DBusConnection* conn;
//   DBusError err;
//   DBusPendingCall* pending;
//   int ret;
//   char *stat;
//   char *param = "get";
//   //printf("Calling remote method with %s\n", param);
//   //initialiset the errors
//   dbus_error_init(&err);
//   //connect to the system bus and check for errors
//   conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
//   if (dbus_error_is_set(&err)) {
//      fprintf(stderr, "Connection Error (%s)\n", err.message);
//      dbus_error_free(&err);
//   }
//   if (NULL == conn) {
//      exit(1);
//   }
//
//   // request our name on the bus
//   ret = dbus_bus_request_name(conn, "traffic.method.caller", DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
//   if (dbus_error_is_set(&err)) {
//      fprintf(stderr, "Name Error (%s)\n", err.message);
//      dbus_error_free(&err);
//   }
//   if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
//      exit(1);
//   }
//
//   // create a new method call and check for errors
//   msg = dbus_message_new_method_call("traffic.method.server", // target for the method call
//                                      "/traffic/method/Object", // object to call on
//                                      "traffic.method.Type", // interface to call on
//                                      "Method"); // method name
//   if (NULL == msg) {
//      fprintf(stderr, "Message Null\n");
//      exit(1);
//   }
//
//   // append arguments
//   dbus_message_iter_init_append(msg, &args);
//   if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &param)) {
//      fprintf(stderr, "Out Of Memory!\n");
//      exit(1);
//   }
//
//   // send message and get a handle for a reply
//   if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) { // -1 is default timeout
//      fprintf(stderr, "Out Of Memory!\n");
//      exit(1);
//   }
//   if (NULL == pending) {
//      fprintf(stderr, "Pending Call Null\n");
//      exit(1);
//   }
//   dbus_connection_flush(conn);
//
//   printf("Request Sent\n");
//
//   // free message
//   dbus_message_unref(msg);
//
//   // block until we recieve a reply
//   dbus_pending_call_block(pending);
//
//   // get the reply message
//   msg = dbus_pending_call_steal_reply(pending);
//   if (NULL == msg) {
//      fprintf(stderr, "Reply Null\n");
//     // exit(1);
//      goto l;
//   }
//   // free the pending message handle
//   dbus_pending_call_unref(pending);
//   // read the parameters
//   if (!dbus_message_iter_init(msg, &args))
//      fprintf(stderr, "Message has no arguments!\n");
//   else if (DBUS_TYPE_STRING!= dbus_message_iter_get_arg_type(&args))
//      fprintf(stderr, "Argument is not string!\n");
//   else
//      dbus_message_iter_get_basic(&args, &stat);

// /// stat is json


//   //strcpy(str,stat);

//  // printf("Got Reply: %s, \n", stat);
//   l:
//   // free reply and close connection

//   dbus_message_unref(msg);
//   dbus_bus_release_name(conn,"traffic.method.caller",&err);

//   if(stat!=NULL){
//      *count = ParseJsonData(traf,stat);
//   } else {
//   }

}
#endif





void
plugin_init(void)
{
	dbg(1,"traffic: plugin_init\n");
	plugin_register_map_type("traffic", map_new_traffic);
}
