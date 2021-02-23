
input_event: a plugin for mjpg-streamer
=======================================

This is an event player intended to play back events recorded
by output_modect.

It is controlled by the following commands through the control
interface (output_http required to be running)

play 0=pause 1=play
step -1=back +1=forward
speed -1=slower +1=faster
frame n jump to frame n
stop 1 quit the program 

You really need an app to use this.
It was written as part of the Mitey Minder system.

 Help for input plugin..: input_event
 ---------------------------------------------------------------
 [-d | --directory ].......: directory with sequential jpegs to play back

NB. input_event relies on the file modification time of the jpegs for playback timimg.
    If you manipulate the event jpegs in any way (eg copy to another place), make
    sure to do so in such a way as to preserve the mod time.


