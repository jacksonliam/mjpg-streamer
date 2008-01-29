/***************************************************************************#
# jpegenc: library to encode a jpeg frame from various input palette.       #
# jpegenc works for embedded device without libjpeg                         #
#.                                                                          #
# 		Copyright (C) 2005 Michel Xhaard                            #
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
#  CREDIT:								    #
# Original code from Nitin Gupta India (?)				    #
#                                                                           #
#***************************************************************************/ 
#ifndef HUFFMAN_H
#define HUFFMAN_H
UINT8 * huffman (JPEG_ENCODER_STRUCTURE *, UINT16, UINT8 *);
UINT8 * close_bitstream (UINT8 *);
#endif	/*HUFFMAN_H */
