/* input_ptp2.c -- MJPG-streamer input plugin to stream JPG frames
 from digital cameras supporting PTP2 capture

 Copyright (C) 2010, Alessio Sangalli (alesan@manoweb.com)

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software Foundation,
 Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#define CAMERA_CHECK_GP(res, msg){if(res != GP_OK){IPRINT(INPUT_PLUGIN_NAME " - Gphoto error, on '%s': %d - %s\n", msg, res, gp_result_as_string(res)); return 0;}}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <gphoto2/gphoto2-camera.h>
#include "input_ptp2.h"

#define INPUT_PLUGIN_NAME "PTP2 input plugin"

static int plugin_id;
static pthread_t thread;
static pthread_mutex_t control_mutex;
static globals* global;

GPContext* context;
Camera* camera;
char* selected_port;
int delay;

int input_init(input_parameter *param, int id)
{
	int i;
	int opt;

	global = param->global;

	if(pthread_mutex_init(&control_mutex, NULL) != 0)
	{
		IPRINT(INPUT_PLUGIN_NAME "- Could not initialize mutex variable\n");
		exit(EXIT_FAILURE);
	}

	control zoom_ctrl;
	zoom_ctrl.group = IN_CMD_GENERIC;
	zoom_ctrl.menuitems = NULL;
	zoom_ctrl.value = 0.0;
	zoom_ctrl.class_id = 0;

	zoom_ctrl.ctrl.id = 1;
	zoom_ctrl.ctrl.type = V4L2_CTRL_TYPE_INTEGER;
	strcpy((char*) zoom_ctrl.ctrl.name, "Zoom");
	zoom_ctrl.ctrl.minimum = 0;
	zoom_ctrl.ctrl.maximum = 10;
	zoom_ctrl.ctrl.step = 1;
	zoom_ctrl.ctrl.default_value = 0;
	zoom_ctrl.ctrl.flags = V4L2_CTRL_FLAG_SLIDER;

	param->global->in[id].in_parameters = (control*) malloc((param->global->in[id].parametercount + 1) * sizeof(control));

	param->global->in[id].in_parameters[param->global->in[id].parametercount] = zoom_ctrl;
	param->global->in[id].parametercount++;

	selected_port = NULL;
	delay = 0;

	param->argv[0] = INPUT_PLUGIN_NAME;

	/* show all parameters for DBG purposes */
	for(i = 0; i < param->argc; i++)
	{
		DBG("argv[%d]=%s\n", i, param->argv[i]);
	}

	optind = 1;
	while((opt = getopt(param->argc, param->argv, "hu:d:")) != -1)
	{
		switch(opt)
		{
			case 'h':
				help();
				return 1;
			case 'u':
				delay = atoi(optarg);
				break;
			case 'd':
				selected_port = strdup(optarg);
				break;
		}
	}

	DBG("usleep: %d\n", delay); DBG("device: %s\n", selected_port);

	return 0;
}

int input_stop(int id)
{
	DBG("will cancel input thread\n");
	pthread_cancel(thread);

	return 0;
}

void help()
{
	printf(" ---------------------------------------------------------------\n"
		" Help for input plugin..: "INPUT_PLUGIN_NAME"\n"
	" ---------------------------------------------------------------\n"
	" The following parameters can be passed to this plugin:\n\n"
	" [-h ]..........: print this help\n"
	" [-u X ]........: delay between frames in us (default 0)\n"
	" [-d X ]........: camera address in [usb:xxx,yyy] form; use\n"
	"                  gphoto2 --auto-detect to get a list of\n"
	"                  available cameras\n"
	" ---------------------------------------------------------------\n");
}

int input_run(int id)
{
	int res, i;

	global->in[id].buf = malloc(256 * 1024);
	if(global->in[id].buf == NULL)
	{
		IPRINT(INPUT_PLUGIN_NAME " - could not allocate memory\n");
		exit(EXIT_FAILURE);
	}
	plugin_id = id;

	// auto-detect algorithm
	CameraAbilitiesList* al;
	GPPortInfoList* il;
	CameraList* list;
	const char* model;
	const char* port;
	context = gp_context_new();
	gp_abilities_list_new(&al);
	gp_abilities_list_load(al, context);
	gp_port_info_list_new(&il);
	gp_port_info_list_load(il);
	gp_list_new(&list);
	gp_abilities_list_detect(al, il, list, context);
	int count = gp_list_count(list);
	IPRINT(INPUT_PLUGIN_NAME " - Detected %d camera(s)\n", count);
	if(count == 0)
	{
		IPRINT(INPUT_PLUGIN_NAME " - No cameras detected.\n");
		return 0;
	}
	GPPortInfo info;
	CameraAbilities a;
	int m, p;
	camera = NULL;
	for(i = 0; i < count; i++)
	{
		res = gp_list_get_name(list, i, &model);
		CAMERA_CHECK_GP(res, "gp_list_get_name");
		m = gp_abilities_list_lookup_model(al, model);
		if(m < 0)
		{
			IPRINT(INPUT_PLUGIN_NAME " - Gphoto abilities_list_lookup_model Code: %d - %s\n", m, gp_result_as_string(m));
			return 0;
		}
		res = gp_abilities_list_get_abilities(al, m, &a);
		CAMERA_CHECK_GP(res, "gp_abilities_list_get_abilities");
		res = gp_list_get_value(list, i, &port);
		CAMERA_CHECK_GP(res, "gp_list_get_value"); DBG("Model: %s; port: %s.\n", model, port);
		if(selected_port != NULL && strcmp(selected_port, port) != 0)
			continue;
		p = gp_port_info_list_lookup_path(il, port);
		if(p < 0)
		{
			IPRINT(INPUT_PLUGIN_NAME " - Gphoto port_info_list_lookup_path Code: %d - %s\n", m, gp_result_as_string(m));
			return 0;
		}
		res = gp_port_info_list_get_info(il, p, &info);
		CAMERA_CHECK_GP(res, "gp_port_info_list_get_info");

		res = gp_camera_new(&camera);
		CAMERA_CHECK_GP(res, "gp_camera_new");
		res = gp_camera_set_abilities(camera, a);
		CAMERA_CHECK_GP(res, "gp_camera_set_abilities");
		res = gp_camera_set_port_info(camera, info);
		CAMERA_CHECK_GP(res, "gp_camera_set_port_info");
	}
	if(camera == NULL)
	{
		IPRINT("Camera %s not found, exiting.\n", selected_port);
		exit(EXIT_FAILURE);
	}
	// cleanup
	gp_list_unref(list);
	gp_port_info_list_free(il);
	gp_abilities_list_free(al);

	// open camera and set capture on
	int value = 1;
	res = gp_camera_init(camera, context);
	CAMERA_CHECK_GP(res, "gp_camera_init");
	camera_set("capture", &value);

	// starting thread
	if(pthread_create(&thread, 0, capture, NULL) != 0)
	{
		free(global->in[id].buf);
		IPRINT("could not start worker thread\n");
		exit(EXIT_FAILURE);
	}
	pthread_detach(thread);

	return 0;
}

