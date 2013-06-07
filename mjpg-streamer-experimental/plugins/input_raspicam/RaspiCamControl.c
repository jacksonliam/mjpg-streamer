/*
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, James Hughes
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <memory.h>

#include "interface/vcos/vcos.h"

#include "interface/vmcs_host/vc_vchi_gencmd.h"
#include "mmal/mmal.h"
//#include "mmal/mmal_logging.h"
//#include "mmal/util/mmal_util.h"
//#include "mmal/util/mmal_util_params.h"
#include "mmal/util/mmal_default_components.h"
#include "RaspiCamControl.h"

/// Cross reference structure, mode string against mode id
typedef struct xref_t
{
   char *mode;
   int mmal_mode;
} XREF_T;

typedef struct
{
   int id;
   char *command;
   char *abbrev;
   char *help;
   int num_parameters;
} COMMAND_LIST;

/// Structure to cross reference exposure strings against the MMAL parameter equivalent
static XREF_T  exposure_map[] =
{
   {"off",           MMAL_PARAM_EXPOSUREMODE_OFF},
   {"auto",          MMAL_PARAM_EXPOSUREMODE_AUTO},
   {"night",         MMAL_PARAM_EXPOSUREMODE_NIGHT},
   {"nightpreview",  MMAL_PARAM_EXPOSUREMODE_NIGHTPREVIEW},
   {"backlight",     MMAL_PARAM_EXPOSUREMODE_BACKLIGHT},
   {"spotlight",     MMAL_PARAM_EXPOSUREMODE_SPOTLIGHT},
   {"sports",        MMAL_PARAM_EXPOSUREMODE_SPORTS},
   {"snow",          MMAL_PARAM_EXPOSUREMODE_SNOW},
   {"beach",         MMAL_PARAM_EXPOSUREMODE_BEACH},
   {"verylong",      MMAL_PARAM_EXPOSUREMODE_VERYLONG},
   {"fixedfps",      MMAL_PARAM_EXPOSUREMODE_FIXEDFPS},
   {"antishake",     MMAL_PARAM_EXPOSUREMODE_ANTISHAKE},
   {"fireworks",     MMAL_PARAM_EXPOSUREMODE_FIREWORKS}
};

static const int exposure_map_size = sizeof(exposure_map) / sizeof(exposure_map[0]);

/// Structure to cross reference awb strings against the MMAL parameter equivalent
static XREF_T awb_map[] =
{
   {"off",           MMAL_PARAM_AWBMODE_OFF},
   {"auto",          MMAL_PARAM_AWBMODE_AUTO},
   {"sun",           MMAL_PARAM_AWBMODE_SUNLIGHT},
   {"cloud",         MMAL_PARAM_AWBMODE_CLOUDY},
   {"shade",         MMAL_PARAM_AWBMODE_SHADE},
   {"tungsten",      MMAL_PARAM_AWBMODE_TUNGSTEN},
   {"fluorescent",   MMAL_PARAM_AWBMODE_FLUORESCENT},
   {"incandescent",  MMAL_PARAM_AWBMODE_INCANDESCENT},
   {"flash",         MMAL_PARAM_AWBMODE_FLASH},
   {"horizon",       MMAL_PARAM_AWBMODE_HORIZON}
};

static const int awb_map_size = sizeof(awb_map) / sizeof(awb_map[0]);

/// Structure to cross reference image effect against the MMAL parameter equivalent
static XREF_T imagefx_map[] =
{
   {"none",          MMAL_PARAM_IMAGEFX_NONE},
   {"negative",      MMAL_PARAM_IMAGEFX_NEGATIVE},
   {"solarise",      MMAL_PARAM_IMAGEFX_SOLARIZE},
   {"sketch",        MMAL_PARAM_IMAGEFX_SKETCH},
   {"denoise",       MMAL_PARAM_IMAGEFX_DENOISE},
   {"emboss",        MMAL_PARAM_IMAGEFX_EMBOSS},
   {"oilpaint",      MMAL_PARAM_IMAGEFX_OILPAINT},
   {"hatch",         MMAL_PARAM_IMAGEFX_HATCH},
   {"gpen",          MMAL_PARAM_IMAGEFX_GPEN},
   {"pastel",        MMAL_PARAM_IMAGEFX_PASTEL},
   {"watercolour",   MMAL_PARAM_IMAGEFX_WATERCOLOUR},
   {"film",          MMAL_PARAM_IMAGEFX_FILM},
   {"blur",          MMAL_PARAM_IMAGEFX_BLUR},
   {"saturation",    MMAL_PARAM_IMAGEFX_SATURATION},
   {"colourswap",    MMAL_PARAM_IMAGEFX_COLOURSWAP},
   {"washedout",     MMAL_PARAM_IMAGEFX_WASHEDOUT},
   {"posterise",     MMAL_PARAM_IMAGEFX_POSTERISE},
   {"colourpoint",   MMAL_PARAM_IMAGEFX_COLOURPOINT},
   {"colourbalance", MMAL_PARAM_IMAGEFX_COLOURBALANCE},
   {"cartoon",       MMAL_PARAM_IMAGEFX_CARTOON}
 };

static const int imagefx_map_size = sizeof(imagefx_map) / sizeof(imagefx_map[0]);

static XREF_T metering_mode_map[] =
{
   {"average",       MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE},
   {"spot",          MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT},
   {"backlit",       MMAL_PARAM_EXPOSUREMETERINGMODE_BACKLIT},
   {"matrix",        MMAL_PARAM_EXPOSUREMETERINGMODE_MATRIX}
};

static const int metering_mode_map_size = sizeof(metering_mode_map)/sizeof(metering_mode_map[0]);


#define CommandSharpness   0
#define CommandContrast    1
#define CommandBrightness  2
#define CommandSaturation  3
#define CommandISO         4
#define CommandVideoStab   5
#define CommandEVComp      6
#define CommandExposure  7
#define CommandAWB         8
#define CommandImageFX     9
#define CommandColourFX    10
#define CommandMeterMode   11
#define CommandRotation    12
#define CommandHFlip       13
#define CommandVFlip       14

static COMMAND_LIST  cmdline_commands[] =
{
   {CommandSharpness,   "-sharpness", "sh", "Set image sharpness (-100 to 100)",  1},
   {CommandContrast,    "-contrast",  "co", "Set image contrast (-100 to 100)",  1},
   {CommandBrightness,  "-brightness","br", "Set image brightness (0 to 100)",  1},
   {CommandSaturation,  "-saturation","sa", "Set image saturation (-100 to 100)", 1},
   {CommandISO,         "-ISO",       "ISO","Set capture ISO",  1},
   {CommandVideoStab,   "-vstab",     "vs", "Turn on video stablisation", 0},
   {CommandEVComp,      "-ev",        "ev", "Set EV compensation",  1},
   {CommandExposure,    "-exposure",  "ex", "Set exposure mode (see Notes)", 1},
   {CommandAWB,         "-awb",       "awb","Set AWB mode (see Notes)", 1},
   {CommandImageFX,     "-imxfx",     "ifx","Set image effect (see Notes)", 1},
   {CommandColourFX,    "-colfx",     "cfx","Set colour effect (U:V)",  1},
   {CommandMeterMode,   "-metering",  "mm", "Set metering mode (see Notes)", 1},
   {CommandRotation,    "-rotation",  "rot","Set image rotation (0-359)", 1},
   {CommandHFlip,       "-hflip",     "hf", "Set horizontal flip", 0},
   {CommandVFlip,       "-vflip",     "vf", "Set vertical flip", 0}
};

static int cmdline_commands_size = sizeof(cmdline_commands) / sizeof(cmdline_commands[0]);


#define parameter_reset -99999

/**
 * Update the passed in parameter according to the rest of the parameters
 * passed in.
 *
 *
 * @return 0 if reached end of cycle for this parameter, !0 otherwise
 */
