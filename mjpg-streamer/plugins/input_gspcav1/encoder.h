/***************************************************************************#
# jpegenc: library to encode a jpeg frame from various input palette.       #
# jpegenc works for embedded device without libjpeg                         #
#.                                                                          #
#       Copyright (C) 2005 Michel Xhaard                            #
#                                                                           #
# This program is free software; you can redistribute it and/or modify      #
# it under the terms of the GNU General Public License as published by      #
# the Free Software Foundation; either version 2 of the License, or         #
# (at your option) any later version.                                       #
#                                                                           #
# This program is distributed in the hope that it will be useful,           #
# but WITHOUT ANY WARRANTY; without even the implied warranty of            #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             #
# GNU General Public License for more details.                              #
#                                                                           #
# You should have received a copy of the GNU General Public License         #
# along with this program; if not, write to the Free Software               #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA #
#  CREDIT:                                  #
# Original code from Nitin Gupta India (?)                      #
#                                                                           #
#***************************************************************************/


#ifndef ENCODER_H
#define ENCODER_H
/********* Image_format know by the encoder *********************/
/* Native YUV Packet */
#define     FOUR_ZERO_ZERO  0   // Grey scale Y00 ...
#define     FOUR_TWO_ZERO   1   //Y00 Y01 Y10 Y11 Cb Cr
#define     FOUR_TWO_TWO    2   //Y00 Cb Y01 Cr
#define     FOUR_FOUR_FOUR  3   //Y00 Cb Cr Y01 Cb Cr
/* transform RGB24 to YUV Packet*/
#define     RGBto444    4   //RGB24 to packet YUV444 
#define     RGBto422    5   //RGB24 to packet YUV422
#define     RGBto420    6   //RGB24 to packet YUV420
#define     RGBto400    7   //RGB24 to packet YUV400
/* transform RGBxxx to YUV Packet*/
#define     RGB565to420 11  //RGB565 to packet YUV420
#define     RGB32to420  12  //RGB32 to packet YUV420
/* transform YUV planar to YUV packet */
#define     YUVto444    8   //YUV444Planar to Packet YUV444
#define     YUVto422    9   //YUV422Planar to Packet YUV422
#define     YUVto420    10  //YUV420Planar to Packet YUV420
/*****************************************************************/

#define     BLOCK_SIZE  64

UINT8 Lqt[BLOCK_SIZE];
UINT8 Cqt[BLOCK_SIZE];
UINT16 ILqt[BLOCK_SIZE];
UINT16 ICqt[BLOCK_SIZE];
INT16 Y1[BLOCK_SIZE];
INT16 Y2[BLOCK_SIZE];
INT16 Y3[BLOCK_SIZE];
INT16 Y4[BLOCK_SIZE];
INT16 CB[BLOCK_SIZE];
INT16 CR[BLOCK_SIZE];
INT16 Temp[BLOCK_SIZE];
INT32 lcode;
UINT16 bitindex;
typedef struct JPEG_ENCODER_STRUCTURE {
    UINT16 mcu_width;
    UINT16 mcu_height;
    UINT16 horizontal_mcus;
    UINT16 vertical_mcus;
    UINT16 rows_in_bottom_mcus;
    UINT16 cols_in_right_mcus;
    UINT16 length_minus_mcu_width;
    UINT16 length_minus_width;
    UINT16 mcu_width_size;
    UINT16 offset;
    INT16 ldc1;
    INT16 ldc2;
    INT16 ldc3;
    UINT16 rows;
    UINT16 cols;
    UINT16 incr;
} JPEG_ENCODER_STRUCTURE;

/* encode picture input to output
quality factor
image_format look the define
image_width image_height of the input picture
return the encoded size in Byte
*/
UINT32 encode_image(UINT8 * input_ptr, UINT8 * output_ptr,
                    UINT32 quality_factor, UINT32 image_format,
                    UINT32 image_width, UINT32 image_height);

#endif  /* ENCODER_H */
