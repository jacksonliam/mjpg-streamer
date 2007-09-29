/* 
*      control.c
*
*      HTTP Control interface for uvc_stream
*      
*      2007 Lucas van Staden (lvs@softhome.net) 
*
*        Please send ONLY GET requests with the following commands:"
*        "PAN     : ?pan=[-]<pan value> -  Pan camera left or right"
*        "TILT    : ?tilt=[-]<tilt value>; - Tilt camera up or down"
*        "RESET   : ?reset=<value> - Reset 1 - pan, 2 tilt, 3 pan/tilt"
*        "CONTROL : ?control - control screen with buttons to pan and tilt"
*
*	
*      this code is heavily based on the webhttpd.c code from the motion project
*
*      Copyright 2004-2005 by Angel Carpintero  (ack@telefonica.net)
*      This software is distributed under the GNU Public License Version 2
*
*/
#include "control.h"
#include "v4l2uvc.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>          
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

pthread_mutex_t httpd_mutex;
          
int warningkill; // This is a dummy variable use to kill warnings when not checking sscanf and similar functions

static char* click_control =
        "<html><head></head><body>"
        "<img id='stream' alt='stream' src=''><br/>\n"
        "<input type='button' value='pan left' onclick='javascript:makeRequest(\"pan=-3\");'>&nbsp;\n"
        "<input type='button' value='pan right' onclick='javascript:makeRequest(\"pan=3\");'>&nbsp;\n"
        "<input type='button' value='tilt up' onclick='javascript:makeRequest(\"tilt=3\");'>&nbsp;\n"
        "<input type='button' value='tilt down'  onclick='javascript:makeRequest(\"tilt=-3\");'><br>\n"
        "<input type='button' value='reset pan' onclick='javascript:makeRequest(\"reset=1\");'>&nbsp;\n"
        "<input type='button' value='reset tilt' onclick='javascript:makeRequest(\"reset=2\");'>&nbsp;\n"
        "<input type='button' value='reset pan/tilt' onclick='javascript:makeRequest(\"reset=3\");'>&nbsp;\n"
        "<br><br>Note: If you find that the streamed image above does not update, try and disable your browser's cache\n"
        "<script type='text/javascript' language='javascript'>\n"
        " var http_request = false;\n"
        " var control_port = %d \n"
        " document.getElementById('stream').src='http://'+window.location.hostname+':%d'\n"
        " function makeRequest(parameters) {\n"
        " 	http_request = false;\n"
        "  	if (window.XMLHttpRequest) { // Mozilla, Safari,...\n"
        "    		http_request = new XMLHttpRequest();\n"
        "    		if (http_request.overrideMimeType) {\n"
        " 			// set type accordingly to anticipated content type\n"
        " 			//http_request.overrideMimeType('text/xml');\n"
        " 			http_request.overrideMimeType('text/html');\n"
        "    		}\n"
        "  	} else if (window.ActiveXObject) { // IE\n"
        "  		try {\n"
        "    			http_request = new ActiveXObject('Msxml2.XMLHTTP');\n"
        "  		} catch (e) {\n"
        "    			try {\n"
        "       			http_request = new ActiveXObject('Microsoft.XMLHTTP');\n"
        "  			} catch (e) {}\n"
        " 		}\n"
        "  	}\n"
        "  	if (!http_request) {\n"
        " 		alert('Cannot create XMLHTTP instance');\n"
        " 		return false;\n"
        "  	}\n"
        "    	http_request.onreadystatechange = alertContents;\n"
        "	http_request.open('GET', 'http://'+window.location.hostname+':'+control_port+'/?' + parameters, true);\n"
        "    	http_request.send(null);\n"
        " }\n"
        " function alertContents() {\n"
        "    if (http_request.readyState == 4) {\n"
        "      if (http_request.status == 200) {\n"
        "        //alert(http_request.responseText);\n"
        "        //result = http_request.responseText;\n"
        "        //document.getElementById('result').innerHTML = result;            \n"
        "      } else {\n"
        "        alert('There was a problem controlling the camera.');\n"
        "      }\n"
        "    }\n"
        " }\n"
        "</script>\n";

static const char* bad_request_response =
	"HTTP/1.0 400 Bad Request\r\n"
	"Content-type: text/html\r\n\r\n"
	"<html>\n"
	"<body>\n"
	"<h1>Bad Request</h1>\n"
	"<p>The server did not understand your request.</p>\n"
	"</body>\n"
	"</html>\n";

static const char* bad_method_response_template =
	"HTTP/1.0 501 Method Not Implemented\r\n"
	"Content-type: text/html\r\n\r\n"
	"<html>\n"
	"<body>\n"
	"<h1>Method Not Implemented</h1>\n"
	"<p>The method is not implemented by this server.</p>\n"
	"</body>\n"
	"</html>\n";


