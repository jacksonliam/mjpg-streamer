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


#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "jdatatype.h"
#include "encoder.h"
#include "huffman.h"
#include "quant.h"
#include "marker.h"
#include "utils.h"
static void (*read_format)(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
                           UINT8 * input_ptr);
static void read_400_format(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
                            UINT8 * input_ptr);
static void read_420_format(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
                            UINT8 * input_ptr);
static void read_422_format(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
                            UINT8 * input_ptr);
static void read_444_format(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
                            UINT8 * input_ptr);
static void RGB_2_444(UINT8 * input_ptr, UINT8 * output_ptr,
                      UINT32 image_width, UINT32 image_height);
static void RGB_2_422(UINT8 * input_ptr, UINT8 * output_ptr,
                      UINT32 image_width, UINT32 image_height);
static void RGB_2_420(UINT8 * input_ptr, UINT8 * output_ptr,
                      UINT32 image_width, UINT32 image_height);
static void RGB_2_400(UINT8 * input_ptr, UINT8 * output_ptr,
                      UINT32 image_width, UINT32 image_height);
static void RGB565_2_420(UINT8 * input_ptr, UINT8 * output_ptr,
                         UINT32 image_width, UINT32 image_height);
static void RGB32_2_420(UINT8 * input_ptr, UINT8 * output_ptr,
                        UINT32 image_width, UINT32 image_height);
static void YUV_2_444(UINT8 * input_ptr, UINT8 * output_ptr,
                      UINT32 image_width, UINT32 image_height);
static void YUV_2_422(UINT8 * input_ptr, UINT8 * output_ptr,
                      UINT32 image_width, UINT32 image_height);
static void YUV_2_420(UINT8 * input_ptr, UINT8 * output_ptr,
                      UINT32 image_width, UINT32 image_height);
static void DCT(INT16 * data);
static void initialization(JPEG_ENCODER_STRUCTURE * jpeg,
                           UINT32 image_format, UINT32 image_width,
                           UINT32 image_height);
static UINT8 *encodeMCU(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
                        UINT32 image_format, UINT8 * output_ptr);
static void
initialization(JPEG_ENCODER_STRUCTURE * jpeg, UINT32 image_format,
               UINT32 image_width, UINT32 image_height)
{
    UINT16 mcu_width, mcu_height, bytes_per_pixel;
    lcode = 0;
    bitindex = 0;
    if(image_format == FOUR_ZERO_ZERO || image_format == FOUR_FOUR_FOUR)

    {
        jpeg->mcu_width = mcu_width = 8;
        jpeg->mcu_height = mcu_height = 8;
        jpeg->horizontal_mcus = (UINT16)((image_width + mcu_width - 1) >> 3);
        jpeg->vertical_mcus = (UINT16)((image_height + mcu_height - 1) >> 3);
        if(image_format == FOUR_ZERO_ZERO)

        {
            bytes_per_pixel = 1;
            read_format = read_400_format;
        }

        else

        {
            bytes_per_pixel = 3;
            read_format = read_444_format;
        }
    }

    else

    {
        jpeg->mcu_width = mcu_width = 16;
        jpeg->horizontal_mcus = (UINT16)((image_width + mcu_width - 1) >> 4);
        if(image_format == FOUR_TWO_ZERO)

        {
            jpeg->mcu_height = mcu_height = 16;
            jpeg->vertical_mcus =
                (UINT16)((image_height + mcu_height - 1) >> 4);
            bytes_per_pixel = 3;
            read_format = read_420_format;
        }

        else

        {
            jpeg->mcu_height = mcu_height = 8;
            jpeg->vertical_mcus =
                (UINT16)((image_height + mcu_height - 1) >> 3);
            bytes_per_pixel = 2;
            read_format = read_422_format;
        }
    }
    jpeg->rows_in_bottom_mcus =
        (UINT16)(image_height - (jpeg->vertical_mcus - 1) * mcu_height);
    jpeg->cols_in_right_mcus =
        (UINT16)(image_width - (jpeg->horizontal_mcus - 1) * mcu_width);
    jpeg->length_minus_mcu_width =
        (UINT16)((image_width - mcu_width) * bytes_per_pixel);
    jpeg->length_minus_width =
        (UINT16)((image_width - jpeg->cols_in_right_mcus) * bytes_per_pixel);
    jpeg->mcu_width_size = (UINT16)(mcu_width * bytes_per_pixel);
    if(image_format != FOUR_TWO_ZERO)
        jpeg->offset =
            (UINT16)((image_width * (mcu_height - 1) -
                      (mcu_width - jpeg->cols_in_right_mcus)) * bytes_per_pixel);

    else
        jpeg->offset =
            (UINT16)((image_width * ((mcu_height >> 1) - 1) -
                      (mcu_width - jpeg->cols_in_right_mcus)) * bytes_per_pixel);
    jpeg->ldc1 = 0;
    jpeg->ldc2 = 0;
    jpeg->ldc3 = 0;
}