static int update_cycle_parameter(int *option, int min, int max, int increment)
{
   vcos_assert(option);
   if (!option)
      return 0;

   if (*option == parameter_reset)
      *option = min - increment;

   *option += increment;

   if (*option > max)
   {
      *option = parameter_reset;
      return 0;
   }
   else
      return 1;
}


/**
 * Test/Demo code to cycle through a bunch of camera settings
 * This code is pretty hacky so please don't complain!!
 * It only does stuff that should have a visual impact (hence demo!)
 * This will override any user supplied parameters
 *
 * Each call of this function will move on to the next setting
 *
 * @param camera Pointer to the camera to change settings on.
 * @return 0 if reached end of complete sequence, !0 otherwise
 */

int raspicamcontrol_cycle_test(MMAL_COMPONENT_T *camera)
{
   static int parameter = 0;
   static int parameter_option = parameter_reset; // which value the parameter currently has

   vcos_assert(camera);

   // We are going to cycle through all the relevant entries in the parameter block
   // and send options to the camera.
   if (parameter == 0)
   {
      // sharpness
      if (update_cycle_parameter(&parameter_option, -100, 100, 10))
         raspicamcontrol_set_sharpness(camera, parameter_option);
      else
      {
         raspicamcontrol_set_sharpness(camera, 0);
         parameter++;
      }
   }
   else
   if (parameter == 1)
   {
      // contrast
      if (update_cycle_parameter(&parameter_option, -100, 100, 10))
         raspicamcontrol_set_contrast(camera, parameter_option);
      else
      {
         raspicamcontrol_set_contrast(camera, 0);
         parameter++;
      }
   }
   else
   if (parameter == 2)
   {
      // brightness
      if (update_cycle_parameter(&parameter_option, 0, 100, 10))
         raspicamcontrol_set_brightness(camera, parameter_option);
      else
      {
         raspicamcontrol_set_brightness(camera, 50);
         parameter++;
      }
   }
   else
   if (parameter == 3)
   {
      // contrast
      if (update_cycle_parameter(&parameter_option, -100, 100, 10))
         raspicamcontrol_set_saturation(camera, parameter_option);
      else
      {
         parameter++;
         raspicamcontrol_set_saturation(camera, 0);
      }
   }
   else
   if (parameter == 4)
   {
      // EV
      if (update_cycle_parameter(&parameter_option, -10, 10, 4))
         raspicamcontrol_set_exposure_compensation(camera, parameter_option);
      else
      {
         raspicamcontrol_set_exposure_compensation(camera, 0);
         parameter++;
      }
   }
   else
   if (parameter == 5)
   {
      // MMAL_PARAM_EXPOSUREMODE_T
      if (update_cycle_parameter(&parameter_option, 0, exposure_map_size, 1))
         raspicamcontrol_set_exposure_mode(camera, exposure_map[parameter_option].mmal_mode);
      else
      {
         raspicamcontrol_set_exposure_mode(camera, MMAL_PARAM_EXPOSUREMODE_AUTO);
         parameter++;
      }
   }
   else
   if (parameter == 6)
   {
      // MMAL_PARAM_AWB_T
      if (update_cycle_parameter(&parameter_option, 0, awb_map_size, 1))
         raspicamcontrol_set_awb_mode(camera, awb_map[parameter_option].mmal_mode);
      else
      {
         raspicamcontrol_set_awb_mode(camera, MMAL_PARAM_AWBMODE_AUTO);
         parameter++;
      }
   }
   if (parameter == 7)
   {
      // MMAL_PARAM_IMAGEFX_T
      if (update_cycle_parameter(&parameter_option, 0, imagefx_map_size, 1))
         raspicamcontrol_set_imageFX(camera, imagefx_map[parameter_option].mmal_mode);
      else
      {
         raspicamcontrol_set_imageFX(camera, MMAL_PARAM_IMAGEFX_NONE);
         parameter++;
      }
   }
   if (parameter == 8)
   {
      MMAL_PARAM_COLOURFX_T colfx = {0,0,0};
      switch (parameter_option)
      {
         case parameter_reset :
            parameter_option = 1;
            colfx.u = 128;
            colfx.v = 128;
            break;
         case 1 :
            parameter_option = 2;
            colfx.u = 100;
            colfx.v = 200;
            break;
         case 2 :
            parameter_option = parameter_reset;
            colfx.enable = 0;
            parameter++;
            break;
      }
      raspicamcontrol_set_colourFX(camera, &colfx);
   }

   // Orientation
   if (parameter == 9)
   {
      switch (parameter_option)
      {
      case parameter_reset:
         raspicamcontrol_set_rotation(camera, 90);
         parameter_option = 1;
         break;

      case 1 :
         raspicamcontrol_set_rotation(camera, 180);
         parameter_option = 2;
         break;

      case 2 :
         raspicamcontrol_set_rotation(camera, 270);
         parameter_option = 3;
         break;

      case 3 :
      {
         raspicamcontrol_set_rotation(camera, 0);
         raspicamcontrol_set_flips(camera, 1,0);
         parameter_option = 4;
         break;
      }
      case 4 :
      {
         raspicamcontrol_set_flips(camera, 0,1);
         parameter_option = 5;
         break;
      }
      case 5 :
      {
         raspicamcontrol_set_flips(camera, 1, 1);
         parameter_option = 6;
         break;
      }
      case 6 :
      {
         raspicamcontrol_set_flips(camera, 0, 0);
         parameter_option = parameter_reset;
         parameter++;
         break;
      }
      }
   }

   if (parameter == 10)
   {
      parameter = 1;
      return 0;
   }

   return 1;
}