void* capture(void* arg)
{
	int res;
	int i = 0;
	CameraFile* file;

	pthread_cleanup_push(cleanup, NULL);
					while(!global->stop)
					{
						unsigned long int xsize;
						const char* xdata;
						pthread_mutex_lock(&control_mutex);
						res = gp_file_new(&file);
						CAMERA_CHECK_GP(res, "gp_file_new");
						res = gp_camera_capture_preview(camera, file, context);
						CAMERA_CHECK_GP(res, "gp_camera_capture_preview");
						pthread_mutex_lock(&global->in[plugin_id].db);
						res = gp_file_get_data_and_size(file, &xdata, &xsize);
						if(xsize == 0)
						{
							if(i++ > 3)
							{
								IPRINT("Restarted too many times; giving up\n");
								return NULL;
							}
							int value = 0;
							IPRINT("Read 0 bytes from camera; restarting it\n");
							camera_set("capture", &value);
							sleep(3);
							value = 1;
							camera_set("capture", &value);
						}
						else
							i = 0;
						CAMERA_CHECK_GP(res, "gp_file_get_data_and_size");
						memcpy(global->in[plugin_id].buf, xdata, xsize);
						res = gp_file_unref(file);
						pthread_mutex_unlock(&control_mutex);
						CAMERA_CHECK_GP(res, "gp_file_unref");
						global->in[plugin_id].size = xsize;
						DBG("Read %d bytes from camera.\n", global->in[plugin_id].size);
						pthread_cond_broadcast(&global->in[plugin_id].db_update);
						pthread_mutex_unlock(&global->in[plugin_id].db);
						usleep(delay);
					}
					pthread_cleanup_pop(1);

	return NULL;
}

int camera_set(char* name, void* value)
{
	int res;

	CameraWidget* config_root;
	CameraWidget* widget;
	res = gp_camera_get_config(camera, &config_root, context);
	CAMERA_CHECK_GP(res, "gp_camera_get_config");
	res = gp_widget_get_child_by_name(config_root, name, &widget);
	CAMERA_CHECK_GP(res, "gp_widget_get_child_by_name");
	res = gp_widget_set_value(widget, value);
	CAMERA_CHECK_GP(res, "gp_widget_set_value");
	res = gp_camera_set_config(camera, config_root, context);
	CAMERA_CHECK_GP(res, "gp_camera_set_config");
	gp_widget_unref(config_root);
	return 1;
}

void cleanup(void *arg)
{
	int value = 0;

	// TODO check to see if we have already cleaned up?

	IPRINT("PTP2 capture - Cleaning up\n");
	camera_set("capture", &value);
	gp_camera_exit(camera, context);
	gp_camera_unref(camera);
	gp_context_unref(context);
	free(global->in[plugin_id].buf);
}

int input_cmd(int plugin, unsigned int control_id, unsigned int group, int value)
{
	int res;
	int i;
	DBG("Requested cmd (id: %d) for the %d plugin. Group: %d value: %d\n", control_id, plugin_id, group, value);
	switch(group)
	{
		case IN_CMD_GENERIC:
			for(i = 0; i < global->in[plugin_id].parametercount; i++)
			{
				if((global->in[plugin_id].in_parameters[i].ctrl.id == control_id) && (global->in[plugin_id].in_parameters[i].group == IN_CMD_GENERIC))
				{
					DBG("Generic control found (id: %d): %s\n", control_id, global->in[plugin_id].in_parameters[i].ctrl.name);
					if(control_id == 1)
					{
						float z = value;
						pthread_mutex_lock(&control_mutex);
						res = camera_set("zoom", &z);
						pthread_mutex_unlock(&control_mutex);
					} DBG("New %s value: %d\n", global->in[plugin_id].in_parameters[i].ctrl.name, value);
					return 0;
				}
			}
			DBG("Requested generic control (%d) did not found\n", control_id);
			return -1;
			break;
	}
	return 0;
}
