/*******************************************************************************
#                                                                              #
#      Copyright (C) 2011 Eugene Katsevman                                     #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>


#include "version.h"

#include "mjpg-proxy.h"

#include "misc.h"



#define HEADER 1
#define CONTENT 0
#define NETBUFFER_SIZE 1024 * 4
#define TRUE 1
#define FALSE 0

const char * CONTENT_LENGTH = "Content-Length:";
char request_str[256];
unsigned char * encoded = NULL;
static const unsigned char base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
 *
 * base64_encode - Base64 encode
 * @src: Data to be encoded
 * @len: Length of the data to be encoded
 * @out_len: Pointer to output length variable, or %NULL if not used
 * Returns: Allocated buffer of out_len bytes of encoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer. Returned buffer is
 * nul terminated to make it easier to use as a C string. The nul terminator is
 * not included in out_len.
 */
unsigned char * base64_encode(const unsigned char *src, size_t len, size_t *out_len)
{
	unsigned char *out, *pos;
	const unsigned char *end, *in;
	size_t olen;
	int line_len;

	olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
	olen += olen / 72; /* line feeds */
	olen++; /* nul termination */
	if (olen < len)
		return NULL; /* integer overflow */
	out = malloc(olen);
	if (out == NULL)
		return NULL;

	end = src + len;
	in = src;
	pos = out;
	line_len = 0;
	while (end - in >= 3) {
		*pos++ = base64_table[in[0] >> 2];
		*pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
		*pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
		*pos++ = base64_table[in[2] & 0x3f];
		in += 3;
		line_len += 4;
		if (line_len >= 72) {
			*pos++ = '\n';
			line_len = 0;
		}
	}

	if (end - in) {
		*pos++ = base64_table[in[0] >> 2];
		if (end - in == 1) {
			*pos++ = base64_table[(in[0] & 0x03) << 4];
			*pos++ = '=';
		} else {
			*pos++ = base64_table[((in[0] & 0x03) << 4) |
					      (in[1] >> 4)];
			*pos++ = base64_table[(in[1] & 0x0f) << 2];
		}
		*pos++ = '=';
		line_len += 4;
	}

	if (line_len)
		*pos++ = '\n';

	*pos = '\0';
	if (out_len)
		*out_len = pos - out;
	return out;
}

void init_extractor_state(struct extractor_state * state) {
    state->length = 0;
    state->part = HEADER;
    state->last_four_bytes = 0;
    state->contentlength_pattern.string = CONTENT_LENGTH;
    state->boundary_pattern.string = state->boundary;
    search_pattern_reset(&state->contentlength_pattern);
    search_pattern_reset(&state->boundary_pattern);
}

void init_mjpg_proxy(struct extractor_state * state){
    state->hostname = strdup("localhost");
    state->port = strdup("8080");
    state->request = strdup("/?action=stream");
    state->credentials = NULL;
    state->boundary = strdup("--boundarydonotcross");

    init_extractor_state(state);
}

// main method
// we process all incoming buffer byte per byte and extract binary data from it to state->buffer
// if boundary is detected, then callback for image processing is run
// TODO; decouple from mjpeg streamer and ensure content-length processing
//       for that, we must properly work with headers, not just detect them
void extract_data(struct extractor_state * state, char * buffer, int length) {
    int i;
    for (i = 0; i < length && !*(state->should_stop); i++) {
        switch (state->part) {
        case HEADER:
            push_byte(&state->last_four_bytes, buffer[i]);
            if (is_crlfcrlf(state->last_four_bytes))
                state->part = CONTENT;
            else if (is_crlf(state->last_four_bytes))
                search_pattern_reset(&state->contentlength_pattern);
            else {
                search_pattern_compare(&state->contentlength_pattern, buffer[i]);
                if (search_pattern_matches(&state->contentlength_pattern)) {
                    DBG("Content length found\n");
                    search_pattern_reset(&state->contentlength_pattern);
                }
            }
            break;

        case CONTENT:
            if (state->length >= BUFFER_SIZE - 1) {
                perror("Buffer too small\n");
                break;
            }
            state->buffer[state->length++] = buffer[i];
            search_pattern_compare(&state->boundary_pattern, buffer[i]);
            if (search_pattern_matches(&state->boundary_pattern)) {
                state->length -= (strlen(state->boundary_pattern.string)+2); // magic happens here
                DBG("Image of length %d received\n", (int)state->length);
                if (state->on_image_received) // callback
                  state->on_image_received(state->buffer, state->length);
                init_extractor_state(state); // reset fsm
            }
            break;
        }

    }

}

