
input_httpout: An input plugin for mjpg_streamer
================================================

This is a drop-in replacement for input_http when reading from http_out

input_httpout has the following features:

   - uses the fact that output_http supplies the frame size,
     so no need to search for boundaries
   - uses roughly 1/2 the cpu cycles as input_http
   - passes the timestamp on to the output plugin(s)
   - finds every line in the headers, so may be
     possible to generalize by someone who
     knows http protocol.

input_httpout takes the same parameters as input_http:

 ---------------------------------------------------------------
 Help for input plugin..: input_httpout
 ---------------------------------------------------------------
 [-H | --host ].....................: output_http host (default: localhost)
 [-p | --port ].....................: output_http port