UINT32 encode_image(UINT8 * input_ptr, UINT8 * output_ptr,
                    UINT32 quality_factor, UINT32 image_format,
                    UINT32 image_width, UINT32 image_height)
{
    UINT16 i, j;
    UINT8 * output;
    JPEG_ENCODER_STRUCTURE JpegStruct;
    JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure = &JpegStruct;
    output = output_ptr;
    switch(image_format) {
    case RGBto444:

    {
        image_format = FOUR_FOUR_FOUR;
        RGB_2_444(input_ptr, output_ptr, image_width, image_height);
    }
    break;
    case RGBto422:

    {
        image_format = FOUR_TWO_TWO;
        RGB_2_422(input_ptr, output_ptr, image_width, image_height);
    }
    break;
    case RGBto420:

    {
        image_format = FOUR_TWO_ZERO;
        RGB_2_420(input_ptr, output_ptr, image_width, image_height);
    }
    break;
    case RGB565to420:

    {
        image_format = FOUR_TWO_ZERO;
        RGB565_2_420(input_ptr, output_ptr, image_width, image_height);
    }
    break;
    case RGB32to420:

    {
        image_format = FOUR_TWO_ZERO;
        RGB32_2_420(input_ptr, output_ptr, image_width, image_height);
    }
    break;
    case RGBto400:

    {
        image_format = FOUR_ZERO_ZERO;
        RGB_2_400(input_ptr, output_ptr, image_width, image_height);
    }
    break;
    case YUVto444:

    {
        image_format = FOUR_FOUR_FOUR;
        YUV_2_444(input_ptr, output_ptr, image_width, image_height);
    }
    break;
    case YUVto422:

    {
        image_format = FOUR_TWO_TWO;
        YUV_2_422(input_ptr, output_ptr, image_width, image_height);
    }
    break;
    case YUVto420:

    {
        image_format = FOUR_TWO_ZERO;
        YUV_2_420(input_ptr, output_ptr, image_width, image_height);
    }
    break;
    }

    /* Initialization of JPEG control structure */
    initialization(jpeg_encoder_structure, image_format, image_width,
                   image_height);

    /* Quantization Table Initialization */
    initialize_quantization_tables(quality_factor);

    /* Writing Marker Data */
    output_ptr =
        write_markers(output_ptr, image_format, image_width, image_height);
    for(i = 1; i <= jpeg_encoder_structure->vertical_mcus; i++)

    {
        if(i < jpeg_encoder_structure->vertical_mcus)
            jpeg_encoder_structure->rows = jpeg_encoder_structure->mcu_height;

        else
            jpeg_encoder_structure->rows =
                jpeg_encoder_structure->rows_in_bottom_mcus;
        for(j = 1; j <= jpeg_encoder_structure->horizontal_mcus; j++)

        {
            if(j < jpeg_encoder_structure->horizontal_mcus)

            {
                jpeg_encoder_structure->cols =
                    jpeg_encoder_structure->mcu_width;
                jpeg_encoder_structure->incr =
                    jpeg_encoder_structure->length_minus_mcu_width;
            }

            else

            {
                jpeg_encoder_structure->cols =
                    jpeg_encoder_structure->cols_in_right_mcus;
                jpeg_encoder_structure->incr =
                    jpeg_encoder_structure->length_minus_width;
            }
            read_format(jpeg_encoder_structure, input_ptr);

            /* Encode the data in MCU */
            output_ptr =
                encodeMCU(jpeg_encoder_structure, image_format, output_ptr);
            input_ptr += jpeg_encoder_structure->mcu_width_size;
        }
        input_ptr += jpeg_encoder_structure->offset;
    }

    /* Close Routine */
    output_ptr = close_bitstream(output_ptr);
    return (UINT32)(output_ptr - output);
}
static UINT8 *
encodeMCU(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
          UINT32 image_format, UINT8 * output_ptr)
{
    DCT(Y1);
    quantization(Y1, ILqt);
    output_ptr = huffman(jpeg_encoder_structure, 1, output_ptr);
    if(image_format == FOUR_ZERO_ZERO)
        return output_ptr;
    DCT(Y2);
    quantization(Y2, ILqt);
    output_ptr = huffman(jpeg_encoder_structure, 1, output_ptr);
    if(image_format == FOUR_TWO_TWO)
        goto chroma;
    DCT(Y3);
    quantization(Y3, ILqt);
    output_ptr = huffman(jpeg_encoder_structure, 1, output_ptr);
    DCT(Y4);
    quantization(Y4, ILqt);
    output_ptr = huffman(jpeg_encoder_structure, 1, output_ptr);
chroma: DCT(CB);
    quantization(CB, ICqt);
    output_ptr = huffman(jpeg_encoder_structure, 2, output_ptr);
    DCT(CR);
    quantization(CR, ICqt);
    output_ptr = huffman(jpeg_encoder_structure, 3, output_ptr);
    return output_ptr;
}


