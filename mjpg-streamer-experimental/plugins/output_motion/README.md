mjpg-streamer output plugin: output_motion
==========================================

This is a motion detection plugin that will call helper script on motion started/stopped event.

Requires development JPEG library.

Usage
=====

    mjpg_streamer [input plugin options] -o 'output_motion.so --foreground-diff-threshold 35 --background-learning-rate 0.01 --motion-noise-threshold 0.002'

The helper script `action.sh` should be accessible from working directory. E.g.,

    case $1 in

      # alarm_status motion_probability fps
      alarm)
        echo "ALARM $2" >&2
        # TODO: place notification code here
      ;;

      # fname scene_change fps
      record) # NB: mjpeg stream as stdin
        echo "ALARM RECORD SCENE_CHANGE=$3 FPS=$4" >&2
        # TODO: place recording code here
      ;;

    esac
