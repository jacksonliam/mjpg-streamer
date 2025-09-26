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
#include <sys/time.h> // for setsockopt timeout

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

#define NETBUFFER_SIZE 1024 * 8
#define TRUE 1
#define FALSE 0

const char * CONTENT_LENGTH = "Content-Length:";
// TODO: this must be decoupled from mjpeg-streamer
char * BOUNDARY =     "--boundarydonotcross";

void initBoundary(const char * buffer, struct extractor_state * state);


// ////////////////////////////////////////////////////////////////////////

void initBoundary(const char * buffer, struct extractor_state * state) {
    /* This function is expecting to be called once per stream. The first response from 
        the stream will contain the boundary identifier. i.e. the value (which is preceded 
        by '--' in the content but not in the headers). The header for identifying the 
        boundary identifier is 'boundary='. 
        
        This function will set the glabal data boundary.
        */
    const int HEADER_SIZE = 4096;
    char headers[HEADER_SIZE];  
    if (strncmp(buffer, "HTTP", 4)) {
        // we have an issue here - there is no boundary string defined here
        // so revert to existing behaviour
        DBG("Initial Response Received does not begin HTTP");
        strcpy(state->boundary, BOUNDARY);
    } else {
        char * endOfHeaders = strstr(buffer,"\r\n\r\n");
        if (endOfHeaders == NULL) {
            // hmmm! first response is a HTTP message but not properly formed
            perror("HTTP Message does not contain a 2 empty lines after the header");
            return;
        }            
        int headerLen = endOfHeaders - buffer;
        if (headerLen > HEADER_SIZE - 1) {
            // whoopsie
            perror("HTTP Message headers are too long for this program");
            return;
        }
        strncpy(headers, buffer, headerLen);
        headers[headerLen] = '\0';

        // look for boundary in the headers
        const char * boundaryTag = "boundary=";
        char * boundaryStart = strstr(headers, boundaryTag);
        if (boundaryStart == NULL) {
            // revert to existing behaviour (truly this is an error)
            DBG("Failed to find %s in initialMessage. Headers: %s", boundaryTag, headers);
            strcpy(state->boundary, BOUNDARY);
        } else {
            // ok, we have the boundary header - extract the value
            strcpy(state->boundary, "--");
            boundaryStart += strlen(boundaryTag);
            char *boundaryEnd = strstr(boundaryStart, "\r\n");
            if (boundaryEnd != NULL) {
                int boundaryLen = boundaryEnd - boundaryStart;
                strncat(state->boundary, boundaryStart, boundaryLen);
                state->boundary[boundaryLen] = 0;
            } else {
                // the last header
                strcat(state->boundary, boundaryStart);
            }
        }
    }
    DBG("Initial Response Received: %s\n", headers);
    DBG("Boundary String set to: %s\n", state->boundary);
    return; // we have received the intial response only - go collect more
}


void init_extractor_state(struct extractor_state * state) {
    // Here's where the intialization goes when we open a stream
    state->length = 0;
    state->boundary[0] = '\0';
    state->frameLength = 0;
    state->collecting = false;

//   state->part = HEADER;
 //   state->last_four_bytes = 0;
 //   state->contentlength.string = CONTENT_LENGTH;
//    state->boundary.string = BOUNDARY;
 //   search_pattern_reset(&state->contentlength);
 //   search_pattern_reset(&state->boundary);
}

void init_mjpg_proxy(struct extractor_state * state){
    // Here's where the intialization of program based variables goes
    state->hostname = strdup("localhost");
    state->port = strdup("8080");
    strcpy(state->path, "/");

//   init_extractor_state(state);
}

