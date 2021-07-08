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
#include "huffman.h"

UINT16 luminance_dc_code_table[] = {
    0x0000, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x000E, 0x001E, 0x003E,
    0x007E, 0x00FE, 0x01FE
};
UINT16 luminance_dc_size_table[] = {
    0x0002, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0004, 0x0005, 0x0006,
    0x0007, 0x0008, 0x0009
};
UINT16 chrominance_dc_code_table[] = {
    0x0000, 0x0001, 0x0002, 0x0006, 0x000E, 0x001E, 0x003E, 0x007E, 0x00FE,
    0x01FE, 0x03FE, 0x07FE
};
UINT16 chrominance_dc_size_table[] = {
    0x0002, 0x0002, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008,
    0x0009, 0x000A, 0x000B
};

UINT16 luminance_ac_code_table[] = {
    0x000A, 0x0000, 0x0001, 0x0004, 0x000B, 0x001A, 0x0078, 0x00F8, 0x03F6,
    0xFF82, 0xFF83, 0x000C, 0x001B, 0x0079, 0x01F6, 0x07F6, 0xFF84, 0xFF85,
    0xFF86, 0xFF87, 0xFF88, 0x001C, 0x00F9, 0x03F7, 0x0FF4, 0xFF89, 0xFF8A,
    0xFF8b, 0xFF8C, 0xFF8D, 0xFF8E, 0x003A, 0x01F7, 0x0FF5, 0xFF8F, 0xFF90,
    0xFF91, 0xFF92, 0xFF93, 0xFF94, 0xFF95, 0x003B, 0x03F8, 0xFF96, 0xFF97,
    0xFF98, 0xFF99, 0xFF9A, 0xFF9B, 0xFF9C, 0xFF9D, 0x007A, 0x07F7, 0xFF9E,
    0xFF9F, 0xFFA0, 0xFFA1, 0xFFA2, 0xFFA3, 0xFFA4, 0xFFA5, 0x007B, 0x0FF6,
    0xFFA6, 0xFFA7, 0xFFA8, 0xFFA9, 0xFFAA, 0xFFAB, 0xFFAC, 0xFFAD, 0x00FA,
    0x0FF7, 0xFFAE, 0xFFAF, 0xFFB0, 0xFFB1, 0xFFB2, 0xFFB3, 0xFFB4, 0xFFB5,
    0x01F8, 0x7FC0, 0xFFB6, 0xFFB7, 0xFFB8, 0xFFB9, 0xFFBA, 0xFFBB, 0xFFBC,
    0xFFBD, 0x01F9, 0xFFBE, 0xFFBF, 0xFFC0, 0xFFC1, 0xFFC2, 0xFFC3, 0xFFC4,
    0xFFC5, 0xFFC6, 0x01FA, 0xFFC7, 0xFFC8, 0xFFC9, 0xFFCA, 0xFFCB, 0xFFCC,
    0xFFCD, 0xFFCE, 0xFFCF, 0x03F9, 0xFFD0, 0xFFD1, 0xFFD2, 0xFFD3, 0xFFD4,
    0xFFD5, 0xFFD6, 0xFFD7, 0xFFD8, 0x03FA, 0xFFD9, 0xFFDA, 0xFFDB, 0xFFDC,
    0xFFDD, 0xFFDE, 0xFFDF, 0xFFE0, 0xFFE1, 0x07F8, 0xFFE2, 0xFFE3, 0xFFE4,
    0xFFE5, 0xFFE6, 0xFFE7, 0xFFE8, 0xFFE9, 0xFFEA, 0xFFEB, 0xFFEC, 0xFFED,
    0xFFEE, 0xFFEF, 0xFFF0, 0xFFF1, 0xFFF2, 0xFFF3, 0xFFF4, 0xFFF5, 0xFFF6,
    0xFFF7, 0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE,
    0x07F9
};
UINT16 luminance_ac_size_table[] = {
    0x0004, 0x0002, 0x0002, 0x0003, 0x0004, 0x0005, 0x0007, 0x0008, 0x000A,
    0x0010, 0x0010, 0x0004, 0x0005, 0x0007, 0x0009, 0x000B, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0005, 0x0008, 0x000A, 0x000C, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0006, 0x0009, 0x000C, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0006, 0x000A, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0007, 0x000B, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0007, 0x000C,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0008,
    0x000C, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0009, 0x000F, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0009, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0009, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x000A, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x000A, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x000B, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x000B
};
UINT16 chrominance_ac_code_table[] = {
    0x0000, 0x0001, 0x0004, 0x000A, 0x0018, 0x0019, 0x0038, 0x0078, 0x01F4,
    0x03F6, 0x0FF4, 0x000B, 0x0039, 0x00F6, 0x01F5, 0x07F6, 0x0FF5, 0xFF88,
    0xFF89, 0xFF8A, 0xFF8B, 0x001A, 0x00F7, 0x03F7, 0x0FF6, 0x7FC2, 0xFF8C,
    0xFF8D, 0xFF8E, 0xFF8F, 0xFF90, 0x001B, 0x00F8, 0x03F8, 0x0FF7, 0xFF91,
    0xFF92, 0xFF93, 0xFF94, 0xFF95, 0xFF96, 0x003A, 0x01F6, 0xFF97, 0xFF98,
    0xFF99, 0xFF9A, 0xFF9B, 0xFF9C, 0xFF9D, 0xFF9E, 0x003B, 0x03F9, 0xFF9F,
    0xFFA0, 0xFFA1, 0xFFA2, 0xFFA3, 0xFFA4, 0xFFA5, 0xFFA6, 0x0079, 0x07F7,
    0xFFA7, 0xFFA8, 0xFFA9, 0xFFAA, 0xFFAB, 0xFFAC, 0xFFAD, 0xFFAE, 0x007A,
    0x07F8, 0xFFAF, 0xFFB0, 0xFFB1, 0xFFB2, 0xFFB3, 0xFFB4, 0xFFB5, 0xFFB6,
    0x00F9, 0xFFB7, 0xFFB8, 0xFFB9, 0xFFBA, 0xFFBB, 0xFFBC, 0xFFBD, 0xFFBE,
    0xFFBF, 0x01F7, 0xFFC0, 0xFFC1, 0xFFC2, 0xFFC3, 0xFFC4, 0xFFC5, 0xFFC6,
    0xFFC7, 0xFFC8, 0x01F8, 0xFFC9, 0xFFCA, 0xFFCB, 0xFFCC, 0xFFCD, 0xFFCE,
    0xFFCF, 0xFFD0, 0xFFD1, 0x01F9, 0xFFD2, 0xFFD3, 0xFFD4, 0xFFD5, 0xFFD6,
    0xFFD7, 0xFFD8, 0xFFD9, 0xFFDA, 0x01FA, 0xFFDB, 0xFFDC, 0xFFDD, 0xFFDE,
    0xFFDF, 0xFFE0, 0xFFE1, 0xFFE2, 0xFFE3, 0x07F9, 0xFFE4, 0xFFE5, 0xFFE6,
    0xFFE7, 0xFFE8, 0xFFE9, 0xFFEA, 0xFFEb, 0xFFEC, 0x3FE0, 0xFFED, 0xFFEE,
    0xFFEF, 0xFFF0, 0xFFF1, 0xFFF2, 0xFFF3, 0xFFF4, 0xFFF5, 0x7FC3, 0xFFF6,
    0xFFF7, 0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE,
    0x03FA
};
UINT16 chrominance_ac_size_table[] = {
    0x0002, 0x0002, 0x0003, 0x0004, 0x0005, 0x0005, 0x0006, 0x0007, 0x0009,
    0x000A, 0x000C, 0x0004, 0x0006, 0x0008, 0x0009, 0x000B, 0x000C, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0005, 0x0008, 0x000A, 0x000C, 0x000F, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0005, 0x0008, 0x000A, 0x000C, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0006, 0x0009, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0006, 0x000A, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0007, 0x000B,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0007,
    0x000B, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0008, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0009, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0009, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0009, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0009, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x000B, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x000E, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x000F, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x000A
};
UINT8 bitsize[] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8
};
#if 0
#define PUTBITS { \
        bits_in_next_word = (INT16) (bitindex + numbits - 32); \
        if (bits_in_next_word < 0) \
        { \
            lcode = (lcode << numbits) | data; \
            bitindex += numbits; \
        } \
        else \
        { \
            lcode = (lcode << (32 - bitindex)) | (data >> bits_in_next_word); \
            if ((*output_ptr++ = (UINT8) (lcode >> 24)) == 0xff) \
                *output_ptr++ = 0; \
            if ((*output_ptr++ = (UINT8) (lcode >> 16)) == 0xff) \
                *output_ptr++ = 0; \
            if ((*output_ptr++ = (UINT8) (lcode >> 8)) == 0xff) \
                *output_ptr++ = 0; \
            if ((*output_ptr++ = (UINT8) lcode) == 0xff) \
                *output_ptr++ = 0; \
            lcode = data; \
            bitindex = bits_in_next_word; \
        } \
    }
