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


#include "jdatatype.h"
#include "encoder.h"
#include "marker.h"

// Header for JPEG Encoder
static UINT16 markerdata[] = {
    // dht
    0xFFC4, 0x1A2, 0x00,
    // luminance dc (2 - 16) + 1
    0x0105, 0x0101, 0x00101, 0x0101, 0x0000, 0x00000, 00000, 00000,
    // luminance dc (2 - 12) + 1
    0x0102, 0x0304, 0x0506, 0x0708, 0x090A, 0x0B01,
    // chrominance dc (1 - 16)
    0x0003, 0x0101, 0x0101, 0x0101, 0x0101, 0x0100, 0x0000, 0x0000,
    // chrominance dc (1 - 12)
    0x0001, 0x00203, 0x0405, 0x0607, 0x0809, 0x00A0B,
    // luminance ac 1 + (1 - 15)
    0x1000, 0x0201, 0x0303, 0x0204, 0x0305, 0x0504, 0x0400, 0x0001,
    // luminance ac 1 + (1 - 162) + 1
    0x7D01, 0x0203, 0x0004, 0x1105, 0x1221, 0x3141, 0x0613, 0x5161, 0x0722,
    0x7114, 0x3281, 0x91A1, 0x0823, 0x42B1, 0xC115, 0x52D1, 0xF024, 0x3362,
    0x7282, 0x090A, 0x1617, 0x1819, 0x1A25, 0x2627, 0x2829, 0x2A34, 0x3536,
    0x3738, 0x393A, 0x4344, 0x4546, 0x4748, 0x494A, 0x5354, 0x5556, 0x5758,
    0x595A, 0x6364, 0x6566, 0x6768, 0x696A, 0x7374, 0x7576, 0x7778, 0x797A,
    0x8384, 0x8586, 0x8788, 0x898A, 0x9293, 0x9495, 0x9697, 0x9899, 0x9AA2,
    0xA3A4, 0xA5A6, 0xA7A8, 0xA9AA, 0xB2B3, 0xB4B5, 0xB6B7, 0xB8B9, 0xBAC2,
    0xC3C4, 0xC5C6, 0xC7C8, 0xC9CA, 0xD2D3, 0xD4D5, 0xD6D7, 0xD8D9, 0xDAE1,
    0xE2E3, 0xE4E5, 0xE6E7, 0xE8E9, 0xEAF1, 0xF2F3, 0xF4F5, 0xF6F7, 0xF8F9,
    0xFA11,
    // chrominance ac (1 - 16)
    0x0002, 0x0102, 0x0404, 0x0304, 0x0705, 0x0404, 0x0001, 0x0277,
    // chrominance ac (1 - 162)
    0x0001, 0x0203, 0x1104, 0x0521, 0x3106, 0x1241, 0x5107, 0x6171, 0x1322,
    0x3281, 0x0814, 0x4291, 0xA1B1, 0xC109, 0x2333, 0x52F0, 0x1562, 0x72D1,
    0x0A16, 0x2434, 0xE125, 0xF117, 0x1819, 0x1A26, 0x2728, 0x292A, 0x3536,
    0x3738, 0x393A, 0x4344, 0x4546, 0x4748, 0x494A, 0x5354, 0x5556, 0x5758,
    0x595A, 0x6364, 0x6566, 0x6768, 0x696A, 0x7374, 0x7576, 0x7778, 0x797A,
    0x8283, 0x8485, 0x8687, 0x8889, 0x8A92, 0x9394, 0x9596, 0x9798, 0x999A,
    0xA2A3, 0xA4A5, 0xA6A7, 0xA8A9, 0xAAB2, 0xB3B4, 0xB5B6, 0xB7B8, 0xB9BA,
    0xC2C3, 0xC4C5, 0xC6C7, 0xC8C9, 0xCAD2, 0xD3D4, 0xD5D6, 0xD7D8, 0xD9DA,
    0xE2E3, 0xE4E5, 0xE6E7, 0xE8E9, 0xEAF2, 0xF3F4, 0xF5F6, 0xF7F8, 0xF9FA
};

