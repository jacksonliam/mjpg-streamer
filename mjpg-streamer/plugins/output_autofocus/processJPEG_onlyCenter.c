/*******************************************************************************
#   processJPEG_onlyCenter: sharpness estimates via JPEG AC coefficients       #
# Based on partial JPEG decompress on the camera JPEG images                   #
#                                                                              #
#   Copyright (C) 2007 Alexander K. Seewald <alex@seewald.at>                  #
#   Many helpful suggestions and improvements due to Richard Atterer           #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; either version 2 of the License, or            #
# (at your option) any later version.                                          #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>

#include "processJPEG_onlyCenter.h"

double getFrameSharpnessValue(unsigned char *data, int len)
{
    int Gpos = 0;
    char c; unsigned short int cnt; int i;
    int cnt2 = 0;
    unsigned char uc;
    assert(sizeof(cnt) == 2);
    assert(sizeof(int) == 4);
    unsigned char *buffer;
    double sumAC[64];
    double sumSqrAC[64];

    int width = -1; int height = -1;
    int huffman_DCY = -1; int huffman_ACY = -1;
    int quant_Y = -1;
    int scaleH_Y = 0; int scaleV_Y = 0;

    unsigned char dht_lookup_bitlen[4][162];
    unsigned int dht_lookup_bits[4][162];
    unsigned int dht_lookup_mask[4][162];
    unsigned char dht_lookup_code[4][162];
    unsigned char dht_lookup_size[4];

    float *QT[4] = { NULL, NULL, NULL, NULL };

    unsigned int mask[17] = { 0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff };

    char fgetc_(void) {
        return data[Gpos++];
    }

    while(!(Gpos >= len)) {
        c = fgetc_();
        while(c != '\xff') {
            c = fgetc_();
        }
        c = fgetc_();
        if(c == '\xd8' || c == '\xe0' || c == '\x00') {
            continue;
        } else if(c == '\xdb') { // read in quantization table
            c = fgetc_(); cnt = (unsigned char)(c) * 256;
            c = fgetc_(); cnt += (unsigned char)(c);
            assert(cnt == 67);
            uc = fgetc_(); if((uc >> 4) != 0) {
                fprintf(stderr, "16bit quantization table not supported\n");
                exit(-1);
            }
            int tab = uc & 0x0f;
            QT[tab] = (float *)malloc(64 * sizeof(float));
            unsigned char hc = 0; int j;
            for(j = 0; j < 64; j++) {
                uc = fgetc_(); QT[tab][j] = (float)uc;
                hc ^= uc;
            }
        } else if(c == '\xc4') { // read in huffmann table
            // 0 and 2 are DC tables (0-15), 1 and 3 are AC tables (0-255)
            c = fgetc_(); cnt = (unsigned char)(c) * 256;
            c = fgetc_(); cnt += (unsigned char)(c);
            buffer = data + Gpos; Gpos += cnt - 2;

            // parse huffman table...
            int pos = 0; unsigned short int tab = 0;
            do {
                uc = buffer[pos]; pos++;
                tab = (uc >> 4) + (uc & 0x0f) * 2;
                tab = (uc & 0x0f) * 2 + (uc >> 4);
                int pos2 = pos + 16;
                unsigned char DhtCodesLen[16];
                int i_len; int table_ind = 0;
                int code_val = 0; int ind_code;
                for(i_len = 1; i_len <= 16; i_len++) {
                    DhtCodesLen[i_len-1] = (unsigned char)(buffer[pos+i_len-1]);
                    for(ind_code = 0; ind_code < DhtCodesLen[i_len-1]; ind_code++) {
                        dht_lookup_code[tab][table_ind] = buffer[pos2++];
                        dht_lookup_bits[tab][table_ind] = (code_val << (32 - i_len));
                        dht_lookup_mask[tab][table_ind] = ((1 << (i_len)) - 1) << (32 - i_len);
                        dht_lookup_bitlen[tab][table_ind] = i_len;

                        table_ind++; code_val++;
                    }
                    code_val <<= 1;
                }
                dht_lookup_size[tab] = table_ind;
                pos = pos2;
            } while(pos < cnt - 2);
        } else if(c == '\xda') { // start frame, followed by entropy-coded data
            c = fgetc_(); assert(c == '\x00');
            c = fgetc_(); assert(c == '\x0c'); // length: 12
            c = fgetc_(); assert(c == '\x03'); // three components
            for(i = 0; i < 3; i++) {
                int comp_id;
                c = fgetc_(); comp_id = c;
                c = fgetc_();
                if(comp_id == 1) { // Y
                    huffman_DCY = (unsigned)(c) >> 4;
                    huffman_ACY = (unsigned)(c) & 0x0f;
                }
            }
            // Y: 0/0, Cb & Cr: 1/1
            c = fgetc_(); c = fgetc_(); c = fgetc_(); // skip three bytes

            int data; uc = fgetc_(); int bitlen = 16; int bits_used = 0;
            data = uc << 24; uc = fgetc_(); data |= uc << 16;

            // read one XC coefficient from any table via huffmann code
            int readXC(int tab) {
                while(bitlen < 16) {
                    uc = fgetc_();
                    data = data | (uc << (24 - bitlen));
                    bitlen += 8;
                    if(((char)uc) == '\xff') {
                        uc = fgetc_();
                        if(((char)uc) == '\x00') {
                            uc = fgetc_();
                        }
                        data = data | (uc << (24 - bitlen));
                        bitlen += 8;
                    }
                }
                int ind = 0; int done = 0; int res = -1;
                while(done == 0) {
                    if((data & dht_lookup_mask[tab][ind]) == dht_lookup_bits[tab][ind]) {
                        res = dht_lookup_code[tab][ind];
                        bits_used = dht_lookup_bitlen[tab][ind];
                        done = 1;
                    }
                    ind++;

                    if(ind >= dht_lookup_size[tab]) {
                        /*fprintf(stderr,"Code not found in huffmann table.\n");*/ return -1;
                    }
                }
                data = data << bits_used; bitlen -= bits_used;
                while(bitlen < 16) {
                    uc = fgetc_();
                    data = data | (uc << (24 - bitlen));
                    bitlen += 8;
                    if(((char)uc) == '\xff') {
                        uc = fgetc_();
                        if(((char)uc) == '\x00') {
                            uc = fgetc_();
                        }
                        data = data | (uc << (24 - bitlen));
                        bitlen += 8;
                    }
                }
                return res;
            }

            int readTable(int tabDC, int tabAC, int * lastDC, int * table) {
                int j;
                for(j = 0; j < 64; j++) {
                    table[j] = 0;
                }
                int size_val = readXC(tabDC); if(size_val < 0) {
                    return -1;
                }
                int DC = (data >> (32 - size_val)) & mask[size_val]; data <<= size_val; bitlen -= size_val; // true value is next size_val bits
                DC = HUFF_EXTEND(DC, size_val); DC += *lastDC; *lastDC = DC;
                table[0] = DC;
                j = 0; int EOB = 0;
                while(j < 63 && (EOB == 0)) {
                    int comb = readXC(tabAC); if(comb < 0) {
                        return -1;
                    }
                    int val = comb & 0x0f;
                    int repeat = comb >> 4;
                    if(val == 0) {
                        if(repeat == 0x0f) {
                            j += 16;
                        } else {
                            EOB = 1;
                        }
                    } else {
                        j += repeat;
                        DC = (data >> (32 - val)) & mask[val]; data <<= val; bitlen -= val;
                        DC = HUFF_EXTEND(DC, val); table[j+1] = DC; j++;
                    }
                }
                if(j <= 63) {
                    return 0;
                } else {
                    /*fprintf(stderr,"ERROR: too many AC entries: %d\n",j);*/ return -1;
                }
            }

            cnt = ((int)(width / 8 / scaleH_Y + 0.5)) * ((int)(height / 8 / scaleV_Y + 0.5));

            int ctx = (int)(width / 8 + 0.5); int cty = (int)(height / 8 + 0.5);
            ctx /= 2; cty /= 2; int rad = ctx / 2; if(cty < ctx) {
                rad = cty / 2;
            }
            rad = rad * rad;
            int xp = 0; int yp = 0;

            int lastDCY = 0; int lastDCCr = 0; int lastDCCb = 0;
            int j; for(j = 0; j < 64; j++) {
                sumAC[j] = 0.0;
                sumSqrAC[j] = 0.0;
            }
            for(i = 0; i < cnt; i++) {
                int table[128];

                if(readTable(huffman_DCY, huffman_ACY + 1, &lastDCY, table) < 0) {
                    goto err;
                }

                // weight by distance to center (gaussian?)
                double xp_ = xp - ctx; double yp_ = yp - ctx;
                double weight = exp(-(xp_ * xp_) / rad - (yp_ * yp_) / rad);
                for(j = 0; j < 64; j++) {
                    double x = (table[j] * QT[0][j]) * (table[j] * QT[0][j]) * weight;
                    sumAC[j] += x;
                    sumSqrAC[j] += x * x;
                }
                cnt2++;
                xp++;
                if(readTable(huffman_DCY, huffman_ACY + 1, &lastDCY, table) < 0) {
                    goto err;
                }
                xp_ = xp - ctx; yp_ = yp - ctx;
                weight = exp(-(xp_ * xp_) / rad - (yp_ * yp_) / rad);
                for(j = 0; j < 64; j++) {
                    double x = (table[j] * QT[0][j]) * (table[j] * QT[0][j]) * weight;
                    sumAC[j] += x;
                    sumSqrAC[j] += x * x;
                }
                cnt2++;

                // ignore  C components
                if(readTable(2, 3, &lastDCCr, table) < 0) {
                    goto err;
                }
                if(readTable(2, 3, &lastDCCb, table) < 0) {
                    goto err;
                }

                xp++; if(xp == (int)(width / 8 + 0.5)) {
                    xp = 0;
                    yp++;
                }
            }
        } else if(c == '\xd9') { // end of file
        } else if(c == '\xc0') { // start of frame
            c = fgetc_(); cnt = (unsigned char)(c) * 256;
            c = fgetc_(); cnt += (unsigned char)(c);

            uc = fgetc_();
            c = fgetc_(); cnt = (unsigned char)(c) * 256;
            c = fgetc_(); cnt += (unsigned char)(c);
            height = cnt;
            c = fgetc_(); cnt = (unsigned char)(c) * 256;
            c = fgetc_(); cnt += (unsigned char)(c);
            width = cnt;
            uc = fgetc_();
            assert((int)uc == 3);

            int j;
            for(j = 0; j < uc; j++) {
                c = fgetc_(); int id = (unsigned)c;
                c = fgetc_();
                if(c != '\x11') {
                    if(id == 1) {
                        scaleH_Y = ((unsigned)c) >> 4;
                        scaleV_Y = ((unsigned)c) & 0x0f;
                    } else {
                        fprintf(stderr, "Sampling > 1 not supported for non-Y channels.\n");
                        exit(-2);
                    }
                }
                c = fgetc_();
                if(id == 1) {
                    quant_Y = (unsigned)c;
                }
            }
        } else {
        }
    }
    int j; int lenCurSeq = 2; int lenPrevTotal = 1; int valCurSeq = 1;
    double sum = 0.0;
    for(j = 0; j < 4; j++) {
        free(QT[j]);
    }
    for(j = 1; j < 21; j++) {
        if(j >= lenPrevTotal + lenCurSeq) {
            lenCurSeq++;
            lenPrevTotal = j;
            valCurSeq++;
        }
        sumAC[j] /= (double)(cnt2);
        sum += (double)valCurSeq * sumAC[j];
    }
    return sum;

err:
    return -1.0;
}