/**
 * Function to take a string, a mapping, and return the int equivalent
 * @param str Incoming string to match
 * @param map Mapping data
 * @param num_refs The number of items in the mapping data
 * @return The integer match for the string, or -1 if no match
 */
static int map_xref(const char *str, const XREF_T *map, int num_refs)
{
	int i;

   for (i=0;i<num_refs;i++)
   {
      if (!strcasecmp(str, map[i].mode))
      {
         return map[i].mmal_mode;
      }
   }
   return -1;
}

/**
 * Function to take a mmal enum (as int) and return the string equivalent
 * @param en Incoming int to match
 * @param map Mapping data
 * @param num_refs The number of items in the mapping data
 * @return const pointer to string, or NULL if no match
 */
static const char *unmap_xref(const int en, XREF_T *map, int num_refs)
{
   int i;

   for (i=0;i<num_refs;i++)
   {
      if (en == map[i].mmal_mode)
      {
         return map[i].mode;
      }
   }
   return NULL;
}

/**
 * Convert string to the MMAL parameter for exposure mode
 * @param str Incoming string to match
 * @return MMAL parameter matching the string, or the AUTO option if no match found
 */
static MMAL_PARAM_EXPOSUREMODE_T exposure_mode_from_string(const char *str)
{
   int i = map_xref(str, exposure_map, exposure_map_size);

   if( i != -1)
      return (MMAL_PARAM_EXPOSUREMODE_T)i;

   fprintf(stderr,"Unknown exposure mode: %s", str);
   return MMAL_PARAM_EXPOSUREMODE_AUTO;
}

