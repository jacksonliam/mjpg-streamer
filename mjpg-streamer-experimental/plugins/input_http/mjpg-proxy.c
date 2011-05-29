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

#include "http_utils.h"


int sockfd;


#define HEADER 1
#define CONTENT 0
#define BUFFER_SIZE 1024 * 100
#define NETBUFFER_SIZE 1024 * 4
#define TRUE 1
#define FALSE 0

const char * CONTENT_LENGTH = "Content-Length:";
// TODO: this must be decoupled from mjpeg-streamer
const char * BOUNDARY =     "--boundarydonotcross";

struct extractor_state state;

void init_extractor_state(struct extractor_state * state) {
    state->length = 0;
    state->part = HEADER;
    state->last_four_bytes = 0;
    state->contentlength.string = CONTENT_LENGTH;
    state->boundary.string = BOUNDARY;
    search_pattern_reset(&state->contentlength);
    search_pattern_reset(&state->boundary);
}

// dumb 4 byte storing to detect double CRLF
int is_crlf(int bytes) {
    int result = (((13 << 8) | (10)) & bytes) == ((13 << 8) | (10));
    return result ;
}

int is_crlfcrlf(int bytes) {
    int result = is_crlf(bytes) && is_crlf(bytes >> 16);
    return result;

}
void push_byte(int * bytes, char byte) {
    * bytes = ((* bytes) << 8) | byte ;
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
                search_pattern_reset(&state->contentlength);
            else {
                search_pattern_compare(&state->contentlength, buffer[i]);
                if (search_pattern_matches(&state->contentlength))
                    DBG("Content length found\n");
            }
            break;

        case CONTENT:
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

char request [] = "GET /?action=stream HTTP/1.0\r\n\r\n";

void send_request_and_process_response() {
    int recv_length;
    char netbuffer[NETBUFFER_SIZE];

    init_extractor_state(&state);
    
    // send request
    send(sockfd, request, sizeof(request), 0);

    // and listen for answer until sockerror or THEY stop us 
    // TODO: we must handle EINTR here, it really might occur
    while ( (recv_length = recv(sockfd, netbuffer, sizeof(netbuffer), 0)) > 0 && !*(state.should_stop))
        extract_data(&state, netbuffer, recv_length);

}


char * host = "localhost";
char * port = "8080";
char * unknown_param = "unknown_param";

char * get_param_value(const char * name){
  if (strcmp(name, "host") ==0)
    return host;
  else if (strcmp(name, "port") == 0)
    return port;
  else return unknown_param;
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
                " ---------------------------------------------------------------\n", program_name);
}
// TODO: this must be reworked, too. I don't know how
void show_version() {
    printf("Version - %s\n", VERSION);
}

int parse_cmd_line(int argc, char * argv []) {
    while (TRUE) {
        static struct option long_options [] = {
            {"help", no_argument, 0, 'h'},
            {"version", no_argument, 0, 'v'},
            {"host", required_argument, 0, 'H'},
            {"port", required_argument, 0, 'p'},
            {0,0,0,0}
        };

        int index = 0, c = 0;
        c = getopt_long_only(argc,argv, "hvH:p:", long_options, &index);

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
                host = strdup(optarg);
                break;
            case 'p' :
                port = strdup(optarg);
                break;
            }
    }

  return 0;
}

// TODO: consider using hints for http
// TODO: maybe remove explicit exit() call. I don't know how it works in threads
// TODO: consider moving delays to plugin command line arguments
void connect_and_stream(){
    struct addrinfo * info, * rp;
    int errorcode;
    while (TRUE) {
        errorcode = getaddrinfo(host, port, NULL, &info);
        if (errorcode) {
            perror(gai_strerror(errorcode));
            exit(EXIT_FAILURE);
        };
        for (rp = info ; rp != NULL; rp = rp->ai_next) {
            sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (sockfd <0) {
                perror("Can't allocate socket, will continue probing\n");
                continue;
            }

            DBG("socket value is %d\n", sockfd);
            if (connect(sockfd, (struct sockaddr *) rp->ai_addr, rp->ai_addrlen)>=0 ) {
                DBG("connected to host\n");
                break;
            }

            close(sockfd);

        }

        freeaddrinfo(info);

        if (rp==NULL) {
            perror("Can't connect to server, will retry in 5 sec");
            sleep(5);
        }
        else
        {
            send_request_and_process_response();
            
            DBG ("Closing socket\n");
            close (sockfd);
            if (*state.should_stop)
              break;
            sleep(1);
        };
    }

}