static const char* end_template =
	"</body>\n"
	"</html>\n";


static const char* ini_template =
	"<html><head></head>\n"
	"<body>\n";
	
static const char* unknown_command =
        "<html><head></head><body>"
        "error: Command unknown<br>"
        "Please send ONLY GET requests with the following commands:<br>"
        "<ol>"
        "<li>PAN     : ?pan=[-]&lt;pan value&gt; -  Pan camera left or right</li>"
        "<li>TILT    : ?tilt=[-]&lt;tilt value&gt; - Tilt camera up or down</li>"
        "<li>RESET   : ?reset=&lt;value&gt; - Reset 1 - pan, 2 tilt, 3 pan/tilt/<li>"
        "<li>CONTROL : ?control - control screen with buttons to pan and tilt</li>"
        "</ol>"
        "<br>"
        "<b>important:</b>Disable your browser cache control for a better experience !<br>";


static const char* ok_response =
	"HTTP/1.1 200 OK\r\n"
	"Server: uvcstream-httpd/\r\n"
	"Connection: close\r\n"
	"Max-Age: 0\r\n"
	"Expires: 0\r\n"
	"Cache-Control: no-cache\r\n"
	"Cache-Control: private\r\n"
	"Pragma: no-cache\r\n"
	"Content-type: text/html\r\n\r\n";
	

int uvc_reset(struct control_data *cd, int reset)
{
    /*int reset = 3; //0-non reset, 1-reset pan, 2-reset tilt, 3-reset pan&tilt */
    struct v4l2_control control_s;
    control_s.id = V4L2_CID_PANTILT_RESET;
    control_s.value = reset;
    if (ioctl(cd->video_dev, VIDIOC_S_CTRL, &control_s) < 0) {
    	fprintf(stderr, "failed to control motor\n");
	return 0;
    }
    return 1;
}

int uvc_move(struct coord *cent, struct control_data *cd)
{
	int dev = cd->video_dev;
	
	
	/* RELATIVE MOVING : Act.Position +/- X and Y */
	
	//int delta_x = cent->x - (imgs->width / 2);
	//int delta_y = cent->y - (imgs->height / 2);
	int move_x_degrees, move_y_degrees;
	
	if (( cd->minmaxfound != 1) || (cent->x == 7777 )) {
		/*int reset = 3; //0-non reset, 1-reset pan, 2-reset tilt, 3-reset pan&tilt */
		int reset = 3;
		struct v4l2_control control_s;
		control_s.id = V4L2_CID_PANTILT_RESET;
		control_s.value = (unsigned char) reset;
		  if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
		    	fprintf(stderr, "failed to control motor\n");
			return 0;
                  }
		SLEEP(1,0); // force a delay, as too many requests will make it go mad

		/* DWe 30.03.07 The orig request failed : 
		* must be VIDIOC_G_CTRL separate for pan and tilt or via VIDIOC_G_EXT_CTRLS - now for 1st manual 
		* Range X = -70 to +70 degrees              
		*	Y = -30 to +30 degrees  
		*/	

		cd->panmin = -4480 / INCPANTILT;
		cd->tiltmin = -1920 / INCPANTILT;
		cd->panmax = 4480 / INCPANTILT; 
		cd->tiltmax = 1920 / INCPANTILT;
		cd->pan_angle = 0;
		cd->tilt_angle = 0;
		cd->minmaxfound = 1;
	}
	
		move_x_degrees = cent->x;
		move_y_degrees = cent->y;

	union pantilt {
		struct {
			short pan;
			short tilt;
		} s16;
		int value;
	};

	struct v4l2_control control_s;
	union pantilt pan;

	if (cd->minmaxfound == 1) {
	/* Check current position of camera and see if we need to adjust
	values down to what is left to move */
		if (move_x_degrees<0 && (cd->panmin - cd->pan_angle) > move_x_degrees)
			move_x_degrees = (cd->panmin - cd->pan_angle);

		if (move_x_degrees>0 && (cd->panmax - cd->pan_angle) < move_x_degrees)
			move_x_degrees = (cd->panmax - cd->pan_angle);

		if (move_y_degrees<0 && (cd->tiltmin - cd->tilt_angle) > move_y_degrees)
			move_y_degrees = (cd->tiltmin - cd->tilt_angle);

		if (move_y_degrees>0 && (cd->tiltmax - cd->tilt_angle) < move_y_degrees)
			move_y_degrees = (cd->tiltmax - cd->tilt_angle);
	}


	/*
	tilt up: - value
	tilt down: + value
	pan left: - value
	pan right: + value
	*/

	pan.s16.pan = -move_x_degrees * INCPANTILT;
	pan.s16.tilt = -move_y_degrees * INCPANTILT;
	
	/* DWe 30.03.07 Must be broken in diff calls, because 
    	   - one call for both is not accept via VIDIOC_S_CTRL -> maybe via VIDIOC_S_EXT_CTRLS
    	   - The Webcam or uvcvideo does not like a call with a zero-move 
    	*/

	if (move_x_degrees != 0) {
		control_s.id = V4L2_CID_PAN_RELATIVE;

//	control_s.value = pan.value;
		control_s.value = pan.s16.pan;
		if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
		    return 0;
		}
	}
	
	/* DWe 30.03.07 We must wait a little,before we set the next CMD, otherwise PAN is mad ... */ 	
        if ((move_x_degrees != 0) && (move_y_degrees != 0)) {
	    SLEEP (1,0);
	}   

	if (move_y_degrees != 0) {

	control_s.id = V4L2_CID_TILT_RELATIVE;

//	control_s.value = pan.value;
	control_s.value = pan.s16.tilt;
		if (ioctl(dev, VIDIOC_S_CTRL, &control_s) < 0) {
		  return 0;
		}
	}
	
  
		if (cd->minmaxfound == 1) {
		
		    	if (move_x_degrees != 0){ 
			    cd->pan_angle += -pan.s16.pan / INCPANTILT;
			}	
			if (move_y_degrees != 0){
			    cd->tilt_angle += -pan.s16.tilt / INCPANTILT;
			}    
		}
	return 1;
}