/**
 * Convert string to the MMAL parameter for AWB mode
 * @param str Incoming string to match
 * @return MMAL parameter matching the string, or the AUTO option if no match found
 */
static MMAL_PARAM_AWBMODE_T awb_mode_from_string(const char *str)
{
   int i = map_xref(str, awb_map, awb_map_size);

   if( i != -1)
      return (MMAL_PARAM_AWBMODE_T)i;

   fprintf(stderr,"Unknown awb mode: %s", str);
   return MMAL_PARAM_AWBMODE_AUTO;
}

/**
 * Convert string to the MMAL parameter for image effects mode
 * @param str Incoming string to match
 * @return MMAL parameter matching the strong, or the AUTO option if no match found
 */
MMAL_PARAM_IMAGEFX_T imagefx_mode_from_string(const char *str)
{
   int i = map_xref(str, imagefx_map, imagefx_map_size);

   if( i != -1)
     return (MMAL_PARAM_IMAGEFX_T)i;

   fprintf(stderr,"Unknown image fx: %s", str);
   return MMAL_PARAM_IMAGEFX_NONE;
}

/**
 * Convert string to the MMAL parameter for exposure metering mode
 * @param str Incoming string to match
 * @return MMAL parameter matching the string, or the AUTO option if no match found
 */
static MMAL_PARAM_EXPOSUREMETERINGMODE_T metering_mode_from_string(const char *str)
{
   int i = map_xref(str, metering_mode_map, metering_mode_map_size);

   if( i != -1)
      return (MMAL_PARAM_EXPOSUREMETERINGMODE_T)i;

   fprintf(stderr,"Unknown metering mode: %s", str);
   return MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE;
}

/**
 * Dump contents of camera parameter structure to stdout for debugging/verbose logging
 *
 * @param params Const pointer to parameters structure to dump
 */
void raspicamcontrol_dump_parameters(const RASPICAM_CAMERA_PARAMETERS *params)
{
   const char *exp_mode = unmap_xref(params->exposureMode, exposure_map, exposure_map_size);
   const char *awb_mode = unmap_xref(params->awbMode, awb_map, awb_map_size);
   const char *image_effect = unmap_xref(params->imageEffect, imagefx_map, imagefx_map_size);
   const char *metering_mode = unmap_xref(params->exposureMeterMode, metering_mode_map, metering_mode_map_size);

   fprintf(stderr, "Sharpness %d, Contrast %d, Brightness %d\n", params->sharpness, params->contrast, params->brightness);
   fprintf(stderr, "Saturation %d, ISO %d, Video Stabilisation %s, Exposure compensation %d\n", params->saturation, params->ISO, params->videoStabilisation ? "Yes": "No", params->exposureCompensation);
   fprintf(stderr, "Exposure Mode '%s', AWB Mode '%s', Image Effect '%s'\n", exp_mode, awb_mode, image_effect);
   fprintf(stderr, "Metering Mode '%s', Colour Effect Enabled %s with U = %d, V = %d\n", metering_mode, params->colourEffects.enable ? "Yes":"No", params->colourEffects.u, params->colourEffects.v);
   fprintf(stderr, "Rotation %d, hflip %s, vflip %s\n", params->rotation, params->hflip ? "Yes":"No",params->vflip ? "Yes":"No");
}

/**
 * Convert a MMAL status return value to a simple boolean of success
 * ALso displays a fault if code is not success
 *
 * @param status The error code to convert
 * @return 0 if status is sucess, 1 otherwise
 */
