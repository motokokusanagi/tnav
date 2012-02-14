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

//TODO: get rid  from GList in module

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
#include <dbus-1.0/dbus/dbus.h>
#include "traffic.h"
#include <json.h>

void
transformation_to_geo (struct coord_geo *g, struct coord *c)
{
	g->lng=c->x/6371000.0/M_PI*180;
	g->lat=navit_atan(exp(c->y/6371000.0))/M_PI*360-9;
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

//TODO: recode
static int
traffic_coord_get(void *priv_data, struct coord *c, int count)
{
	//dbg(0,"Count is %d\n",count);
	struct map_rect_priv *mr=priv_data;
	int ret=0;
	traffic_item *r = (traffic_item*)mr->traffic_list->prev->data;
	if(r){
		*c = r->coords[0];
		dbg (1,"%d -- %d \n", c[0].x,c[0].y);
		c++;
		*c = r->coords[1];
		ret=2;
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


static int
traffic_attr_get(void *priv_data, enum attr_type attr_type, struct attr *attr)
{
//	struct map_rect_priv *mr=priv_data;
//	char *str=NULL;
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
		mr->item.id_hi=1;
	else
		mr->item.id_hi=0;
	mr->item.id_lo=0;
	mr->item.meth=&methods_traffic;
	mr->item.priv_data=mr;
	mr->traffic_list=NULL;
	mr->sel = sel;

	query(mr);
	dbg (1,"0x%p\n",mr->traffic_list);
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

	if(mr->traffic_list && mr->traffic_list->next) {
		mr->traffic_list = g_list_next(mr->traffic_list);
		mr->item.type = item_from_name("street_traffic");
		return &mr->item;
	}else {
		mr->traffic_list = g_list_first(mr->traffic_list);
		return NULL;
	}
		return &mr->item;
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
	struct attr *data=attr_search(attrs, NULL, attr_data);
	struct attr *charset=attr_search(attrs, NULL, attr_charset);
	struct attr *flags=attr_search(attrs, NULL, attr_flags);

	if (! data)
		return NULL;

	*meth=map_methods_traffic;
	m=g_new0(struct map_priv, 1);
	m->id=++map_id;
	m->engine=g_strdup(data->u.str);
	if (flags)
		m->flags=flags->u.num; //

	if (charset) {
		m->charset=g_strdup(charset->u.str);
		meth->charset=m->charset;
	}
	return m;
}

// TODO: method to parse json string to list of traffic lines recode as allocator
// TODO: check data for correctness

struct json_object* json_tokener_parse_verbose(const char *str, enum json_tokener_error *error)
{
    struct json_tokener* tok;
    struct json_object* obj;

    tok = json_tokener_new();
    obj = json_tokener_parse_ex(tok, str, -1);
    *error = tok->err;
    if(tok->err != json_tokener_success) {
        obj = NULL;
    }

    json_tokener_free(tok);
    return obj;
}
int ParseJsonData (GList **TrafficList, char *strJson)
{
	int Length=0, i;
	struct json_object *JsonData;
	struct json_object *JsonTraffCoordData, *JsonTraffCoord, *JsonTmp;
	traffic_item *temp;
	struct coord_geo temp_coord;
	//dbg(0,"%s\n",strJson);
	if(strJson) {
		int error;
		JsonData = json_tokener_parse_verbose(strJson,&error);

		if(error==0) {
			JsonTraffCoordData = json_object_object_get(JsonData, "TraffData");
			Length = json_object_array_length(JsonTraffCoordData);
			for (i=0; i<Length; i++) {
				temp = (traffic_item*)g_malloc(sizeof(traffic_item));
				JsonTraffCoord = json_object_array_get_idx(JsonTraffCoordData, i);
				JsonTmp = json_object_object_get(JsonTraffCoord, "lat1");
				temp_coord.lat =  json_object_get_double(JsonTmp);
				//printf("%f\n",temp_coord.lat);
				JsonTmp = json_object_object_get(JsonTraffCoord, "lng1");
				temp_coord.lng = json_object_get_double(JsonTmp);
				transform_from_geo(projection_mg,&temp_coord,&temp->coords[0]);

				JsonTmp = json_object_object_get(JsonTraffCoord, "lat2");
				temp_coord.lat =  json_object_get_double(JsonTmp);
				JsonTmp = json_object_object_get(JsonTraffCoord, "lng2");
				temp_coord.lng = json_object_get_double(JsonTmp);
				transform_from_geo(projection_mg,&temp_coord,&temp->coords[1]);
				JsonTmp = json_object_object_get(JsonTraffCoord, "speed");
				temp->speed = (char)json_object_get_int(JsonTmp);
				//dbg (0,"list0x%x\n",*TrafficList);
				*TrafficList = g_list_append(*TrafficList,temp);

			}
			temp = (traffic_item*)g_malloc(sizeof(traffic_item));
			*TrafficList = g_list_append(*TrafficList,temp);
		}
	}
	return Length;
}

#define dCALLER "traffic.method.caller"
#define dOBJECT "/traffic/method/Object"
#define dSERVER "traffic.method.server"
#define dTYPE "traffic.method.Type"

// function to query over dbus

void dbus_query(char *parameter, char **result)
{
	DBusMessage* msg;
	DBusMessageIter args;
	DBusConnection* conn;
	DBusError err;
	DBusPendingCall* pending;
	int ret,len;
	char *tmp;
	dbus_error_init(&err);

	conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "Connection Error (%s)\n", err.message);
		dbus_error_free(&err);
	}
	if (NULL == conn) {
	      exit(1);
	}

	ret = dbus_bus_request_name(conn, dCALLER, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "Name Error (%s)\n", err.message);
		dbus_error_free(&err);
	}
	if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
		exit(1);
	}

	msg = dbus_message_new_method_call(dSERVER, // target for the method call
			dOBJECT, // object to call on
			dTYPE, // interface to call on
			"Method"); // method name
	if (NULL == msg) {
		fprintf(stderr, "Message Null\n");
		exit(1);
	}

	dbus_message_iter_init_append(msg, &args);
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parameter)) {
		fprintf(stderr, "Out Of Memory!\n");
		exit(1);
	}

	// send message and get a handle for a reply
	if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) { // -1 is default timeout
		fprintf(stderr, "Out Of Memory!\n");
		exit(1);
	}
	if (NULL == pending) {
		fprintf(stderr, "Pending Call Null\n");
		exit(1);
	}
	dbus_connection_flush(conn);
	dbus_message_unref(msg);
	dbus_pending_call_block(pending);
	msg = dbus_pending_call_steal_reply(pending);

	if (NULL == msg) {
		fprintf(stderr, "Reply Null\n");
		goto l;
	}
	dbus_pending_call_unref(pending);
	if (!dbus_message_iter_init(msg, &args))
		fprintf(stderr, "Message has no arguments!\n");
	else if (DBUS_TYPE_STRING!= dbus_message_iter_get_arg_type(&args))
	    fprintf(stderr, "Argument is not string!\n");
	else
	    dbus_message_iter_get_basic(&args, &tmp);
	if(tmp) {
		len = strlen(tmp);
		*result = g_malloc(len*sizeof(char));
		strcpy(*result,tmp);
	}


	l:
	//free reply and close connection
	dbus_message_unref(msg);
	dbus_bus_release_name(conn,dCALLER,&err);
	//printf("%s\n",result);
}