static void send_template_ini_client(int client_socket, const char* template)
{
	write(client_socket, ok_response, strlen (ok_response));
	write(client_socket, template, strlen(template));
}

static void send_control_template(int client_socket, const char* template, int control_port,int stream_port)
{
	char *dynamic=NULL;
	dynamic = malloc(4096);
	sprintf(dynamic,template,htons(control_port),htons(stream_port));
	write(client_socket, ok_response, strlen (ok_response));
	write(client_socket, dynamic, strlen(dynamic));
}

static void send_template(int client_socket, char *res)
{
	write(client_socket, res, strlen(res));
}

static void send_template_end_client(int client_socket)
{
	write(client_socket, end_template, strlen(end_template));
}

static int handle_get(int client_socket, const char* url, struct control_data *cd)
{
  if (*url == '/' ){		
      char *res=NULL;
      char *pointer = (char *)url;
      char command[10];
      int value;
      int ssconfret;
      res = malloc(2048);
      
      
      // replace the = with a space to allow sscanf to work 
      // not very eligant, but works.
      ssconfret = sscanf (pointer,"/?%[a-z]=%d",command,&value);
      if (ssconfret == 2){
        send_template_ini_client(client_socket,ini_template);
        struct coord cent;
	cent.width = cd->width;
	cent.height = cd->height;
	cent.y = 0;
	cent.x = value;
	if (!strcmp(command,"pan")) {
		/* PAN RELATIVE */
		struct coord cent;
		cent.y = 0;
		cent.x = value;
		cd->moved = uvc_move(&cent, cd);
		if (cd->moved > 0) {
		    sprintf(res,"<b>pan:</b>%d",value);
                } else {
		    /*error in track action*/
                    sprintf(res, "pan:error");
                }		
	} 
	else if (!strcmp(command,"tilt")) {
		/* TILT RELATIVE */
		struct coord cent;
		cent.y = value;
		cent.x = 0;
		cd->moved = uvc_move(&cent, cd);
		if (cd->moved > 0) {
		    sprintf(res,"<b>tilt:</b>%d",value);
                } else {
		   /*error in track action*/
                    sprintf(res, "tilt:error");
                }
	} else if (!strcmp(command,"reset")) {
		/* reset */
		struct coord cent;
		cent.y = 0;
		cent.x = 0;
		cd->moved = uvc_reset(cd, value);
		if (cd->moved) {
		    sprintf(res,"<b>reset:</b>%d",value);
                } else {
		/*error in track action*/
                    sprintf(res, "reset:error");
                }
        }
        send_template(client_socket, res);
        send_template_end_client(client_socket);            
     } else if (ssconfret == 1){
	if (!strcmp(command,"control")) {
          send_control_template(client_socket,click_control,cd->control_port,cd->stream_port);
          send_template_end_client(client_socket);
        } else {
          send_template_ini_client(client_socket,unknown_command);
          send_template_end_client(client_socket);
        }
          
     } else {
	send_template_ini_client(client_socket,unknown_command);
        send_template_end_client(client_socket);
     }  
  }
  return 1;
}


/*
 -TODO-
 As usually web clients uses nonblocking connect/read
 read_client should handle nonblocking sockets.
*/