int mmal_status_to_int(MMAL_STATUS_T status)
{
   if (status == MMAL_SUCCESS)
      return 0;
   else
   {
      switch (status)
      {
      case MMAL_ENOMEM :   fprintf(stderr,"Out of memory"); break;
      case MMAL_ENOSPC :   fprintf(stderr,"Out of resources (other than memory)"); break;
      case MMAL_EINVAL:    fprintf(stderr,"Argument is invalid"); break;
      case MMAL_ENOSYS :   fprintf(stderr,"Function not implemented"); break;
      case MMAL_ENOENT :   fprintf(stderr,"No such file or directory"); break;
      case MMAL_ENXIO :    fprintf(stderr,"No such device or address"); break;
      case MMAL_EIO :      fprintf(stderr,"I/O error"); break;
      case MMAL_ESPIPE :   fprintf(stderr,"Illegal seek"); break;
      case MMAL_ECORRUPT : fprintf(stderr,"Data is corrupt \attention FIXME: not POSIX"); break;
      case MMAL_ENOTREADY :fprintf(stderr,"Component is not ready \attention FIXME: not POSIX"); break;
      case MMAL_ECONFIG :  fprintf(stderr,"Component is not configured \attention FIXME: not POSIX"); break;
      case MMAL_EISCONN :  fprintf(stderr,"Port is already connected "); break;
      case MMAL_ENOTCONN : fprintf(stderr,"Port is disconnected"); break;
      case MMAL_EAGAIN :   fprintf(stderr,"Resource temporarily unavailable. Try again later"); break;
      case MMAL_EFAULT :   fprintf(stderr,"Bad address"); break;
      default :            fprintf(stderr,"Unknown status error"); break;
      }

      return 1;
   }
}

/**
 * Give the supplied parameter block a set of default values
 * @params Pointer to parameter block
 */
void raspicamcontrol_set_defaults(RASPICAM_CAMERA_PARAMETERS *params)
{
   vcos_assert(params);

   params->sharpness = 0;
   params->contrast = 0;
   params->brightness = 50;
   params->saturation = 0;
   params->ISO = 400;
   params->videoStabilisation = 0;
   params->exposureCompensation = 0;
   params->exposureMode = MMAL_PARAM_EXPOSUREMODE_AUTO;
   params->exposureMeterMode = MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE;
   params->awbMode = MMAL_PARAM_AWBMODE_AUTO;
   params->imageEffect = MMAL_PARAM_IMAGEFX_NONE;
   params->colourEffects.enable = 0;
   params->colourEffects.u = 128;
   params->colourEffects.v = 128;
   params->rotation = 0;
   params->hflip = params->vflip = 0;
}

/**
 * Get all the current camera parameters from specified camera component
 * @param camera Pointer to camera component
 * @param params Pointer to parameter block to accept settings
 * @return 0 if successful, non-zero if unsuccessful
 */
int raspicamcontrol_get_all_parameters(MMAL_COMPONENT_T *camera, RASPICAM_CAMERA_PARAMETERS *params)
{
   vcos_assert(camera);
   vcos_assert(params);

   if (!camera || !params)
      return 1;

/* TODO : Write these get functions
   params->sharpness = raspicamcontrol_get_sharpness(camera);
   params->contrast = raspicamcontrol_get_contrast(camera);
   params->brightness = raspicamcontrol_get_brightness(camera);
   params->saturation = raspicamcontrol_get_saturation(camera);
   params->ISO = raspicamcontrol_get_ISO(camera);
   params->videoStabilisation = raspicamcontrol_get_video_stabilisation(camera);
   params->exposureCompensation = raspicamcontrol_get_exposure_compensation(camera);
   params->exposureMode = raspicamcontrol_get_exposure_mode(camera);
   params->awbMode = raspicamcontrol_get_awb_mode(camera);
   params->imageEffect = raspicamcontrol_get_image_effect(camera);
   params->colourEffects = raspicamcontrol_get_colour_effect(camera);
   params->thumbnailConfig = raspicamcontrol_get_thumbnail_config(camera);
*/
   return 0;
}

/**
 * Set the specified camera to all the specified settings
 * @param camera Pointer to camera component
 * @param params Pointer to parameter block containing parameters
 * @return 0 if successful, none-zero if unsuccessful.
 */
