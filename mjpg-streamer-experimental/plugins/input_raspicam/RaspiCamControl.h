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

#ifndef RASPICAMCONTROL_H_
#define RASPICAMCONTROL_H_

/* Various parameters
 *
 * Exposure Mode
 *          MMAL_PARAM_EXPOSUREMODE_OFF,
            MMAL_PARAM_EXPOSUREMODE_AUTO,
            MMAL_PARAM_EXPOSUREMODE_NIGHT,
            MMAL_PARAM_EXPOSUREMODE_NIGHTPREVIEW,
            MMAL_PARAM_EXPOSUREMODE_BACKLIGHT,
            MMAL_PARAM_EXPOSUREMODE_SPOTLIGHT,
            MMAL_PARAM_EXPOSUREMODE_SPORTS,
            MMAL_PARAM_EXPOSUREMODE_SNOW,
            MMAL_PARAM_EXPOSUREMODE_BEACH,
            MMAL_PARAM_EXPOSUREMODE_VERYLONG,
            MMAL_PARAM_EXPOSUREMODE_FIXEDFPS,
            MMAL_PARAM_EXPOSUREMODE_ANTISHAKE,
            MMAL_PARAM_EXPOSUREMODE_FIREWORKS,
 *
 * AWB Mode
 *          MMAL_PARAM_AWBMODE_OFF,
            MMAL_PARAM_AWBMODE_AUTO,
            MMAL_PARAM_AWBMODE_SUNLIGHT,
            MMAL_PARAM_AWBMODE_CLOUDY,
            MMAL_PARAM_AWBMODE_SHADE,
            MMAL_PARAM_AWBMODE_TUNGSTEN,
            MMAL_PARAM_AWBMODE_FLUORESCENT,
            MMAL_PARAM_AWBMODE_INCANDESCENT,
            MMAL_PARAM_AWBMODE_FLASH,
            MMAL_PARAM_AWBMODE_HORIZON,
 *
 * Image FX
            MMAL_PARAM_IMAGEFX_NONE,
            MMAL_PARAM_IMAGEFX_NEGATIVE,
            MMAL_PARAM_IMAGEFX_SOLARIZE,
            MMAL_PARAM_IMAGEFX_POSTERIZE,
            MMAL_PARAM_IMAGEFX_WHITEBOARD,
            MMAL_PARAM_IMAGEFX_BLACKBOARD,
            MMAL_PARAM_IMAGEFX_SKETCH,
            MMAL_PARAM_IMAGEFX_DENOISE,
            MMAL_PARAM_IMAGEFX_EMBOSS,
            MMAL_PARAM_IMAGEFX_OILPAINT,
            MMAL_PARAM_IMAGEFX_HATCH,
            MMAL_PARAM_IMAGEFX_GPEN,
            MMAL_PARAM_IMAGEFX_PASTEL,
            MMAL_PARAM_IMAGEFX_WATERCOLOUR,
            MMAL_PARAM_IMAGEFX_FILM,
            MMAL_PARAM_IMAGEFX_BLUR,
            MMAL_PARAM_IMAGEFX_SATURATION,
            MMAL_PARAM_IMAGEFX_COLOURSWAP,
            MMAL_PARAM_IMAGEFX_WASHEDOUT,
            MMAL_PARAM_IMAGEFX_POSTERISE,
            MMAL_PARAM_IMAGEFX_COLOURPOINT,
            MMAL_PARAM_IMAGEFX_COLOURBALANCE,
            MMAL_PARAM_IMAGEFX_CARTOON,

 */



// There isn't actually a MMAL structure for the following, so make one
typedef struct
{
   int enable;       /// Turn colourFX on or off
   int u,v;          /// U and V to use
} MMAL_PARAM_COLOURFX_T;

typedef struct
{
   int enable;
   int width,height;
   int quality;
} MMAL_PARAM_THUMBNAIL_CONFIG_T;