static int read_client(int client_socket, struct control_data *cd)
{
	int alive = 1;
	int ret = 1;
	char buffer[1024] = {'\0'};
	int length = 1024;
	//struct context **cnt = userdata;

	/* lock the mutex */
	pthread_mutex_lock(&httpd_mutex);

	while (alive)
	{
		int nread = 0, readb = -1;

		nread = read (client_socket, buffer, length);

		if (nread <= 0) {
			pthread_mutex_unlock(&httpd_mutex);
			return -1;
		}
		else {
			char method[sizeof (buffer)];
			char url[sizeof (buffer)];
			char protocol[sizeof (buffer)];

			buffer[nread] = '\0';

			warningkill = sscanf (buffer, "%s %s %s", method, url, protocol);

			while ((strstr (buffer, "\r\n\r\n") == NULL) && (readb!=0) && (nread < length)){
				readb = read (client_socket, buffer+nread, sizeof (buffer) - nread);

				if (readb == -1){
					nread = -1;
					break;
				}

				nread +=readb;
				
				if (nread > length) {
					break;
				}
				buffer[nread] = '\0';
			}

			/* Make sure the last read didn't fail.  If it did, there's a
			problem with the connection, so give up.  */
			if (nread == -1) {
				//motion_log(LOG_ERR, 1, "httpd READ");
				pthread_mutex_unlock(&httpd_mutex);
				return -1;
			}
			alive = 0;

			/* Check Protocol */
			if (strcmp (protocol, "HTTP/1.0") && strcmp (protocol, "HTTP/1.1")) {
				/* We don't understand this protocol.  Report a bad response.  */
				warningkill = write (client_socket, bad_request_response, sizeof (bad_request_response));
				pthread_mutex_unlock(&httpd_mutex);
				return -1;
			}
			else if (strcmp (method, "GET")) {
				/* This server only implements the GET method.  If client
				uses other method, report the failure.  */
				char response[1024];
				snprintf (response, sizeof (response),bad_method_response_template, method);
				warningkill = write (client_socket, response, strlen (response));
				pthread_mutex_unlock(&httpd_mutex);
				return -1;
			} 
			else {
				ret=handle_get (client_socket, url, cd);				
				/* A valid request.  Process it.  */
			}
		}
	}
	pthread_mutex_unlock(&httpd_mutex);

	return ret;
}




/*
   acceptnonblocking
   
   This function waits timeout seconds for listen socket.
   Returns :
   	-1 if the timeout expires or on accept error.
	curfd (client socket) on accept success.
*/

static int acceptnonblocking(int serverfd, int timeout)
{
	int curfd;
	socklen_t namelen = sizeof(struct sockaddr_in);
	struct sockaddr_in client;
	struct timeval tm;
	fd_set fds;

	tm.tv_sec = timeout; /* Timeout in seconds */
	tm.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(serverfd,&fds);
	if( select (serverfd + 1, &fds, NULL, NULL, &tm) > 0){
		if(FD_ISSET(serverfd, &fds)) { 
			if((curfd = accept(serverfd, (struct sockaddr*)&client, &namelen))>0){
				return(curfd);
			}	
		}	
	}
	return -1;
} 




/* #########################################################################
uvcstream_control
######################################################################### */
void control(struct control_data *cd)
{
  struct sockaddr_in addr;
  int sd, on, client_sent_quit_message = 1;
  int client_socket_fd;
  struct sigaction act;
  
  /* set signal handlers TO IGNORE */
  memset(&act,0,sizeof(act));
  sigemptyset(&act.sa_mask);
  act.sa_handler = SIG_IGN;
  sigaction(SIGPIPE,&act,NULL);
  sigaction(SIGCHLD,&act,NULL);


  /* open socket for server */
  sd = socket(PF_INET, SOCK_STREAM, 0);
  if ( sd < 0 ) {
    fprintf(stderr, "control socket failed\n");
    exit(1);
  }

  /* ignore "socket already in use" errors */
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    perror("setsockopt(SO_REUSEADDR) failed");
    //exit(1);
    close(sd);
    return;
  }

  /* configure server address to listen to all local IPs */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = cd->control_port;
  addr.sin_addr.s_addr = INADDR_ANY;
  if ( bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 ) {
    fprintf(stderr, "control port bind failed\n");
    //exit(1);
    close(sd);
    return;
  } 

  /* start listening on socket */
  if ( listen(sd, 5) == -1 ) {
    fprintf(stderr, "control port listen failed\n");
    close(sd);
    return;
  }

  while ((client_sent_quit_message!=0)) { 
		client_socket_fd = acceptnonblocking(sd, 1);
		if (client_socket_fd>0) {
			/* Get the Client request */
			client_sent_quit_message = read_client (client_socket_fd, cd);
			/* Close Connection */
			if (client_socket_fd){
				close(client_socket_fd);
			}	
		} 
    }
    close(sd);
    pthread_mutex_destroy(&httpd_mutex);
}

void *uvcstream_control(void *arg)
{	
	struct control_data *cd=arg;
	control(cd);
	pthread_exit(NULL);
}
          