/* DCT for One block(8x8) */
static void
DCT(INT16 * data)
{
    UINT16 i;
    INT32 x0, x1, x2, x3, x4, x5, x6, x7, x8;

    /*  All values are shifted left by 10
        and rounded off to nearest integer */
    static const UINT16 c1 = 1420;    /* cos PI/16 * root(2)  */
    static const UINT16 c2 = 1338;    /* cos PI/8 * root(2)   */
    static const UINT16 c3 = 1204;    /* cos 3PI/16 * root(2) */
    static const UINT16 c5 = 805; /* cos 5PI/16 * root(2) */
    static const UINT16 c6 = 554; /* cos 3PI/8 * root(2)  */
    static const UINT16 c7 = 283; /* cos 7PI/16 * root(2) */
    static const UINT16 s1 = 3;
    static const UINT16 s2 = 10;
    static const UINT16 s3 = 13;
    for(i = 8; i > 0; i--)

    {
        x8 = data[0] + data[7];
        x0 = data[0] - data[7];
        x7 = data[1] + data[6];
        x1 = data[1] - data[6];
        x6 = data[2] + data[5];
        x2 = data[2] - data[5];
        x5 = data[3] + data[4];
        x3 = data[3] - data[4];
        x4 = x8 + x5;
        x8 -= x5;
        x5 = x7 + x6;
        x7 -= x6;
        data[0] = (INT16)(x4 + x5);
        data[4] = (INT16)(x4 - x5);
        data[2] = (INT16)((x8 * c2 + x7 * c6) >> s2);
        data[6] = (INT16)((x8 * c6 - x7 * c2) >> s2);
        data[7] = (INT16)((x0 * c7 - x1 * c5 + x2 * c3 - x3 * c1) >> s2);
        data[5] = (INT16)((x0 * c5 - x1 * c1 + x2 * c7 + x3 * c3) >> s2);
        data[3] = (INT16)((x0 * c3 - x1 * c7 - x2 * c1 - x3 * c5) >> s2);
        data[1] = (INT16)((x0 * c1 + x1 * c3 + x2 * c5 + x3 * c7) >> s2);
        data += 8;
    }
    data -= 64;
    for(i = 8; i > 0; i--)

    {
        x8 = data[0] + data[56];
        x0 = data[0] - data[56];
        x7 = data[8] + data[48];
        x1 = data[8] - data[48];
        x6 = data[16] + data[40];
        x2 = data[16] - data[40];
        x5 = data[24] + data[32];
        x3 = data[24] - data[32];
        x4 = x8 + x5;
        x8 -= x5;
        x5 = x7 + x6;
        x7 -= x6;
        data[0] = (INT16)((x4 + x5) >> s1);
        data[32] = (INT16)((x4 - x5) >> s1);
        data[16] = (INT16)((x8 * c2 + x7 * c6) >> s3);
        data[48] = (INT16)((x8 * c6 - x7 * c2) >> s3);
        data[56] = (INT16)((x0 * c7 - x1 * c5 + x2 * c3 - x3 * c1) >> s3);
        data[40] = (INT16)((x0 * c5 - x1 * c1 + x2 * c7 + x3 * c3) >> s3);
        data[24] = (INT16)((x0 * c3 - x1 * c7 - x2 * c1 - x3 * c5) >> s3);
        data[8] = (INT16)((x0 * c1 + x1 * c3 + x2 * c5 + x3 * c7) >> s3);
        data++;
    }
}
static void
read_400_format(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
                UINT8 * input_ptr)
{
    INT32 i, j;
    INT16 *Y1_Ptr = Y1;
    UINT16 rows = jpeg_encoder_structure->rows;
    UINT16 cols = jpeg_encoder_structure->cols;
    UINT16 incr = jpeg_encoder_structure->incr;
    for(i = rows; i > 0; i--) {
        for(j = cols; j > 0; j--)
            *Y1_Ptr++ = *input_ptr++ - 128;
        for(j = 8 - cols; j > 0; j--)
            *Y1_Ptr = *(Y1_Ptr - 1); Y1_Ptr++;
        input_ptr += incr;
    }

    for(i = 8 - rows; i > 0; i--) {
        for(j = 8; j > 0; j--)
            *Y1_Ptr = *(Y1_Ptr - 8); Y1_Ptr++;
    }
}
static void
read_420_format(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
                UINT8 * input_ptr)
{
    INT32 i, j;
    UINT16 Y1_rows, Y3_rows, Y1_cols, Y2_cols;
    INT16 * Y1_Ptr = Y1;
    INT16 * Y2_Ptr = Y2;
    INT16 * Y3_Ptr = Y3;
    INT16 * Y4_Ptr = Y4;
    INT16 * CB_Ptr = CB;
    INT16 * CR_Ptr = CR;
    INT16 * Y1Ptr = Y1 + 8;
    INT16 * Y2Ptr = Y2 + 8;
    INT16 * Y3Ptr = Y3 + 8;
    INT16 * Y4Ptr = Y4 + 8;
    UINT16 rows = jpeg_encoder_structure->rows;
    UINT16 cols = jpeg_encoder_structure->cols;
    UINT16 incr = jpeg_encoder_structure->incr;
    if(rows <= 8)

    {
        Y1_rows = rows;
        Y3_rows = 0;
    }

    else

    {
        Y1_rows = 8;
        Y3_rows = (UINT16)(rows - 8);
    }
    if(cols <= 8)

    {
        Y1_cols = cols;
        Y2_cols = 0;
    }

    else

    {
        Y1_cols = 8;
        Y2_cols = (UINT16)(cols - 8);
    }
    for(i = Y1_rows >> 1; i > 0; i--)

    {
        for(j = Y1_cols >> 1; j > 0; j--)

        {
            *Y1_Ptr++ = *input_ptr++ - 128;
            *Y1_Ptr++ = *input_ptr++ - 128;
            *Y1Ptr++ = *input_ptr++ - 128;
            *Y1Ptr++ = *input_ptr++ - 128;
            *CB_Ptr++ = *input_ptr++ - 128;
            *CR_Ptr++ = *input_ptr++ - 128;
        }
        for(j = Y2_cols >> 1; j > 0; j--)

        {
            *Y2_Ptr++ = *input_ptr++ - 128;
            *Y2_Ptr++ = *input_ptr++ - 128;
            *Y2Ptr++ = *input_ptr++ - 128;
            *Y2Ptr++ = *input_ptr++ - 128;
            *CB_Ptr++ = *input_ptr++ - 128;
            *CR_Ptr++ = *input_ptr++ - 128;
        }
        if(cols <= 8)

        {
            for(j = 8 - Y1_cols; j > 0; j--)

            {
                *Y1_Ptr = *(Y1_Ptr - 1); Y1_Ptr++;
                *Y1Ptr = *(Y1Ptr - 1); Y1Ptr++;
            }
            for(j = 8; j > 0; j--)

            {
                *Y2_Ptr++ = *(Y1_Ptr - 1);
                *Y2Ptr++ = *(Y1Ptr - 1);
            }

        } else

        {
            for(j = 8 - Y2_cols; j > 0; j--)

            {
                *Y2_Ptr = *(Y2_Ptr - 1); Y2_Ptr++;
                *Y2Ptr = *(Y2Ptr - 1);Y2_Ptr++;
            }
        }
        for(j = (16 - cols) >> 1; j > 0; j--)

        {
            *CB_Ptr = *(CB_Ptr - 1); CB_Ptr++;
            *CR_Ptr = *(CR_Ptr - 1); CR_Ptr++;
        }
        Y1_Ptr += 8;
        Y2_Ptr += 8;
        Y1Ptr += 8;
        Y2Ptr += 8;
        input_ptr += incr;
    }
    for(i = Y3_rows >> 1; i > 0; i--)

    {
        for(j = Y1_cols >> 1; j > 0; j--)

        {
            *Y3_Ptr++ = *input_ptr++ - 128;
            *Y3_Ptr++ = *input_ptr++ - 128;
            *Y3Ptr++ = *input_ptr++ - 128;
            *Y3Ptr++ = *input_ptr++ - 128;
            *CB_Ptr++ = *input_ptr++ - 128;
            *CR_Ptr++ = *input_ptr++ - 128;
        }
        for(j = Y2_cols >> 1; j > 0; j--)

        {
            *Y4_Ptr++ = *input_ptr++ - 128;
            *Y4_Ptr++ = *input_ptr++ - 128;
            *Y4Ptr++ = *input_ptr++ - 128;
            *Y4Ptr++ = *input_ptr++ - 128;
            *CB_Ptr++ = *input_ptr++ - 128;
            *CR_Ptr++ = *input_ptr++ - 128;
        }
        if(cols <= 8)

        {
            for(j = 8 - Y1_cols; j > 0; j--)

            {
                *Y3_Ptr = *(Y3_Ptr - 1); Y3_Ptr++;
                *Y3Ptr = *(Y3Ptr - 1); Y3Ptr++;
            }
            for(j = 8; j > 0; j--)

            {
                *Y4_Ptr++ = *(Y3_Ptr - 1);
                *Y4Ptr++ = *(Y3Ptr - 1);
            }
        }

        else

        {
            for(j = 8 - Y2_cols; j > 0; j--)

            {
                *Y4_Ptr = *(Y4_Ptr - 1); Y4_Ptr++;
                *Y4Ptr = *(Y4Ptr - 1); Y4Ptr++;
            }
        }
        for(j = (16 - cols) >> 1; j > 0; j--)

        {
            *CB_Ptr = *(CB_Ptr - 1); CB_Ptr++;
            *CR_Ptr = *(CR_Ptr - 1); CR_Ptr++;
        }
        Y3_Ptr += 8;
        Y4_Ptr += 8;
        Y3Ptr += 8;
        Y4Ptr += 8;
        input_ptr += incr;
    }
    if(rows <= 8)

    {
        for(i = 8 - rows; i > 0; i--)

        {
            for(j = 8; j > 0; j--)

            {
                *Y1_Ptr = *(Y1_Ptr - 8); Y1_Ptr++;
                *Y2_Ptr = *(Y2_Ptr - 8); Y2_Ptr++;
            }
        }
        for(i = 8; i > 0; i--)

        {
            Y1_Ptr -= 8;
            Y2_Ptr -= 8;
            for(j = 8; j > 0; j--)

            {
                *Y3_Ptr++ = *Y1_Ptr++;
                *Y4_Ptr++ = *Y2_Ptr++;
            }
        }
    }

    else

    {
        for(i = (16 - rows); i > 0; i--)

        {
            for(j = 8; j > 0; j--)

            {
                *Y3_Ptr = *(Y3_Ptr - 8); Y3_Ptr++;
                *Y4_Ptr = *(Y4_Ptr - 8); Y4_Ptr++;
            }
        }
    }
    for(i = ((16 - rows) >> 1); i > 0; i--)

    {
        for(j = 8; j > 0; j--)

        {
            *CB_Ptr = *(CB_Ptr - 8); CB_Ptr++;
            *CR_Ptr = *(CR_Ptr - 8); CR_Ptr++;
        }
    }
}
static void
read_422_format(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
                UINT8 * input_ptr)
{
    INT32 i, j;
    UINT16 Y1_cols, Y2_cols;
    INT16 * Y1_Ptr = Y1;
    INT16 * Y2_Ptr = Y2;
    INT16 * CB_Ptr = CB;
    INT16 * CR_Ptr = CR;
    UINT16 rows = jpeg_encoder_structure->rows;
    UINT16 cols = jpeg_encoder_structure->cols;
    UINT16 incr = jpeg_encoder_structure->incr;
    if(cols <= 8)

    {
        Y1_cols = cols;
        Y2_cols = 0;
    }

    else

    {
        Y1_cols = 8;
        Y2_cols = (UINT16)(cols - 8);
    }
    for(i = rows; i > 0; i--)

    {
        for(j = Y1_cols >> 1; j > 0; j--)

        {
            *Y1_Ptr++ = *input_ptr++ - 128;
            *CB_Ptr++ = *input_ptr++ - 128;
            *Y1_Ptr++ = *input_ptr++ - 128;
            *CR_Ptr++ = *input_ptr++ - 128;
        }
        for(j = Y2_cols >> 1; j > 0; j--)

        {
            *Y2_Ptr++ = *input_ptr++ - 128;
            *CB_Ptr++ = *input_ptr++ - 128;
            *Y2_Ptr++ = *input_ptr++ - 128;
            *CR_Ptr++ = *input_ptr++ - 128;
        }
        if(cols <= 8)

        {
            for(j = 8 - Y1_cols; j > 0; j--)
                *Y1_Ptr = *(Y1_Ptr - 1); Y1_Ptr++;
            for(j = 8 - Y2_cols; j > 0; j--)
                *Y2_Ptr = *(Y1_Ptr - 1); Y2_Ptr++;
        }

        else

        {
            for(j = 8 - Y2_cols; j > 0; j--)
                *Y2_Ptr = *(Y2_Ptr - 1); Y2_Ptr++;
        }
        for(j = (16 - cols) >> 1; j > 0; j--)

        {
            *CB_Ptr = *(CB_Ptr - 1); CB_Ptr++;
            *CR_Ptr = *(CR_Ptr - 1); CR_Ptr++;
        }
        input_ptr += incr;
    }
    for(i = 8 - rows; i > 0; i--)

    {
        for(j = 8; j > 0; j--)

        {
            *Y1_Ptr = *(Y1_Ptr - 8); Y1_Ptr++;
            *Y2_Ptr = *(Y2_Ptr - 8); Y2_Ptr++;
            *CB_Ptr = *(CB_Ptr - 8); CB_Ptr++;
            *CR_Ptr = *(CR_Ptr - 8); CR_Ptr++;
        }
    }
}
static void
read_444_format(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
                UINT8 * input_ptr)
{
    INT32 i, j;
    INT16 * Y1_Ptr = Y1;
    INT16 * CB_Ptr = CB;
    INT16 * CR_Ptr = CR;
    UINT16 rows = jpeg_encoder_structure->rows;
    UINT16 cols = jpeg_encoder_structure->cols;
    UINT16 incr = jpeg_encoder_structure->incr;
    for(i = rows; i > 0; i--)

    {
        for(j = cols; j > 0; j--)

        {
            *Y1_Ptr++ = *input_ptr++ - 128;
            *CB_Ptr++ = *input_ptr++ - 128;
            *CR_Ptr++ = *input_ptr++ - 128;
        }
        for(j = 8 - cols; j > 0; j--)

        {
            *Y1_Ptr = *(Y1_Ptr - 1); Y1_Ptr++;
            *CB_Ptr = *(CB_Ptr - 1); CB_Ptr++;
            *CR_Ptr = *(CR_Ptr - 1); CR_Ptr++;
        }
        input_ptr += incr;
    }
    for(i = 8 - rows; i > 0; i--)

    {
        for(j = 8; j > 0; j--)

        {
            *Y1_Ptr = *(Y1_Ptr - 8); Y1_Ptr++;
            *CB_Ptr = *(CB_Ptr - 8); CB_Ptr++;
            *CR_Ptr = *(CR_Ptr - 8); CR_Ptr++;
        }
    }
}