int raspicamcontrol_set_all_parameters(MMAL_COMPONENT_T *camera, const RASPICAM_CAMERA_PARAMETERS *params)
{
   int result;

   result  = raspicamcontrol_set_saturation(camera, params->saturation);
   result += raspicamcontrol_set_sharpness(camera, params->sharpness);
   result += raspicamcontrol_set_contrast(camera, params->contrast);
   result += raspicamcontrol_set_brightness(camera, params->brightness);
   //result += raspicamcontrol_set_ISO(camera, params->ISO); TODO Not working for some reason
   result += raspicamcontrol_set_video_stabilisation(camera, params->videoStabilisation);
   result += raspicamcontrol_set_exposure_compensation(camera, params->exposureCompensation);
   result += raspicamcontrol_set_exposure_mode(camera, params->exposureMode);
   result += raspicamcontrol_set_metering_mode(camera, params->exposureMeterMode);
   result += raspicamcontrol_set_awb_mode(camera, params->awbMode);
   result += raspicamcontrol_set_imageFX(camera, params->imageEffect);
   result += raspicamcontrol_set_colourFX(camera, &params->colourEffects);
   //result += raspicamcontrol_set_thumbnail_parameters(camera, &params->thumbnailConfig);  TODO Not working for some reason
   result += raspicamcontrol_set_rotation(camera, params->rotation);
   result += raspicamcontrol_set_flips(camera, params->hflip, params->vflip);

   return result;
}

/**
 * Adjust the saturation level for images
 * @param camera Pointer to camera component
 * @param saturation Value to adjust, -100 to 100
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_saturation(MMAL_COMPONENT_T *camera, int saturation)
{
   int ret = 0;

   if (!camera)
      return 1;

   if (saturation >= -100 && saturation <= 100)
   {
      MMAL_RATIONAL_T value = {saturation, 100};
      ret = mmal_status_to_int(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_SATURATION, value));
   }
   else
   {
      fprintf(stderr,"Invalid saturation value");
      ret = 1;
   }

   return ret;
}

/**
 * Set the sharpness of the image
 * @param camera Pointer to camera component
 * @param sharpness Sharpness adjustment -100 to 100
 */
int raspicamcontrol_set_sharpness(MMAL_COMPONENT_T *camera, int sharpness)
{
   int ret = 0;

   if (!camera)
      return 1;

   if (sharpness >= -100 && sharpness <= 100)
   {
      MMAL_RATIONAL_T value = {sharpness, 100};
      ret = mmal_status_to_int(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_SHARPNESS, value));
   }
   else
   {
      fprintf(stderr,"Invalid sharpness value");
      ret = 1;
   }

   return ret;
}

/**
 * Set the contrast adjustment for the image
 * @param camera Pointer to camera component
 * @param contrast Contrast adjustment -100 to  100
 * @return
 */
int raspicamcontrol_set_contrast(MMAL_COMPONENT_T *camera, int contrast)
{
   int ret = 0;

   if (!camera)
      return 1;

   if (contrast >= -100 && contrast <= 100)
   {
      MMAL_RATIONAL_T value = {contrast, 100};
      ret = mmal_status_to_int(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_CONTRAST, value));
   }
   else
   {
      fprintf(stderr,"Invalid contrast value");
      ret = 1;
   }

   return ret;
}