UINT8 * write_markers(UINT8 * output_ptr, UINT32 image_format,
                      UINT32 image_width, UINT32 image_height)
{
    UINT16 i, header_length;
    UINT8 number_of_components;

    // Start of image marker
    *output_ptr++ = 0xFF;
    *output_ptr++ = 0xD8;

    // Quantization table marker
    *output_ptr++ = 0xFF;
    *output_ptr++ = 0xDB;

    // Quantization table length
    *output_ptr++ = 0x00;
    *output_ptr++ = 0x84;

    // Pq, Tq
    *output_ptr++ = 0x00;

    // Lqt table
    for(i = 0; i < 64; i++)
        *output_ptr++ = Lqt[i];

    // Pq, Tq
    *output_ptr++ = 0x01;

    // Cqt table
    for(i = 0; i < 64; i++)
        *output_ptr++ = Cqt[i];

    // huffman table(DHT)
    for(i = 0; i < 210; i++)

    {
        *output_ptr++ = (UINT8)(markerdata[i] >> 8);
        *output_ptr++ = (UINT8) markerdata[i];
    }
    if(image_format == FOUR_ZERO_ZERO)
        number_of_components = 1;

    else
        number_of_components = 3;

    // Frame header(SOF)

    // Start of frame marker
    *output_ptr++ = 0xFF;
    *output_ptr++ = 0xC0;
    header_length = (UINT16)(8 + 3 * number_of_components);

    // Frame header length
    *output_ptr++ = (UINT8)(header_length >> 8);
    *output_ptr++ = (UINT8) header_length;

    // Precision (P)
    *output_ptr++ = 0x08;

    // image height
    *output_ptr++ = (UINT8)(image_height >> 8);
    *output_ptr++ = (UINT8) image_height;

    // image width
    *output_ptr++ = (UINT8)(image_width >> 8);
    *output_ptr++ = (UINT8) image_width;

    // Nf
    *output_ptr++ = number_of_components;
    if(image_format == FOUR_ZERO_ZERO)

    {
        *output_ptr++ = 0x01;
        *output_ptr++ = 0x11;
        *output_ptr++ = 0x00;
    }

    else

    {
        *output_ptr++ = 0x01;
        if(image_format == FOUR_TWO_ZERO)
            *output_ptr++ = 0x22;

        else if(image_format == FOUR_TWO_TWO)
            *output_ptr++ = 0x21;

        else
            *output_ptr++ = 0x11;
        *output_ptr++ = 0x00;
        *output_ptr++ = 0x02;
        *output_ptr++ = 0x11;
        *output_ptr++ = 0x01;
        *output_ptr++ = 0x03;
        *output_ptr++ = 0x11;
        *output_ptr++ = 0x01;
    }

    // Scan header(SOF)

    // Start of scan marker
    *output_ptr++ = 0xFF;
    *output_ptr++ = 0xDA;
    header_length = (UINT16)(6 + (number_of_components << 1));

    // Scan header length
    *output_ptr++ = (UINT8)(header_length >> 8);
    *output_ptr++ = (UINT8) header_length;

    // Ns
    *output_ptr++ = number_of_components;
    if(image_format == FOUR_ZERO_ZERO)

    {
        *output_ptr++ = 0x01;
        *output_ptr++ = 0x00;
    }

    else

    {
        *output_ptr++ = 0x01;
        *output_ptr++ = 0x00;
        *output_ptr++ = 0x02;
        *output_ptr++ = 0x11;
        *output_ptr++ = 0x03;
        *output_ptr++ = 0x11;
    }
    *output_ptr++ = 0x00;
    *output_ptr++ = 0x3F;
    *output_ptr++ = 0x00;
    return output_ptr;
}