#define CLIP(color) (unsigned char)(((color)>0xFF)?0xff:(((color)<0)?0:(color)))

/* translate RGB24 to YUV444 in input */
static void
RGB_2_444(UINT8 * input_ptr, UINT8 * output_ptr, UINT32 image_width,
          UINT32 image_height)
{
    UINT32 i, size;
    UINT8 R, G, B;
    INT32 Y, Cb, Cr;
    size = image_width * image_height;
    for(i = size; i > 0; i--)

    {
        B = input_ptr[0];
        G = input_ptr[1];
        R = input_ptr[2];

        //input_ptr -= 3;
        Y = CLIP((77 * R + 150 * G + 29 * B) >> 8);
        Cb = CLIP(((-43 * R - 85 * G + 128 * B) >> 8) + 128);
        Cr = CLIP(((128 * R - 107 * G - 21 * B) >> 8) + 128);
        *input_ptr++ = (UINT8) Y;
        *input_ptr++ = (UINT8) Cb;
        *input_ptr++ = (UINT8) Cr;
    }
}


/* translate RGB24 to YUV422 in input */
static void
RGB_2_422(UINT8 * input_ptr, UINT8 * output_ptr, UINT32 image_width,
          UINT32 image_height)
{
    UINT32 i, size;
    UINT8 R, G, B, R1, G1, B1;
    INT32 Y, Yp, Cb, Cr;
    UINT8 * inbuf = input_ptr;
    size = image_width * image_height;
    for(i = size; i > 0; i--)

    {
        B = inbuf[0];
        G = inbuf[1];
        R = inbuf[2];
        B1 = inbuf[3];
        G1 = inbuf[4];
        R1 = inbuf[5];
        inbuf += 6;
        Y = CLIP((77 * R + 150 * G + 29 * B) >> 8);
        Yp = CLIP((77 * R1 + 150 * G1 + 29 * B1) >> 8);
        Cb = CLIP(((-43 * R - 85 * G + 128 * B) >> 8) + 128);
        Cr = CLIP(((128 * R - 107 * G - 21 * B) >> 8) + 128);
        *input_ptr++ = (UINT8) Y;
        *input_ptr++ = (UINT8) Cb;
        *input_ptr++ = (UINT8) Yp;
        *input_ptr++ = (UINT8) Cr;
    }
}


