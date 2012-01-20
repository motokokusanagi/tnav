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

static int map_id;

static void
get_line(struct map_rect_priv *mr)
{
	//recode  to gettig from text array
	// lastlen used for pipes?
	if(mr->f) {
		if (!mr->m->is_pipe) 
			mr->pos=ftell(mr->f);
		else
			mr->pos+=mr->lastlen;
		fgets(mr->line, SIZE, mr->f);
		mr->lastlen=strlen(mr->line)+1;
		if (strlen(mr->line) >= SIZE-1) 
			printf("line too long\n");
	        dbg(1,"read traffic line: %s\n", mr->line);
	}
}
//struct TraffCoord *traf;
//void query(struct TraffCoord *traf,int *count);
//int  ParseJsonData (struct TraffCoord *TraffData, char * strJson);

static void
map_destroy_traffic(struct map_priv *m)
{
	dbg(1,"map_destroy_traffic\n");
	g_free(m->filename);
	
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
parse_line(struct map_rect_priv *mr, int attr)
{
	int pos;
	// give line with coords
	pos=coord_parse(mr->line, projection_mg, &mr->c);
	if (pos < strlen(mr->line) && attr) {
		strcpy(mr->attrs, mr->line+pos);
	}
	return pos;
}

static int
traffic_coord_get(void *priv_data, struct coord *c, int count)
{
  dbg(1,"Count is %d\n",count);
	struct map_rect_priv *mr=priv_data;
	int ret=0;
	dbg(1,"traffic_coord_get %d\n",count);
	while (count--) {
		if (mr->f && !feof(mr->f) && (!mr->item.id_hi || !mr->eoc) && parse_line(mr, mr->item.id_hi)) {
			*c=mr->c;
			dbg(1,"c=0x%x,0x%x\n", c->x, c->y);
			c++;
			ret++;		
			get_line(mr);
			if (mr->item.id_hi)
				mr->eoc=1;
		} else {
			mr->more=0;
			break;

	}
	return ret;
}

static void
traffic_attr_rewind(void *priv_data)
{
	struct map_rect_priv *mr=priv_data;
	mr->attr_pos=0;
	mr->attr_last=attr_none;
}

static void
traffic_encode_attr(char *attr_val, enum attr_type attr_type, struct attr *attr)
{
	if (attr_type >= attr_type_int_begin && attr_type <= attr_type_int_end) 
		attr->u.num=atoi(attr_val);
	else
		attr->u.str=attr_val;
}

static int
traffic_attr_get(void *priv_data, enum attr_type attr_type, struct attr *attr)
{	
	struct map_rect_priv *mr=priv_data;
	char *str=NULL;
	dbg(1,"traffic_attr_get mr=%p attrs='%s' ", mr, mr->attrs);
	if (attr_type != mr->attr_last) {
		dbg(1,"reset attr_pos\n");
		mr->attr_pos=0;
		mr->attr_last=attr_type;
	}
	if (attr_type == attr_any) {
		dbg(1,"attr_any");
		if (attr_from_line(mr->attrs,NULL,&mr->attr_pos,mr->attr, mr->attr_name)) {
			attr_type=attr_from_name(mr->attr_name);
			dbg(1,"found attr '%s' 0x%x\n", mr->attr_name, attr_type);
			attr->type=attr_type;
			traffic_encode_attr(mr->attr, attr_type, attr);
			return 1;
		}
	} else {
		str=attr_to_name(attr_type);
		dbg(1,"attr='%s' ",str);
		if (attr_from_line(mr->attrs,str,&mr->attr_pos,mr->attr, NULL)) {
			traffic_encode_attr(mr->attr, attr_type, attr);
			dbg(1,"found\n");
			return 1;
		}
	}
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
	
//	if (map->is_pipe) {
//#ifdef HAVE_POPEN
//
//		char *oargs,*args=g_strdup(map->filename),*sep=" ";
//		enum layer_type lay;
//		g_free(mr->args);
//		while (sel) {
//			oargs=args;
//			args=g_strdup_printf("%s 0x%x 0x%x 0x%x 0x%x", oargs, sel->u.c_rect.lu.x, sel->u.c_rect.lu.y, sel->u.c_rect.rl.x, sel->u.c_rect.rl.y);
//			g_free(oargs);
//			for (lay=layer_town ; lay < layer_end ; lay++) {
//				oargs=args;
//				args=g_strdup_printf("%s%s%d", oargs, sep, sel->order);
//				g_free(oargs);
//				sep=",";
//			}
//			sel=sel->next;
//		}
//		dbg(1,"popen args %s\n", args);
//		mr->args=args;
//		mr->f=popen(mr->args, "r");
//		mr->pos=0;
//		mr->lastlen=0;
//#else
//		dbg(0,"map_rect_new_traffic is unable to work with pipes %s\n",map->filename);
//#endif
//
//
//	} else {
//		mr->f=fopen(map->filename, "r");
//	}
//	if(!mr->f) {
//		printf("map_rect_new_traffic unable to open traffic %s. Error: %s\n",map->filename, strerror(errno));
//	}
//	get_line(mr);
	return mr;
}


static void
map_rect_destroy_traffic(struct map_rect_priv *mr)
{
//	if (mr->f) {
//		if (mr->m->is_pipe) {
//#ifdef HAVE_POPEN
//			pclose(mr->f);
//#endif
//		}
//		else {
//			fclose(mr->f);
//		}
//	}
	g_list_free (mr->traffic_list);
        g_free(mr);
}

static struct item *
map_rect_get_item_traffic(struct map_rect_priv *mr)
{
//	printf("\n%p",&mr->item);
//	char *p,type[SIZE];
//	dbg(1,"map_rect_get_item_traffic id_hi=%d line=%s\n", mr->item.id_hi, mr->line);
//	if (!mr->f) {
//		return NULL;
//	}
//	if (mr->more != 0) printf("\n%d",mr->more);
//	//more is special element for i
//	//while (mr->more) {
//		//struct coord c;
//		//traffic_coord_get(mr, &c, 1);
//	//}
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
//			if (mr->m->is_pipe) {
//#ifdef HAVE_POPEN
//				pclose(mr->f);
//				mr->f=popen(mr->args, "r");
//				mr->pos=0;
//				mr->lastlen=0;
//#endif
//			} else {
//				fseek(mr->f, 0, SEEK_SET);
//				clearerr(mr->f);
//			}
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
//			dbg(1,"map_rect_get_item_traffic: point found\n");
//			mr->eoc=0;
//			mr->item.id_lo=mr->pos;
//		} else {
//			if (parse_line(mr, 1)) {
//				get_line(mr);
//				continue;
//			}
//			dbg(1,"map_rect_get_item_traffic: line found\n");
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
//			dbg(1,"type='%s'\n", type);
//			mr->item.type=item_from_name(type);
//			if (mr->item.type == type_none)
//				printf("Warning: type '%s' unknown\n", type);
//		} else {
//			get_line(mr);
//			continue;
//		}
//		mr->attr_last=attr_none;
//		mr->more=1;
//		dbg(1,"return attr='%s'\n", mr->attrs);
		return &mr->item;
	}
}

static struct item *
map_rect_get_item_byid_traffic(struct map_rect_priv *mr, int id_hi, int id_lo)
{
//	if (mr->m->is_pipe) {
//#ifndef _MSC_VER
//		pclose(mr->f);
//		mr->f=popen(mr->args, "r");
//		mr->pos=0;
//		mr->lastlen=0;
//#endif /* _MSC_VER */
//	} else
//		fseek(mr->f, id_lo, SEEK_SET);
//	get_line(mr);
//	mr->item.id_hi=id_hi;
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
void query_stub(GList *traffic_list)
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

}
#else
void query_real(GList *traffic_list)
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
