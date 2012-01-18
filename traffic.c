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
#include <json/json.h>
#include <dbus/dbus.h>
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
struct TraffCoord *traf;
void query(struct TraffCoord *traf,int *count);
int  ParseJsonData (struct TraffCoord *TraffData, char * strJson);

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
	if (map->is_pipe) {
#ifdef HAVE_POPEN

		char *oargs,*args=g_strdup(map->filename),*sep=" ";
		enum layer_type lay;
		g_free(mr->args);
		while (sel) {
			oargs=args;
			args=g_strdup_printf("%s 0x%x 0x%x 0x%x 0x%x", oargs, sel->u.c_rect.lu.x, sel->u.c_rect.lu.y, sel->u.c_rect.rl.x, sel->u.c_rect.rl.y);
			g_free(oargs);
			for (lay=layer_town ; lay < layer_end ; lay++) {
				oargs=args;
				args=g_strdup_printf("%s%s%d", oargs, sep, sel->order);
				g_free(oargs);
				sep=",";
			}
			sel=sel->next;
		}
		dbg(1,"popen args %s\n", args);
		mr->args=args;
		mr->f=popen(mr->args, "r");
		mr->pos=0;
		mr->lastlen=0;
#else
		dbg(0,"map_rect_new_traffic is unable to work with pipes %s\n",map->filename);
#endif 
	
	  
	} else {
		mr->f=fopen(map->filename, "r");
	}
	if(!mr->f) {
		printf("map_rect_new_traffic unable to open traffic %s. Error: %s\n",map->filename, strerror(errno));
	}
	get_line(mr);
	return mr;
}


static void
map_rect_destroy_traffic(struct map_rect_priv *mr)
{
	if (mr->f) {
		if (mr->m->is_pipe) {
#ifdef HAVE_POPEN
			pclose(mr->f);
#endif
		}
		else {
			fclose(mr->f);
		}
	}
        g_free(mr);
}

static struct item *
map_rect_get_item_traffic(struct map_rect_priv *mr)
{
	printf("\n%p",&mr->item);
	char *p,type[SIZE];
	dbg(1,"map_rect_get_item_traffic id_hi=%d line=%s\n", mr->item.id_hi, mr->line);
	if (!mr->f) {
		return NULL;
	}
	if (mr->more != 0) printf("\n%d",mr->more);
	//more is special element for i
	//while (mr->more) {
		//struct coord c;
		//traffic_coord_get(mr, &c, 1);
	//}
	for(;;) {
		if (feof(mr->f)) {
			dbg(1,"map_rect_get_item_traffic: eof %d\n",mr->item.id_hi);
			if (mr->m->flags & 1) {
				if (!mr->item.id_hi) 
					return NULL;
				mr->item.id_hi=0;
			} else {
				if (mr->item.id_hi) 
					return NULL;
				mr->item.id_hi=1;
			}
			if (mr->m->is_pipe) {
#ifdef HAVE_POPEN
				pclose(mr->f);
				mr->f=popen(mr->args, "r");
				mr->pos=0;
				mr->lastlen=0;
#endif
			} else {
				fseek(mr->f, 0, SEEK_SET);
				clearerr(mr->f);
			}
			get_line(mr);
		}
		if ((p=strchr(mr->line,'\n'))) 
			*p='\0';
		if (mr->item.id_hi) {
			mr->attrs[0]='\0';
			if (!parse_line(mr, 1)) {
				get_line(mr);
				continue;
			}
			dbg(1,"map_rect_get_item_traffic: point found\n");
			mr->eoc=0;
			mr->item.id_lo=mr->pos;
		} else {
			if (parse_line(mr, 1)) {
				get_line(mr);
				continue;
			}
			dbg(1,"map_rect_get_item_traffic: line found\n");
			if (! mr->line[0]) {
				get_line(mr);
				continue;
			}
			mr->item.id_lo=mr->pos;
			strcpy(mr->attrs, mr->line);
			get_line(mr);
			dbg(1,"mr=%p attrs=%s\n", mr, mr->attrs);
		}
		dbg(1,"get_attrs %s\n", mr->attrs);
		if (attr_from_line(mr->attrs,"type",NULL,type,NULL)) {
			dbg(1,"type='%s'\n", type);
			mr->item.type=item_from_name(type);
			if (mr->item.type == type_none) 
				printf("Warning: type '%s' unknown\n", type);
		} else {
			get_line(mr);
			continue;
		}
		mr->attr_last=attr_none;
		mr->more=1;
		dbg(1,"return attr='%s'\n", mr->attrs);
		return &mr->item;
	}
}