void send_request_and_process_response(struct extractor_state * state) {
    int recv_length;
    char netbuffer[NETBUFFER_SIZE];

    init_extractor_state(state);
    
    // send request
    send(state->sockfd, request_str, strlen(request_str), 0);

    // and listen for answer until sockerror or THEY stop us 
    // TODO: we must handle EINTR here, it really might occur
    while ( (recv_length = recv(state->sockfd, netbuffer, sizeof(netbuffer), 0)) > 0 && !*(state->should_stop))
        extract_data(state, netbuffer, recv_length);

}

// TODO:this must be reworked to decouple from mjpeg-streamer
void show_help(char * program_name) {

fprintf(stderr, " ---------------------------------------------------------------\n" \
                " Help for input plugin..: %s\n" \
                " ---------------------------------------------------------------\n" \
                " The following parameters can be passed to this plugin:\n\n" \
                " [-v | --version ]........: current SVN Revision\n" \
                " [-h | --help]............: show this message\n"
                " [-H | --host]............: host, defaults to localhost\n"
                " [-p | --port]............: port, defaults to 8080\n"
                " [-r | --request].........: request, defaults to /?action=stream\n"
                " [-c | --credentials].....: credentials, defaults to NULL\n"
                " [-b | --boundary]........: boundary, defaults to --boundarydonotcross\n"
                " ---------------------------------------------------------------\n", program_name);
}
// TODO: this must be reworked, too. I don't know how
void show_version() {
    printf("Version - %s\n", VERSION);
}

int parse_cmd_line(struct extractor_state * state, int argc, char * argv []) {
	size_t encoded_len;

    while (TRUE) {
        static struct option long_options [] = {
            {"help", no_argument, 0, 'h'},
            {"version", no_argument, 0, 'v'},
            {"host", required_argument, 0, 'H'},
            {"port", required_argument, 0, 'p'},
            {"request", required_argument, 0, 'r'},
            {"credentials", required_argument, 0, 'c'},
            {"boundary", required_argument, 0, 'b'},
            {0,0,0,0}
        };

        int index = 0, c = 0;
        c = getopt_long_only(argc,argv, "hvH:p:r:c:b:", long_options, &index);

        if (c==-1) break;

        if (c=='?'){
            show_help(argv[0]);
            return 1;
            }
        else
            switch (c) {
            case 'h' :
                show_help(argv[0]);
                return 1;
                break;
            case 'v' :
                show_version();
                return 1;
                break;
            case 'H' :
                free(state->hostname);
                state->hostname = strdup(optarg);
                break;
            case 'p' :
                free(state->port);
                state->port = strdup(optarg);
                break;
            case 'r' :
                free(state->request);
                state->request = strdup(optarg);
                break;
            case 'c' :
                free(state->credentials);
                state->credentials = strdup(optarg);
                break;
            case 'b' :
                free(state->boundary);
                state->boundary = strdup(optarg);
                break;
            }
    }
    // See if we have credentials and build request accordingly
    if (state->credentials != NULL) {
		sprintf(request_str, "GET %s HTTP/1.0\r\nAuthorization: Basic %s\r\n\r\n", state->request, base64_encode(state->credentials, strlen(state->credentials), &encoded_len));
    } else {
		sprintf(request_str, "GET %s HTTP/1.0\r\n\r\n", state->request);
    }
  return 0;
}

// TODO: consider using hints for http

// TODO: consider moving delays to plugin command line arguments
void connect_and_stream(struct extractor_state * state){
    struct addrinfo * info, * rp;
    int errorcode;

    while (TRUE) {
        errorcode = getaddrinfo(state->hostname, state->port, NULL, &info);
        if (errorcode) {
            perror(gai_strerror(errorcode));
        };
        for (rp = info ; rp != NULL; rp = rp->ai_next) {
            state->sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (state->sockfd <0) {
                perror("Can't allocate socket, will continue probing\n");
                continue;
            }

            DBG("socket value is %d\n", state->sockfd);
            if (connect(state->sockfd, (struct sockaddr *) rp->ai_addr, rp->ai_addrlen)>=0 ) {
                DBG("connected to host\n");
                break;
            }

            close(state->sockfd);

        }

        freeaddrinfo(info);

        if (rp==NULL) {
            perror("Can't connect to server, will retry in 5 sec");
            sleep(5);
        }
        else
        {
            send_request_and_process_response(state);
            
            DBG ("Closing socket\n");
            close (state->sockfd);
            if (*state->should_stop)
              break;
            sleep(1);
        };
    }

}

void close_mjpg_proxy(struct extractor_state * state){
    free(state->hostname);
    free(state->port);
    free(state->request);
    free(state->credentials);
    free(state->boundary);
    free(encoded);
}