/* translate RGB24 to YUV420 in input */
static void
RGB_2_420(UINT8 * input_ptr, UINT8 * output_ptr, UINT32 image_width,
          UINT32 image_height)
{
    UINT32 i, j, size;
    UINT8 R, G, B, R1, G1, B1, Rd, Gd, Bd, Rd1, Gd1, Bd1;
    INT32 Y, Yd, Y11, Yd1, Cb, Cr;
    UINT8 * inbuf = input_ptr;
    UINT8 * inbuf1 = input_ptr + (image_width * 3);
    size = image_width * image_height >> 2;
    for(i = size, j = 0; i > 0; i--)

    {
        B = inbuf[0];
        G = inbuf[1];
        R = inbuf[2];
        B1 = inbuf[3];
        G1 = inbuf[4];
        R1 = inbuf[5];
        Bd = inbuf1[0];
        Gd = inbuf1[1];
        Rd = inbuf1[2];
        Bd1 = inbuf1[3];
        Gd1 = inbuf1[4];
        Rd1 = inbuf1[5];
        inbuf += 6;
        inbuf1 += 6;
        j++;
        if(j >= image_width / 2) {
            j = 0;
            inbuf += (image_width * 3);
            inbuf1 += (image_width * 3);
        }
        Y = CLIP((77 * R + 150 * G + 29 * B) >> 8);
        Y11 = CLIP((77 * R1 + 150 * G1 + 29 * B1) >> 8);
        Yd = CLIP((77 * Rd + 150 * Gd + 29 * Bd) >> 8);
        Yd1 = CLIP((77 * Rd1 + 150 * Gd1 + 29 * Bd1) >> 8);
        Cb = CLIP(((-43 * R - 85 * G + 128 * B) >> 8) + 128);
        Cr = CLIP(((128 * R - 107 * G - 21 * B) >> 8) + 128);
        *input_ptr++ = (UINT8) Y;
        *input_ptr++ = (UINT8) Y11;
        *input_ptr++ = (UINT8) Yd;
        *input_ptr++ = (UINT8) Yd1;
        *input_ptr++ = (UINT8) Cb;
        *input_ptr++ = (UINT8) Cr;
    }
}


