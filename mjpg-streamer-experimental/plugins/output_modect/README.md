
output_modect: A motion detection plugin for mjpg_streamer
==========================================================

   - Saves frames in groups of events
   - an event is the time motion is active plus linger time
   - each event has its own subdirectory in the given save path
   - event directories are named with the event number, date and time
   - optionally limit the disk space to be used for events
   - optionally recycle storage space by removing the oldest events
   - optionally embed the timestamp on saved frames
   - optionally run a script on motion transitions
   - optionally output motion status on a port

   - requires libturbojpeg to be installed

---------------------------------------------------------------
 Help for output plugin..: output_modect
 ---------------------------------------------------------------
 The following parameters can be passed to this plugin (defaults):

---------------- General ----------------------------------------
 [-g | --debug ] .....................: print info on stderr (off)

---------------- Motion Detection--------------------------------
 [-p | --pixel_threshold ] %d.........: diff between compared pixels to declare an alarm pixel (30)
                                         lower = more sensitive
 [-a | --alarm_pixels ] %d............: number of alarmed pixels required to declare motion (60)
                                         lower = more sensitive
 [-j | --jpeg_scale ] %d..............: scale to use for internal jpg decode (1-8) (3)
                                         higher = more sensitive
 [-l | --linger ] %d..................: time to wait before turning off motion (ms) (3000)

---------------- Output------------------------------------------
 [-s | --save_events ] ...............: save motion events to disk (off)
 [-o | --output_directory ] %s........: path for event saving (NULL)
 [-m | --mb_storage_alloc ] %d........: disk alloc for events in Mb (0=no limit) (0Mb)
 [-r | --recycle ] %d.................: recycle when storage allocation full (0)
                                         0 = off else % storage to delete (oldest events removed)
 [-t | --timestamp %d.................: embed date & time on saved frames 0=off, 1-4=font size (0)
 [-n | --name %s......................: id for output functions (see notes for default)
 [-e | --exec ] %s....................: path/to/script to execute on motion on/off (NULL)
 [-sp| --status_port ] %d.............: output motion status on this port (0)
 ------------------------------------------------------------------

name defaults to
   if input plugin is input_uvc: the device given on the input_uvc command line
   else if input plugin is input_http or input_httpout: "http:$host:$port"
   else the name of the input plugin

The following parameters can be viewed/changed dynamically via the control interface
provided that you also have output_http running: (script control.sh provided)

   debug: 0 = off, 1 = on
   modect: 0 = turn motion detection off, 1 = turn it on
   motion: 0 = force off, 1 = force on; (also turns modect off)
   pixel_threshold
   alarm_pixels
   jpeg_scale
   linger
   save_events: 0=off 1=on
   mb_storage_alloc
   recycle
   timestamp