void query(struct map_rect_priv *mr)
{
	char *jsonString;/*="{\
\"TraffData\": [\
        {\
          \"lat1\" : 40.7406,\
          \"lng1\" : -73.9902,\
           \"lat2\" :40.7406,\
           \"lng2\" :-73.9902,\
           \"speed\" : 5\
        },\
        {\
          \"lat1\" : 40.7406,\
          \"lng1\" : -73.9902,\
           \"lat2\" :40.7405,\
           \"lng2\" :-73.99,\
           \"speed\" : 5\
        },\
        {\
          \"lat1\" : 40.7405,\
          \"lng1\" : -73.99,\
           \"lat2\" :40.7406,\
           \"lng2\" :-73.99,\
           \"speed\" : 5\
        }]}\
";*/

	char param[256];
	struct coord_geo geo1,ge2;
	transform_to_geo(projection_mg,&mr->sel->u.c_rect.lu,&geo1);
	transform_to_geo(projection_mg,&mr->sel->u.c_rect.rl,&ge2);
	sprintf(param,"get lp: %lf %lf rb: %lf %lf order:%d",geo1.lat,geo1.lng,ge2.lat,ge2.lng,mr->sel->order);
	dbg(0,"%s\n",param);
	char *param1 = "get";

	dbus_query(param1,&jsonString);
	//printf("Value: %s\n",jsonString);
	if(jsonString!=NULL) {
		ParseJsonData(&mr->traffic_list,jsonString);
		free(jsonString);
	}
//	sleep(1);
}

void
plugin_init(void)
{
	dbg(1,"traffic: plugin_init\n");
	plugin_register_map_type("traffic", map_new_traffic);
}
