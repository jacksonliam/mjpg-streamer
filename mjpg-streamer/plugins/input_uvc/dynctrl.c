#include <sys/ioctl.h>
#include <sys/time.h>
#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#include <linux/videodev2.h>

#include "../../utils.h"
#include "../../mjpg_streamer.h"
#include "uvcvideo.h"
#include "dynctrl.h"

/* some Logitech webcams have pan/tilt/focus controls */
static struct uvc_xu_control_info xu_ctrls[] = {
  {
    .entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector = XU_MOTORCONTROL_PANTILT_RELATIVE,
    .index    = 0,
    .size     = 4,
    .flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_DEF
  },
  {
    .entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector = XU_MOTORCONTROL_PANTILT_RESET,
    .index    = 1,
    .size     = 1,
    .flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF
  },
  {
    .entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector = XU_MOTORCONTROL_FOCUS,
    .index    = 2,
    .size     = 6,
    .flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_DEF
  },
};

/* mapping for Pan/Tilt/Focus */
static struct uvc_xu_control_mapping xu_mappings[] = {
  {
    .id        = V4L2_CID_PAN_RELATIVE,
    .name      = "Pan (relative)",
    .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector  = XU_MOTORCONTROL_PANTILT_RELATIVE,
    .size      = 16,
    .offset    = 0,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_SIGNED
  },
  {
    .id        = V4L2_CID_TILT_RELATIVE,
    .name      = "Tilt (relative)",
    .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector  = XU_MOTORCONTROL_PANTILT_RELATIVE,
    .size      = 16,
    .offset    = 16,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_SIGNED
  },
  {
    .id        = V4L2_CID_PANTILT_RESET,
    .name      = "Pan/Tilt (reset)",
    .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector  = XU_MOTORCONTROL_PANTILT_RESET,
    .size      = 2,
    .offset    = 0,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_ENUM
  },
  {
    .id        = V4L2_CID_FOCUS_LOGITECH,
    .name      = "Focus (absolute)",
    .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector  = XU_MOTORCONTROL_FOCUS,
    .size      = 8,
    .offset    = 0,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
  }
};

void initDynCtrls(int dev) {
#ifdef UVC_DYN_CONTROLS
  int i, err;

  /* try to add all controls listed above */
  for ( i=0; i<LENGTH_OF(xu_ctrls); i++ ) {
    DBG("adding control for %d\n", i);
    errno=0;
    if ( (err=ioctl(dev, UVCIOC_CTRL_ADD, &xu_ctrls[i])) < 0 ) {
      if ( errno != EEXIST ) {
        DBG("uvcioc ctrl add error: errno=%d (retval=%d)\n", errno, err);
      } else {
        DBG("control %d already exists\n", i);
      }
    }
  }

  /* after adding the controls, add the mapping now */
  for ( i=0; i<LENGTH_OF(xu_mappings); i++ ) {
    DBG("mapping controls for %s\n", xu_mappings[i].name);
    errno=0;
    if ((err=ioctl(dev, UVCIOC_CTRL_MAP, &xu_mappings[i])) < 0) {
      if (errno != EEXIST) {
        DBG("uvcioc ctrl map error: errno=%d (retval=%d)\n", errno, err);
      } else {
        DBG("mapping %d already exists\n", i);
      }
    }
  }
#endif
}