#endif
UINT8 * huffman(JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
                UINT16 component, UINT8 * output_ptr)
{
    UINT16 i;
    UINT16 * DcCodeTable, *DcSizeTable, *AcCodeTable, *AcSizeTable;
    INT16 * Temp_Ptr, Coeff, LastDc;
    UINT16 AbsCoeff, HuffCode, HuffSize, RunLength = 0, DataSize = 0, index;
    INT16 bits_in_next_word;
    UINT16 numbits;
    UINT32 data;
    Temp_Ptr = Temp;
    Coeff = *Temp_Ptr++;
    if(component == 1)

    {
        DcCodeTable = luminance_dc_code_table;
        DcSizeTable = luminance_dc_size_table;
        AcCodeTable = luminance_ac_code_table;
        AcSizeTable = luminance_ac_size_table;
        LastDc = jpeg_encoder_structure->ldc1;
        jpeg_encoder_structure->ldc1 = Coeff;
    }

    else

    {
        DcCodeTable = chrominance_dc_code_table;
        DcSizeTable = chrominance_dc_size_table;
        AcCodeTable = chrominance_ac_code_table;
        AcSizeTable = chrominance_ac_size_table;
        if(component == 2)

        {
            LastDc = jpeg_encoder_structure->ldc2;
            jpeg_encoder_structure->ldc2 = Coeff;
        }

        else

        {
            LastDc = jpeg_encoder_structure->ldc3;
            jpeg_encoder_structure->ldc3 = Coeff;
        }
    }
    Coeff -= LastDc;
    AbsCoeff = (Coeff < 0) ? -Coeff-- : Coeff;
    while(AbsCoeff != 0)

    {
        AbsCoeff >>= 1;
        DataSize++;
    }
    HuffCode = DcCodeTable[DataSize];
    HuffSize = DcSizeTable[DataSize];
    Coeff &= (1 << DataSize) - 1;
    data = (HuffCode << DataSize) | Coeff;
    numbits = HuffSize + DataSize;
    //PUTBITS
    {
        bits_in_next_word = (INT16)(bitindex + numbits - 32);
        if(bits_in_next_word < 0) {
            lcode = (lcode << numbits) | data;
            bitindex += numbits;
        } else {
            lcode = (lcode << (32 - bitindex)) | (data >> bits_in_next_word);
            if((*output_ptr++ = (UINT8)(lcode >> 24)) == 0xff)
                * output_ptr++ = 0;
            if((*output_ptr++ = (UINT8)(lcode >> 16)) == 0xff)
                * output_ptr++ = 0;
            if((*output_ptr++ = (UINT8)(lcode >> 8)) == 0xff)
                * output_ptr++ = 0;
            if((*output_ptr++ = (UINT8) lcode) == 0xff)
                * output_ptr++ = 0;
            lcode = data;
            bitindex = bits_in_next_word;
        }
    }
    for(i = 63; i > 0; i--)

    {
        if((Coeff = *Temp_Ptr++) != 0)

        {
            while(RunLength > 15)

            {
                RunLength -= 16;
                data = AcCodeTable[161];
                numbits = AcSizeTable[161];
                //PUTBITS
                {
                    bits_in_next_word = (INT16)(bitindex + numbits - 32);
                    if(bits_in_next_word < 0) {
                        lcode = (lcode << numbits) | data;
                        bitindex += numbits;
                    } else {
                        lcode = (lcode << (32 - bitindex)) | (data >> bits_in_next_word);
                        if((*output_ptr++ = (UINT8)(lcode >> 24)) == 0xff)
                            * output_ptr++ = 0;
                        if((*output_ptr++ = (UINT8)(lcode >> 16)) == 0xff)
                            * output_ptr++ = 0;
                        if((*output_ptr++ = (UINT8)(lcode >> 8)) == 0xff)
                            * output_ptr++ = 0;
                        if((*output_ptr++ = (UINT8) lcode) == 0xff)
                            * output_ptr++ = 0;
                        lcode = data;
                        bitindex = bits_in_next_word;
                    }
                }
            }
            AbsCoeff = (Coeff < 0) ? -Coeff-- : Coeff;
            if(AbsCoeff >> 8 == 0)
                DataSize = bitsize[AbsCoeff];

            else
                DataSize = bitsize[AbsCoeff >> 8] + 8;
            index = RunLength * 10 + DataSize;
            HuffCode = AcCodeTable[index];
            HuffSize = AcSizeTable[index];
            Coeff &= (1 << DataSize) - 1;
            data = (HuffCode << DataSize) | Coeff;
            numbits = HuffSize + DataSize;
            // PUTBITS
            {
                bits_in_next_word = (INT16)(bitindex + numbits - 32);
                if(bits_in_next_word < 0) {
                    lcode = (lcode << numbits) | data;
                    bitindex += numbits;
                } else {
                    lcode = (lcode << (32 - bitindex)) | (data >> bits_in_next_word);
                    if((*output_ptr++ = (UINT8)(lcode >> 24)) == 0xff)
                        * output_ptr++ = 0;
                    if((*output_ptr++ = (UINT8)(lcode >> 16)) == 0xff)
                        * output_ptr++ = 0;
                    if((*output_ptr++ = (UINT8)(lcode >> 8)) == 0xff)
                        * output_ptr++ = 0;
                    if((*output_ptr++ = (UINT8) lcode) == 0xff)
                        * output_ptr++ = 0;
                    lcode = data;
                    bitindex = bits_in_next_word;
                }
            }
            RunLength = 0;
        }

        else
            RunLength++;
    }
    if(RunLength != 0)

    {
        data = AcCodeTable[0];
        numbits = AcSizeTable[0];
        // PUTBITS
        {
            bits_in_next_word = (INT16)(bitindex + numbits - 32);
            if(bits_in_next_word < 0) {
                lcode = (lcode << numbits) | data;
                bitindex += numbits;
            } else {
                lcode = (lcode << (32 - bitindex)) | (data >> bits_in_next_word);
                if((*output_ptr++ = (UINT8)(lcode >> 24)) == 0xff)
                    * output_ptr++ = 0;
                if((*output_ptr++ = (UINT8)(lcode >> 16)) == 0xff)
                    * output_ptr++ = 0;
                if((*output_ptr++ = (UINT8)(lcode >> 8)) == 0xff)
                    * output_ptr++ = 0;
                if((*output_ptr++ = (UINT8) lcode) == 0xff)
                    * output_ptr++ = 0;
                lcode = data;
                bitindex = bits_in_next_word;
            }
        }
    }
    return output_ptr;
}


/* For bit Stuffing and EOI marker */
UINT8 * close_bitstream(UINT8 * output_ptr)
{
    UINT16 i, count;
    UINT8 * ptr;
    if(bitindex > 0)

    {
        lcode <<= (32 - bitindex);
        count = (bitindex + 7) >> 3;
        ptr = (UINT8 *) & lcode + 3;
        for(i = count; i > 0; i--)

        {
            if((*output_ptr++ = *ptr--) == 0xff)
                * output_ptr++ = 0;
        }
    }

    // End of image marker
    *output_ptr++ = 0xFF;
    *output_ptr++ = 0xD9;
    return output_ptr;
}