/**
 * Adjust the brightness level for images
 * @param camera Pointer to camera component
 * @param brightness Value to adjust, 0 to 100
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_brightness(MMAL_COMPONENT_T *camera, int brightness)
{
   int ret = 0;

   if (!camera)
      return 1;

   if (brightness >= 0 && brightness <= 100)
   {
      MMAL_RATIONAL_T value = {brightness, 100};
      ret = mmal_status_to_int(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_BRIGHTNESS, value));
   }
   else
   {
      fprintf(stderr,"Invalid brightness value");
      ret = 1;
   }

   return ret;
}

/**
 * Adjust the ISO used for images
 * @param camera Pointer to camera component
 * @param ISO Value to set TODO :
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_ISO(MMAL_COMPONENT_T *camera, int ISO)
{
   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_ISO, ISO));
}

/**
 * Adjust the metering mode for images
 * @param camera Pointer to camera component
 * @param saturation Value from following
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_BACKLIT,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_MATRIX
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_metering_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMETERINGMODE_T m_mode )
{
   MMAL_PARAMETER_EXPOSUREMETERINGMODE_T meter_mode = {{MMAL_PARAMETER_EXP_METERING_MODE,sizeof(meter_mode)},
                                                      m_mode};
   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &meter_mode.hdr));
}


/**
 * Set the video stabilisation flag. Only used in video mode
 * @param camera Pointer to camera component
 * @param saturation Flag 0 off 1 on
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_video_stabilisation(MMAL_COMPONENT_T *camera, int vstabilisation)
{
   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set_boolean(camera->control, MMAL_PARAMETER_VIDEO_STABILISATION, vstabilisation));
}

/**
 * Adjust the exposure compensation for images (EV)
 * @param camera Pointer to camera component
 * @param exp_comp Value to adjust, -10 to +10
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_exposure_compensation(MMAL_COMPONENT_T *camera, int exp_comp)
{
   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set_int32(camera->control, MMAL_PARAMETER_EXPOSURE_COMP , exp_comp));
}


/**
 * Set exposure mode for images
 * @param camera Pointer to camera component
 * @param mode Exposure mode to set from
 *   - MMAL_PARAM_EXPOSUREMODE_OFF,
 *   - MMAL_PARAM_EXPOSUREMODE_AUTO,
 *   - MMAL_PARAM_EXPOSUREMODE_NIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_NIGHTPREVIEW,
 *   - MMAL_PARAM_EXPOSUREMODE_BACKLIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_SPOTLIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_SPORTS,
 *   - MMAL_PARAM_EXPOSUREMODE_SNOW,
 *   - MMAL_PARAM_EXPOSUREMODE_BEACH,
 *   - MMAL_PARAM_EXPOSUREMODE_VERYLONG,
 *   - MMAL_PARAM_EXPOSUREMODE_FIXEDFPS,
 *   - MMAL_PARAM_EXPOSUREMODE_ANTISHAKE,
 *   - MMAL_PARAM_EXPOSUREMODE_FIREWORKS,
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_exposure_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMODE_T mode)
{
   MMAL_PARAMETER_EXPOSUREMODE_T exp_mode = {{MMAL_PARAMETER_EXPOSURE_MODE,sizeof(exp_mode)}, mode};

   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &exp_mode.hdr));
}


/**
 * Set the aWB (auto white balance) mode for images
 * @param camera Pointer to camera component
 * @param awb_mode Value to set from
 *   - MMAL_PARAM_AWBMODE_OFF,
 *   - MMAL_PARAM_AWBMODE_AUTO,
 *   - MMAL_PARAM_AWBMODE_SUNLIGHT,
 *   - MMAL_PARAM_AWBMODE_CLOUDY,
 *   - MMAL_PARAM_AWBMODE_SHADE,
 *   - MMAL_PARAM_AWBMODE_TUNGSTEN,
 *   - MMAL_PARAM_AWBMODE_FLUORESCENT,
 *   - MMAL_PARAM_AWBMODE_INCANDESCENT,
 *   - MMAL_PARAM_AWBMODE_FLASH,
 *   - MMAL_PARAM_AWBMODE_HORIZON,
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_awb_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_AWBMODE_T awb_mode)
{
   MMAL_PARAMETER_AWBMODE_T param = {{MMAL_PARAMETER_AWB_MODE,sizeof(param)}, awb_mode};

   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &param.hdr));
}

/**
 * Set the image effect for the images
 * @param camera Pointer to camera component
 * @param imageFX Value from
 *   - MMAL_PARAM_IMAGEFX_NONE,
 *   - MMAL_PARAM_IMAGEFX_NEGATIVE,
 *   - MMAL_PARAM_IMAGEFX_SOLARIZE,
 *   - MMAL_PARAM_IMAGEFX_POSTERIZE,
 *   - MMAL_PARAM_IMAGEFX_WHITEBOARD,
 *   - MMAL_PARAM_IMAGEFX_BLACKBOARD,
 *   - MMAL_PARAM_IMAGEFX_SKETCH,
 *   - MMAL_PARAM_IMAGEFX_DENOISE,
 *   - MMAL_PARAM_IMAGEFX_EMBOSS,
 *   - MMAL_PARAM_IMAGEFX_OILPAINT,
 *   - MMAL_PARAM_IMAGEFX_HATCH,
 *   - MMAL_PARAM_IMAGEFX_GPEN,
 *   - MMAL_PARAM_IMAGEFX_PASTEL,
 *   - MMAL_PARAM_IMAGEFX_WATERCOLOUR,
 *   - MMAL_PARAM_IMAGEFX_FILM,
 *   - MMAL_PARAM_IMAGEFX_BLUR,
 *   - MMAL_PARAM_IMAGEFX_SATURATION,
 *   - MMAL_PARAM_IMAGEFX_COLOURSWAP,
 *   - MMAL_PARAM_IMAGEFX_WASHEDOUT,
 *   - MMAL_PARAM_IMAGEFX_POSTERISE,
 *   - MMAL_PARAM_IMAGEFX_COLOURPOINT,
 *   - MMAL_PARAM_IMAGEFX_COLOURBALANCE,
 *   - MMAL_PARAM_IMAGEFX_CARTOON,
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_imageFX(MMAL_COMPONENT_T *camera, MMAL_PARAM_IMAGEFX_T imageFX)
{
   MMAL_PARAMETER_IMAGEFX_T imgFX = {{MMAL_PARAMETER_IMAGE_EFFECT,sizeof(imgFX)}, imageFX};

   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &imgFX.hdr));
}

/* TODO :what to do with the image effects parameters?
   MMAL_PARAMETER_IMAGEFX_PARAMETERS_T imfx_param = {{MMAL_PARAMETER_IMAGE_EFFECT_PARAMETERS,sizeof(imfx_param)},
                              imageFX, 0, {0}};
mmal_port_parameter_set(camera->control, &imfx_param.hdr);
                             */

