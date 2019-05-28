#include "../../mjpg_streamer.h"

enum {	EVENT_FLG_NON_DYNAMIC, EVENT_FLG_DYNAMIC };
enum {	EVENT_ID_SPEED, EVENT_ID_STEP,
		EVENT_ID_FRAME, EVENT_ID_STOP, EVENT_ID_PLAY_MODE };
enum { EVENT_MODE_PAUSE, EVENT_MODE_PLAY, EVENT_MODE_STEP_FWD,
		EVENT_MODE_STEP_BACK, EVENT_MODE_JUMP};

#define EVENT_SPEED_DFLT 1
#define EVENT_FRAME_DFLT 1
#define EVENT_PLAY_MODE_DFLT EVENT_MODE_PLAY
#define EVENT_STEP_DFLT 0
#define EVENT_MODE_DFLT EVENT_MODE_PLAY

#define TRUE 1
#define FALSE 0

struct _control ieparms[] =
{
	{
		.ctrl.id = EVENT_ID_SPEED, 
		.ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
		.ctrl.name = "speed",
		.ctrl.minimum = -1,
		.ctrl.maximum = 1,
		.ctrl.step = 1,
		.ctrl.default_value = EVENT_SPEED_DFLT,
		.ctrl.flags = EVENT_FLG_DYNAMIC,
		.ctrl.reserved = {0,0},
		.value = EVENT_SPEED_DFLT, 
		.menuitems = NULL,
		.class_id = 0,
		.group = 1
	},
	{
		.ctrl.id = EVENT_ID_STEP, 
		.ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
		.ctrl.name = "xstep",
		.ctrl.minimum = -1,
		.ctrl.maximum = 1,
		.ctrl.step = 1,
		.ctrl.default_value = 0,
		.ctrl.flags = EVENT_FLG_DYNAMIC,
		.ctrl.reserved = {0,0},
		.value = EVENT_STEP_DFLT, 
		.menuitems = NULL,
		.class_id = 0,
		.group = 1
	},
	{
		.ctrl.id = EVENT_ID_FRAME, 
		.ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
		.ctrl.name = "frame",
		.ctrl.minimum = 0,
		.ctrl.maximum = 1,
		.ctrl.step = 1,
		.ctrl.default_value = EVENT_FRAME_DFLT,
		.ctrl.flags = EVENT_FLG_DYNAMIC,
		.ctrl.reserved = {0,0},
		.value = EVENT_MODE_DFLT, 
		.menuitems = NULL,
		.class_id = 0,
		.group = 1
	},
	{
		.ctrl.id = EVENT_ID_STOP, 
		.ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
		.ctrl.name = "stop",
		.ctrl.minimum = 1,
		.ctrl.maximum = 1,
		.ctrl.step = 1,
		.ctrl.default_value = 0,
		.ctrl.flags = EVENT_FLG_DYNAMIC,
		.ctrl.reserved = {0,0},
		.value = 0, 
		.menuitems = NULL,
		.class_id = 0,
		.group = 1
	},
	{
		.ctrl.id = EVENT_ID_PLAY_MODE, 
		.ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
		.ctrl.name = "mode",
		.ctrl.minimum = 0,
		.ctrl.maximum = 1,
		.ctrl.step = 1,
		.ctrl.default_value = EVENT_PLAY_MODE_DFLT,
		.ctrl.flags = EVENT_FLG_DYNAMIC,
		.ctrl.reserved = {0,0},
		.value = EVENT_MODE_DFLT, 
		.menuitems = NULL,
		.class_id = 0,
		.group = 1
	}
};