static struct item *
map_rect_get_item_byid_traffic(struct map_rect_priv *mr, int id_hi, int id_lo)
{
	if (mr->m->is_pipe) {
#ifndef _MSC_VER
		pclose(mr->f);
		mr->f=popen(mr->args, "r");
		mr->pos=0;
		mr->lastlen=0;
#endif /* _MSC_VER */
	} else
		fseek(mr->f, id_lo, SEEK_SET);
	get_line(mr);
	mr->item.id_hi=id_hi;
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

/*struct TraffCoord
{
	double FirstCoord, SecondCoord;
	char Direction[2];
	//int MaxSpeed;
} traf[100] = {  {4629.868000,3037.662000,4623.916000,3046.296000,'N','E','N','E',5}
  
};*/



struct TraffCoord
{
	double FirstCoord, SecondCoord, ThirdCoord, FourthCoord;
	char Direction[4];
	int MaxSpeed;
};

static struct map_priv *
map_new_traffic(struct map_methods *meth, struct attr **attrs, struct callback_list *cbl)
{
	struct map_priv *m;
	struct attr *data=attr_search(attrs, NULL, attr_data); // look for data 
	struct attr *charset=attr_search(attrs, NULL, attr_charset); // charset 
	struct attr *flags=attr_search(attrs, NULL, attr_flags); // flags
	struct file_wordexp *wexp; // ???
	int len,is_pipe=0;  //???
	char *wdata; //???
	char **wexp_data; // ???
	if (! data)
		return NULL;
	dbg(1,"map_new_traffic %s\n", data->u.str);	 // dummy
	//fprintf(stderr,"traffic map : %s\n", data->u.str); // dummy
	wdata=g_strdup(data->u.str); // strings points to file of map
	//dbg(0,"data %s\n",data->u.str);
	len=strlen(wdata); 
	// pipes will be not used
	if (len && wdata[len-1] == '|') {
		wdata[len-1]='\0';
		is_pipe=1;
	}
	wexp=file_wordexp_new(wdata);
	wexp_data=file_wordexp_get_array(wexp);
	*meth=map_methods_traffic;
	m=g_new0(struct map_priv, 1);
	m->id=++map_id; // will be there
	//m->filename=g_strdup(wexp_data[0]); // file name will has gone
	m->is_pipe=is_pipe; // will has gone
	if (flags)  
		m->flags=flags->u.num; // 
	// счас здесь создадим файлик с содержимым
	//
	char *fname = "./hello_my.txt";
	char *fmode = "w";
	//char *buf=malloc(1024);;
	m->filename = g_strdup(fname);
	FILE * fil = fopen(fname,fmode);
	if(fil==NULL) {
	  return 0;
	}
	struct TraffCoord* here = (struct TraffCoord*)malloc(sizeof(struct TraffCoord)*100);;
	int count=0,i=0;
	query(here,&count);
	printf("count:%d\n",count);

	for(i=0;i<2;i++) {
		fprintf(fil,"type=street_traffic\n");
		fprintf(fil,"%.6f %c %.6f %c\n",here[i].FirstCoord,here[i].Direction[0],here[i].SecondCoord,here[i].Direction[1]);
		fprintf(fil,"%.6f %c %.6f %c\n",here[i].ThirdCoord,here[i].Direction[2],here[i].FourthCoord,here[i].Direction[3]);


		//
		//printf("type=street_traffic\n");
		//printf("%.6f %c %.6f %c\n",here[i].FirstCoord,here[i].Direction[0],here[i].SecondCoord,here[i].Direction[1]);
		//printf("%.6f %c %.6f %c\n",here[i].ThirdCoord,here[i].Direction[2],here[i].FourthCoord,here[i].Direction[3]);
	}
	//printf(buf);
	//fprintf(fil,buf);
	//fprintf(fil,"type=street_traffic\n");
	//fprintf(fil,"4629.868000 N 3037.662000 E\n");
	//fprintf(fil,"4623.916000 N 3046.296000 E\n");
	
	
	
	//fprintf(fil,"4628.916000 N 3081.296000 E\n");
	//fprintf(fil,"4624.916000 N 3046.296000 E\n");
	fclose(fil);
	dbg(1,"map_new_traffic %s\n", m->filename);
	if (charset) {
		m->charset=g_strdup(charset->u.str);
		meth->charset=m->charset;
	}
	file_wordexp_destroy(wexp);
	g_free(wdata);
	
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

//void query(char *str)
void query(struct TraffCoord *traf,int *count)
{
   DBusMessage* msg;
   DBusMessageIter args;
   DBusConnection* conn;
   DBusError err;
   DBusPendingCall* pending;
   int ret;
   char *stat;
   char *param = "get";
   //printf("Calling remote method with %s\n", param);
   // initialiset the errors
   dbus_error_init(&err);
   // connect to the system bus and check for errors
   conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
   if (dbus_error_is_set(&err)) { 
      fprintf(stderr, "Connection Error (%s)\n", err.message); 
      dbus_error_free(&err);
   }
   if (NULL == conn) { 
      exit(1); 
   }

   // request our name on the bus
   ret = dbus_bus_request_name(conn, "traffic.method.caller", DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
   if (dbus_error_is_set(&err)) { 
      fprintf(stderr, "Name Error (%s)\n", err.message); 
      dbus_error_free(&err);
   }
   if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) { 
      exit(1);
   }

   // create a new method call and check for errors
   msg = dbus_message_new_method_call("traffic.method.server", // target for the method call
                                      "/traffic/method/Object", // object to call on
                                      "traffic.method.Type", // interface to call on
                                      "Method"); // method name
   if (NULL == msg) { 
      fprintf(stderr, "Message Null\n");
      exit(1);
   }

   // append arguments
   dbus_message_iter_init_append(msg, &args);
   if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &param)) {
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
   
   printf("Request Sent\n");
   
   // free message
   dbus_message_unref(msg);
   
   // block until we recieve a reply
   dbus_pending_call_block(pending);

   // get the reply message
   msg = dbus_pending_call_steal_reply(pending);
   if (NULL == msg) {
      fprintf(stderr, "Reply Null\n"); 
     // exit(1); 
      goto l;
   }
   // free the pending message handle
   dbus_pending_call_unref(pending);
   // read the parameters
   if (!dbus_message_iter_init(msg, &args))
      fprintf(stderr, "Message has no arguments!\n"); 
   else if (DBUS_TYPE_STRING!= dbus_message_iter_get_arg_type(&args)) 
      fprintf(stderr, "Argument is not string!\n"); 
   else
      dbus_message_iter_get_basic(&args, &stat);
   

 /// stat is json


   //strcpy(str,stat);

  // printf("Got Reply: %s, \n", stat);
   l:
   // free reply and close connection
   
   dbus_message_unref(msg);   
   dbus_bus_release_name(conn,"traffic.method.caller",&err);

   if(stat!=NULL){
      *count = ParseJsonData(traf,stat);
   } else {

   }
   //dbus_connection_close(conn);
}

void
plugin_init(void)
{
	dbg(1,"traffic: plugin_init\n");
	plugin_register_map_type("traffic", map_new_traffic);
}