/* translate RGB32 to YUV420 in input */
static void
RGB32_2_420(UINT8 * input_ptr, UINT8 * output_ptr, UINT32 image_width,
            UINT32 image_height)
{
    UINT32 i, j, size;
    UINT8 R, G, B, R1, G1, B1, Rd, Gd, Bd, Rd1, Gd1, Bd1;
    INT32 Y, Yd, Y11, Yd1, Cb, Cr;
    UINT8 * inbuf = input_ptr;
    UINT8 * inbuf1 = input_ptr + (image_width * 4);
    size = image_width * image_height >> 2;
    for(i = size, j = 0; i > 0; i--)

    {
        B = inbuf[0];
        G = inbuf[1];
        R = inbuf[2];
        B1 = inbuf[4];
        G1 = inbuf[5];
        R1 = inbuf[6];
        Bd = inbuf1[0];
        Gd = inbuf1[1];
        Rd = inbuf1[2];
        Bd1 = inbuf1[4];
        Gd1 = inbuf1[5];
        Rd1 = inbuf1[6];
        inbuf += 8;
        inbuf1 += 8;
        j++;
        if(j >= image_width / 2) {
            j = 0;
            inbuf += (image_width * 4);
            inbuf1 += (image_width * 4);
        }
        Y = CLIP((77 * R + 150 * G + 29 * B) >> 8);
        Y11 = CLIP((77 * R1 + 150 * G1 + 29 * B1) >> 8);
        Yd = CLIP((77 * Rd + 150 * Gd + 29 * Bd) >> 8);
        Yd1 = CLIP((77 * Rd1 + 150 * Gd1 + 29 * Bd1) >> 8);
        Cb = CLIP(((-43 * R - 85 * G + 128 * B) >> 8) + 128);
        Cr = CLIP(((128 * R - 107 * G - 21 * B) >> 8) + 128);
        *input_ptr++ = (UINT8) Y;
        *input_ptr++ = (UINT8) Y11;
        *input_ptr++ = (UINT8) Yd;
        *input_ptr++ = (UINT8) Yd1;
        *input_ptr++ = (UINT8) Cb;
        *input_ptr++ = (UINT8) Cr;
    }
}


