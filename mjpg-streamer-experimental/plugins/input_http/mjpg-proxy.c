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

const char * CONTENTLENGTH_STRING = "Content-Length:";
const char * BOUNDARY_STRING = "boundary=";

void init_extractor_state(struct extractor_state * state) {
    state->length = 0;
    state->part = HEADER;
    state->last_four_bytes = 0;
    search_pattern_reset(&state->contentlength_string);
    search_pattern_reset(&state->boundary_string);
    search_pattern_reset(&state->boundary);
}

void init_mjpg_proxy(struct extractor_state * state){
    state->hostname = strdup("localhost");
    state->port = strdup("8080");
    state->path = strdup("/?action=stream");

    state->contentlength_string.string = CONTENTLENGTH_STRING;
    state->boundary_string.string = BOUNDARY_STRING;
    state->boundary.string = strdup("--boundarydonotcross");

    init_extractor_state(state);
}

// main method
// we process all incoming buffer byte per byte and extract binary data from it to state->buffer
// if boundary is detected, then callback for image processing is run
// TODO; decouple from mjpeg streamer and ensure content-length processing
//       for that, we must properly work with headers, not just detect them
void extract_data(struct extractor_state * state, char * buffer, int length) {

    int i;
    char * buffer2;

    if (strncmp (buffer, "HTTP/", 5) == 0) {
        // HTTP response - we will try to parse at least the boundary string
        for (i = 0; i < length && !*(state->should_stop); i++) {
	    search_pattern_compare(&state->boundary_string, buffer[i]);
            if (search_pattern_matches(&state->boundary_string)) {
	        DBG("Boundary_string found, let's parse the value!\n");
		i++;
		buffer2 = strstr(&buffer[i], "\r\n");
		if(buffer2 == NULL)
		    return;
		if (buffer[i]=='-' && buffer[i+1]=='-')
		    i=i+2;	// sometimes the string already contains the "--" prefix
		if(buffer2 == &buffer[i])
		    return;	// the string seems to be empty, rather use the default value

                *buffer2 = '\0';
                free(state->boundary.string);
                state->boundary.string = (char*) malloc (strlen(&buffer[i]) + 3 * sizeof(char));
                sprintf(state->boundary.string, "--%s", &buffer[i]);
                search_pattern_reset(&state->boundary);

                DBG("The new boundary string: %s\n", state->boundary.string);
		return;
	    }
	}
        return;		// the rest of this response is supposed to be also part of http header
    }

    for (i = 0; i < length && !*(state->should_stop); i++) {
        switch (state->part) {
        case HEADER:
            push_byte(&state->last_four_bytes, buffer[i]);
            if (is_crlfcrlf(state->last_four_bytes))
                state->part = CONTENT;
            /*else if (is_crlf(state->last_four_bytes))
                search_pattern_reset(&state->contentlength_string);
            else {
                search_pattern_compare(&state->contentlength_string, buffer[i]);
                if (search_pattern_matches(&state->contentlength_string)) {
                    DBG("Content length found\n");
                    search_pattern_reset(&state->contentlength_string);
                }
            }*/
            break;

        case CONTENT:
            if (state->length >= BUFFER_SIZE - 1) {
                perror("Buffer too small\n");
                break;
            }
            state->buffer[state->length++] = buffer[i];
            search_pattern_compare(&state->boundary, buffer[i]);
            if (search_pattern_matches(&state->boundary)) {
                state->length -= (strlen(state->boundary.string)+2); // magic happens here
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

    DBG("path is: %s\n", state->path);
    char *request = (char*) malloc (19 * sizeof(char) + sizeof(state->path));
    sprintf(request, "GET %s HTTP/1.0\r\n\r\n", state->path);

    // send request
    send(state->sockfd, request, strlen(request), 0);

    free (request);

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
                " [-H | --host]............: select host to data from, localhost is default\n"
                " [-p | --port]............: port, defaults to 8080\n"
                " [-P | --path]............: path (+query), defaults to /?action=stream\n"
                " ---------------------------------------------------------------\n", program_name);
}
// TODO: this must be reworked, too. I don't know how
void show_version() {
    printf("Version - %s\n", VERSION);
}

int parse_cmd_line(struct extractor_state * state, int argc, char * argv []) {
    while (TRUE) {
        static struct option long_options [] = {
            {"help", no_argument, 0, 'h'},
            {"version", no_argument, 0, 'v'},
            {"host", required_argument, 0, 'H'},
            {"port", required_argument, 0, 'p'},
            {"path", required_argument, 0, 'P'},
            {0,0,0,0}
        };

        int index = 0, c = 0;
        c = getopt_long_only(argc,argv, "hvH:p:P:", long_options, &index);

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
            case 'P' :
                free(state->path);
                state->path = strdup(optarg);
                break;
            }
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
    free(state->path);
    free(state->boundary.string);
}