// main method
// we process all incoming buffer byte per byte and extract binary data from it to state->buffer
// if boundary is detected, then callback for image processing is run
// TODO; decouple from mjpeg streamer and ensure content-length processing
//       for that, we must properly work with headers, not just detect them
void extract_data(struct extractor_state * state, char * buffer, int length) {
    DBG("New message received\n");
    if (strlen(state->boundary) == 0) {
        initBoundary(buffer, state);
    }

    // boundary has been intialized - yay!
    if (state->collecting) { // collecting is set whenever we find a boundary
        memcpy(&state->buffer[state->length], buffer, length);
        state->length += length;
        DBG("Adding new data to open frame\n");
        if (state->length >= state->frameLength) { // frame length is included with a new boundary
            // there is a CRLF at the end of all frames - this is now being deleted
            state->length = state->frameLength;
            state->collecting = false; // we're done with this frame
            state->on_image_received(state->buffer, state->length); // pass on the frame
            state->length = 0; // reset the buffer
        }
        return;
    } else {
        // We're not collecting, but would like to. Look for a message beginning with boundary
        if (strncmp(buffer, state->boundary, strlen(state->boundary)) == 0) {
            // we have a new frame - get the frame length and start a new frame buffer
            char * cLength = strstr(buffer, CONTENT_LENGTH);
            if (cLength == NULL) {
                // new frame, no content-length, just keep looking
                perror("No content-length sent with a boundary marker");
                return;
            }
            cLength += strlen(CONTENT_LENGTH);
            char * lengthEnd = strstr(cLength, "\r\n");
            const int MAX_LEN_LEN=24;
            char contLength[MAX_LEN_LEN];
            strncpy(contLength, cLength, lengthEnd - cLength);
            contLength[lengthEnd - cLength] = '\0';
            state->frameLength = atoi(contLength);
            DBG("Receiving frame of length %d, %s\n", state->frameLength, contLength);
            char * data = strstr(cLength, "\r\n\r\n");
            data += 4;
            state->length = length - (data - buffer); // first entry
            memcpy(state->buffer, data, state->length);
            state->collecting = true;
        } else {
            // we typically get here whenever the stream is being established - a frame has not been opened
            DBG("Discarding Frame: %s not collecting\n", state->boundary);
        }
    }
}

/*
void extract_data_defunct(struct extractor_state * state, char * buffer, int length) {
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
                if (search_pattern_matches(&state->contentlength)) {
                    DBG("Content length found\n");
                    search_pattern_reset(&state->contentlength);
                }
            }
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
*/

//char request [] = "GET /1/mjpg/stream?action=stream HTTP/1.0\r\n\r\n";
//char request [] = "GET /1/mjpg/stream HTTP/1.0\r\n\r\n"; // motionplus

void send_request_and_process_response(struct extractor_state * state) {
    int recv_length;
    char netbuffer[NETBUFFER_SIZE];

    init_extractor_state(state);

    const int REQUEST_MAX_LEN = 256;
    char request[REQUEST_MAX_LEN];
    strcpy(request, "GET ");
    strcat(request, state->path);
    strcat(request, " HTTP/1.0\r\n\r\n");      
   
    // send request - error here. We need to check the success of send. Otherwise we just hang on the recv.
    int sendLen = send(state->sockfd, request, strlen(request), 0);
//    printf("Response from socket send is: %i\n", sendLen);

    // and listen for answer until sockerror or THEY stop us 
    // TODO: we must handle EINTR here, it really might occur
    while ( (recv_length = recv(state->sockfd, netbuffer, sizeof(netbuffer), 0)) > 0 && !*(state->should_stop))
        extract_data(state, netbuffer, recv_length);
    DBG("Ending Stream \n");
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
                " [-t | --path]............: path to the stream, defaults to '/'"
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
            {"path", required_argument, 0, 't'},
            {0,0,0,0}
        };

        int index = 0, c = 0;
        c = getopt_long_only(argc,argv, "hvHt:p:", long_options, &index);

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
            case 't' :
                strcpy(state->path, optarg);
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
    struct addrinfo hints; //
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP socket

    while (TRUE) {
        // Hints must be set for connect to work correctly - if the socket is not TCP then its not correct
        errorcode = getaddrinfo(state->hostname, state->port, &hints, &info);
        if (errorcode) {
            perror(gai_strerror(errorcode));
            info = NULL;
        }
        for (rp = info ; rp != NULL; rp = rp->ai_next) {
            // printf("Socket Info - family %i, socktype: %i, protocol: %i \n", rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            state->sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (state->sockfd <0) {
                perror("Can't allocate socket, will continue probing\n");
                continue;
            }

            struct timeval tv;
            tv.tv_sec = 10; // 10 seconds
            tv.tv_usec = 0; // 0 microseconds
            setsockopt(state->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

            DBG("socket value is %d\n", state->sockfd);
            int result = connect(state->sockfd, (struct sockaddr *) rp->ai_addr, rp->ai_addrlen);
            if (result >= 0 ) {
                DBG("connected to host. Result of connect: %i, socket: %i\n", result, state->sockfd);
                break;
            }

            close(state->sockfd);
        }

        freeaddrinfo(info);

        if (rp==NULL) {
            const int sleepTimer = 30;
            perror("Can't connect to server, will retry in 30 sec");
            sleep(sleepTimer);
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
}