/* translate RGB565 to YUV420 in input */
static void
RGB565_2_420(UINT8 * input_ptr, UINT8 * output_ptr, UINT32 image_width,
             UINT32 image_height)
{
    UINT32 i, j, size;
    UINT8 R, G, B, R1, G1, B1, Rd, Gd, Bd, Rd1, Gd1, Bd1;
    INT32 Y, Yd, Y11, Yd1, Cb, Cr;
    Myrgb16 * inbuf = (Myrgb16 *) input_ptr;
    Myrgb16 * inbuf1 = inbuf + (image_width);
    size = image_width * image_height >> 2;
    for(i = size, j = 0; i > 0; i--)

    {

        /*
           B = inbuf[0] & 0xf8;
           G = ((inbuf[0] & 0x07) << 5) | ((inbuf[1] & 0xe0) >> 3);
           R = (inbuf[1] & 0x1f) << 3;

           B1 = inbuf[2] & 0xf8;
           G1 = ((inbuf[2] & 0x07) << 5) | ((inbuf[3] & 0xe0) >> 3);
           R1 = (inbuf[3] & 0x1f) << 3;

           Bd = inbuf1[0] & 0xf8;
           Gd = ((inbuf1[0] & 0x07) << 5) | ((inbuf1[1] & 0xe0) >> 3);
           Rd = (inbuf1[1] & 0x1f) << 3;

           Bd1 = inbuf1[2] & 0xf8;
           Gd1 = ((inbuf1[2] & 0x07) << 5) | ((inbuf1[3] & 0xe0) >> 3);
           Rd1 = (inbuf1[3] & 0x1f) << 3;
         */
        B = inbuf[0].blue << 3;
        G = inbuf[0].green << 2;
        R = inbuf[0].red << 3;
        B1 = inbuf[1].blue << 3;
        G1 = inbuf[1].green << 2;
        R1 = inbuf[1].red << 3;
        Bd = inbuf1[0].blue << 3;
        Gd = inbuf1[0].green << 2;
        Rd = inbuf1[0].red << 3;
        Bd1 = inbuf1[1].blue << 3;
        Gd1 = inbuf[1].green << 2;
        Rd1 = inbuf[1].red << 3;
        inbuf += 2;
        inbuf1 += 2;
        j++;
        if(j >= image_width / 2) {
            j = 0;
            inbuf += (image_width);
            inbuf1 += (image_width);
        }
        Y = CLIP((77 * R + 150 * G + 29 * B) >> 8);
        Y11 = CLIP((77 * R1 + 150 * G1 + 29 * B1) >> 8);
        Yd = CLIP((77 * Rd + 150 * Gd + 29 * Bd) >> 8);
        Yd1 = CLIP((77 * Rd1 + 150 * Gd1 + 29 * Bd1) >> 8);
        Cb = CLIP(((-43 * R - 85 * G + 128 * B) >> 8) + 128);
        Cr = CLIP(((128 * R - 107 * G - 21 * B) >> 8) + 128);
        *input_ptr++ = (UINT8) Y;
        *input_ptr++ = (UINT8) Y11;
        *input_ptr++ = (UINT8) Yd;
        *input_ptr++ = (UINT8) Yd1;
        *input_ptr++ = (UINT8) Cb;
        *input_ptr++ = (UINT8) Cr;
    }
}
static void
RGB_2_400(UINT8 * input_ptr, UINT8 * output_ptr, UINT32 image_width,
          UINT32 image_height)
{
    UINT32 i, size;
    UINT8 R, G, B;
    INT32 Y;
    UINT8 * inbuf = input_ptr;
    size = image_width * image_height;
    for(i = size; i > 0; i--)

    {
        B = inbuf[0];
        G = inbuf[1];
        R = inbuf[2];
        inbuf += 3;
        Y = CLIP((77 * R + 150 * G + 29 * B) >> 8);
        *input_ptr++ = (UINT8) Y;
    }
}