/**
 * Set the colour effect  for images (Set UV component)
 * @param camera Pointer to camera component
 * @param colourFX  Contains enable state and U and V numbers to set (e.g. 128,128 = Black and white)
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_colourFX(MMAL_COMPONENT_T *camera, const MMAL_PARAM_COLOURFX_T *colourFX)
{
   MMAL_PARAMETER_COLOURFX_T colfx = {{MMAL_PARAMETER_COLOUR_EFFECT,sizeof(colfx)}, 0, 0, 0};

   if (!camera)
      return 1;

   colfx.enable = colourFX->enable;
   colfx.u = colourFX->u;
   colfx.v = colourFX->v;

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &colfx.hdr));

}


/**
 * Set the rotation of the image
 * @param camera Pointer to camera component
 * @param rotation Degree of rotation (any number, but will be converted to 0,90,180 or 270 only)
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_rotation(MMAL_COMPONENT_T *camera, int rotation)
{
   int ret;
   int my_rotation = ((rotation % 360 ) / 90) * 90;

   ret = mmal_port_parameter_set_int32(camera->output[0], MMAL_PARAMETER_ROTATION, my_rotation);
   mmal_port_parameter_set_int32(camera->output[1], MMAL_PARAMETER_ROTATION, my_rotation);
   mmal_port_parameter_set_int32(camera->output[2], MMAL_PARAMETER_ROTATION, my_rotation);

   return ret;
}

/**
 * Set the flips state of the image
 * @param camera Pointer to camera component
 * @param hflip If true, horizontally flip the image
 * @param vflip If true, vertically flip the image
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_flips(MMAL_COMPONENT_T *camera, int hflip, int vflip)
{
   MMAL_PARAMETER_MIRROR_T mirror = {{MMAL_PARAMETER_MIRROR, sizeof(MMAL_PARAMETER_MIRROR_T)}, MMAL_PARAM_MIRROR_NONE};

   if (hflip && vflip)
      mirror.value = MMAL_PARAM_MIRROR_BOTH;
   else
   if (hflip)
      mirror.value = MMAL_PARAM_MIRROR_HORIZONTAL;
   else
   if (vflip)
      mirror.value = MMAL_PARAM_MIRROR_VERTICAL;

   mmal_port_parameter_set(camera->output[0], &mirror.hdr);
   mmal_port_parameter_set(camera->output[1], &mirror.hdr);
   return mmal_port_parameter_set(camera->output[2], &mirror.hdr);
}

static int raspicamcontrol_get_mem_gpu(void)
{
   char response[80] = "";
   int gpu_mem = 0;
   if (vc_gencmd(response, sizeof response, "get_mem gpu") == 0)
      vc_gencmd_number_property(response, "gpu", &gpu_mem);
   return gpu_mem;
}

static void raspicamcontrol_get_camera(int *supported, int *detected)
{
   char response[80] = "";
   if (vc_gencmd(response, sizeof response, "get_camera") == 0)
   {
      if (supported)
         vc_gencmd_number_property(response, "supported", supported);
      if (detected)
         vc_gencmd_number_property(response, "detected", detected);
   }
}

void raspicamcontrol_check_configuration(int min_gpu_mem)
{
   int gpu_mem = raspicamcontrol_get_mem_gpu();
   int supported = 0, detected = 0;
   raspicamcontrol_get_camera(&supported, &detected);
   if (!supported)
      fprintf(stderr,"Camera is not enabled in this build. Try running \"sudo raspi-config\" and ensure that \"camera\" has been enabled\n");
   else if (gpu_mem < min_gpu_mem)
      fprintf(stderr,"Only %dM of gpu_mem is configured. Try running \"sudo raspi-config\" and ensure that \"memory_split\" has a value of %d or greater\n", gpu_mem, min_gpu_mem);
   else if (!detected)
      fprintf(stderr,"Camera is not detected. Please check carefully the camera module is installed correctly\n");
   else
      fprintf(stderr,"Failed to run camera app. Please check for firmware updates\n");
}