/// struct contain camera settings
typedef struct
{
   int sharpness;             /// -100 to 100
   int contrast;              /// -100 to 100
   int brightness;            ///  0 to 100
   int saturation;            ///  -100 to 100
   int ISO;                   ///  TODO : what range?
   int videoStabilisation;    /// 0 or 1 (false or true)
   int exposureCompensation;  /// -10 to +10 ?
   MMAL_PARAM_EXPOSUREMODE_T exposureMode;
   MMAL_PARAM_EXPOSUREMETERINGMODE_T exposureMeterMode;
   MMAL_PARAM_AWBMODE_T awbMode;
   MMAL_PARAM_IMAGEFX_T imageEffect;
   MMAL_PARAMETER_IMAGEFX_PARAMETERS_T imageEffectsParameters;
   MMAL_PARAM_COLOURFX_T colourEffects;
   int rotation;              /// 0-359
   int hflip;                 /// 0 or 1
   int vflip;                 /// 0 or 1
} RASPICAM_CAMERA_PARAMETERS;

int raspicamcontrol_parse_cmdline(RASPICAM_CAMERA_PARAMETERS *params, const char *arg1, const char *arg2);
void raspicamcontrol_display_help();
int raspicamcontrol_cycle_test(MMAL_COMPONENT_T *camera);

int raspicamcontrol_set_all_parameters(MMAL_COMPONENT_T *camera, const RASPICAM_CAMERA_PARAMETERS *params);
int raspicamcontrol_get_all_parameters(MMAL_COMPONENT_T *camera, RASPICAM_CAMERA_PARAMETERS *params);
void raspicamcontrol_dump_parameters(const RASPICAM_CAMERA_PARAMETERS *params);

void raspicamcontrol_set_defaults(RASPICAM_CAMERA_PARAMETERS *params);

void raspicamcontrol_check_configuration(int min_gpu_mem);

// Individual setting functions
int raspicamcontrol_set_saturation(MMAL_COMPONENT_T *camera, int saturation);
int raspicamcontrol_set_sharpness(MMAL_COMPONENT_T *camera, int sharpness);
int raspicamcontrol_set_contrast(MMAL_COMPONENT_T *camera, int contrast);
int raspicamcontrol_set_brightness(MMAL_COMPONENT_T *camera, int brightness);
int raspicamcontrol_set_ISO(MMAL_COMPONENT_T *camera, int ISO);
int raspicamcontrol_set_metering_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMETERINGMODE_T mode);
int raspicamcontrol_set_video_stabilisation(MMAL_COMPONENT_T *camera, int vstabilisation);
int raspicamcontrol_set_exposure_compensation(MMAL_COMPONENT_T *camera, int exp_comp);
int raspicamcontrol_set_exposure_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMODE_T mode);
int raspicamcontrol_set_awb_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_AWBMODE_T awb_mode);
int raspicamcontrol_set_imageFX(MMAL_COMPONENT_T *camera, MMAL_PARAM_IMAGEFX_T imageFX);
int raspicamcontrol_set_colourFX(MMAL_COMPONENT_T *camera, const MMAL_PARAM_COLOURFX_T *colourFX);
int raspicamcontrol_set_rotation(MMAL_COMPONENT_T *camera, int rotation);
int raspicamcontrol_set_flips(MMAL_COMPONENT_T *camera, int hflip, int vflip);

//Individual getting functions
int raspicamcontrol_get_saturation(MMAL_COMPONENT_T *camera);
int raspicamcontrol_get_sharpness(MMAL_COMPONENT_T *camera);
int raspicamcontrol_get_contrast(MMAL_COMPONENT_T *camera);
int raspicamcontrol_get_brightness(MMAL_COMPONENT_T *camera);
int raspicamcontrol_get_ISO(MMAL_COMPONENT_T *camera);
MMAL_PARAM_EXPOSUREMETERINGMODE_T raspicamcontrol_get_metering_mode(MMAL_COMPONENT_T *camera);
int raspicamcontrol_get_video_stabilisation(MMAL_COMPONENT_T *camera);
int raspicamcontrol_get_exposure_compensation(MMAL_COMPONENT_T *camera);
MMAL_PARAM_THUMBNAIL_CONFIG_T raspicamcontrol_get_thumbnail_parameters(MMAL_COMPONENT_T *camera);
MMAL_PARAM_EXPOSUREMODE_T raspicamcontrol_get_exposure_mode(MMAL_COMPONENT_T *camera);
MMAL_PARAM_AWBMODE_T raspicamcontrol_get_awb_mode(MMAL_COMPONENT_T *camera);
MMAL_PARAM_IMAGEFX_T raspicamcontrol_get_imageFX(MMAL_COMPONENT_T *camera);
MMAL_PARAM_COLOURFX_T raspicamcontrol_get_colourFX(MMAL_COMPONENT_T *camera);


#endif /* RASPICAMCONTROL_H_ */