/* translate YUV444P to YUV444 in input */
static void
YUV_2_444(UINT8 * input_ptr, UINT8 * output_ptr, UINT32 image_width,
          UINT32 image_height)
{
    UINT32 i, size;
    UINT8 * Ytmp = NULL;
    UINT8 * Cbtmp = NULL;
    UINT8 * Crtmp = NULL;
    UINT8 * Buff = NULL;
    Buff =
        (UINT8 *) realloc((UINT8 *) Buff,
                          (size_t)(image_width * image_height * 3));
    if(Buff) {
        memcpy(Buff, input_ptr, image_width * image_height * 3);
        Ytmp = Buff;
        Cbtmp = Buff + image_width * image_height;
        Crtmp = Buff + (image_width * image_height << 1);
        size = image_width * image_height;
        for(i = size; i > 0; i--)

        {
            *input_ptr++ = (UINT8) * Ytmp++;
            *input_ptr++ = (UINT8) * Cbtmp++;
            *input_ptr++ = (UINT8) * Crtmp++;
        }
        free(Buff);
        Buff = NULL;
    }
}


/* translate YUV422P to YUV422 in input */
static void
YUV_2_422(UINT8 * input_ptr, UINT8 * output_ptr, UINT32 image_width,
          UINT32 image_height)
{
    UINT32 i, size;
    UINT8 * Ytmp = NULL;
    UINT8 * Cbtmp = NULL;
    UINT8 * Crtmp = NULL;
    UINT8 * Buff = NULL;
    Buff =
        (UINT8 *) realloc((UINT8 *) Buff,
                          (size_t)(image_width * image_height * 2));
    if(Buff) {
        memcpy(Buff, input_ptr, image_width * image_height * 2);
        Ytmp = Buff;
        Cbtmp = Buff + image_width * image_height;
        Crtmp = Cbtmp + (image_width * image_height >> 1);
        size = image_width * image_height;
        for(i = size; i > 0; i--)

        {
            *input_ptr++ = (UINT8) * Ytmp++;
            *input_ptr++ = (UINT8) * Cbtmp++;
            *input_ptr++ = (UINT8) * Ytmp++;
            *input_ptr++ = (UINT8) * Crtmp++;
        }
        free(Buff);
        Buff = NULL;
    }
}


/* translate YUV420P to YUV420 in input */
static void
YUV_2_420(UINT8 * input_ptr, UINT8 * output_ptr, UINT32 image_width,
          UINT32 image_height)
{
    UINT32 x, y, size;
    UINT8 * Ytmp = NULL;
    UINT8 * Y2tmp = NULL;
    UINT8 * Cbtmp = NULL;
    UINT8 * Crtmp = NULL;
    UINT8 * Buff = NULL;
    Buff =
        (UINT8 *) realloc((UINT8 *) Buff,
                          (size_t)((image_width * image_height * 3) >> 1));
    if(Buff) {
        memcpy(Buff, input_ptr, (image_width * image_height * 3) >> 1);
        Ytmp = Buff;
        Y2tmp = Buff + image_width;
        Cbtmp = Buff + image_width * image_height;
        Crtmp = Cbtmp + (image_width * image_height >> 2);
        size = image_width * image_height >> 2;
        for(y = 0; y < image_height; y += 2)

        {
            for(x = 0; x < image_width; x += 2) {
                *input_ptr++ = (UINT8) * Ytmp++;
                *input_ptr++ = (UINT8) * Ytmp++;
                *input_ptr++ = (UINT8) * Y2tmp++;
                *input_ptr++ = (UINT8) * Y2tmp++;
                *input_ptr++ = (UINT8) * Cbtmp++;
                *input_ptr++ = (UINT8) * Crtmp++;
            }
            Ytmp += image_width;
            Y2tmp += image_width;
        }
        free(Buff);
        Buff = NULL;
    }
}


