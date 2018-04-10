
#CAM=camX
#STREAM='http://127.0.0.1:38091/?action=stream'
#ROTATE=90
#SCENE_CHANGE=0.003
TILE=3x3
SCENE_CHANGE2=0.006
TILE2=6x6
ROOT=/myhome/VIDEO/$CAM

STORAGE='https://xxx.yyy.zzz/video'

pub='mosquitto_pub'

case $1 in

  # alarm_status motion_probability fps
  alarm)
    $pub -t state/alarm/camera/$CAM -m $2 -r
    #$pub -t report/text/$CAM -m $2
    $pub -t kodi/room/notify/warning/4000 -m "$CAM|$2"
    echo "$CAM $2" >&2
  ;;

  # fname scene_change fps
  record) # NB: mjpeg stream as stdin
    echo "$CAM RECORD SCENE_CHANGE=$3 FPS=$4" >&2
    # ensure writable directory
    DIR="$ROOT/$2"
    mkdir -p $DIR
    # start audio recorder
    if test -n "$PA_SOURCE"; then
      nice -n 16 \
        ffmpeg -y -loglevel fatal -strict experimental -threads 1 \
          -f pulse -channel_layout stereo -server "$PA_SERVER" -i "$PA_SOURCE" \
          -ac 1 -f mp3 -t 180 $DIR.mp3 \
      &
      AUDIO_WRITER_PID=$!
    fi
    # start preview reporter
    (
      while test ! -r $DIR/preview-001.jpg; do sleep 1; done
      $pub -t "report/image/$CAM/keyboard/@live=${STREAM}" -f $DIR/preview-001.jpg
      rm -f $DIR/preview-001.jpg
    ) &
    # start video recorder
    case "$ROTATE" in
       90) ROTATE=transpose=1 ;;
      180) ROTATE=transpose=2,transpose=2 ;;
      270) ROTATE=transpose=3 ;;
        *) ROTATE=null ;;
    esac
    CAPTION="drawbox=color=black@0.5:width=iw:height=45:t=max,drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf:fontsize=20:fontcolor=white:x=(w-tw)/2:y=lh:text='${CAM} %{localtime\:%Y-%m-%d %T}'"
    nice -n 16 \
      ffmpeg -y -loglevel fatal -strict experimental -threads 1 \
        -f mjpeg -i - -an \
        -filter_complex "[0:v:0]${ROTATE}[rotated];[rotated]${CAPTION}[caption];[caption]setpts=N/(20*TB),split=3[v1][v2][v3];[v2]select=gt(scene\,${3}),tile=${TILE}:padding=5:color=white[preview];[v3]select=gt(scene\,${SCENE_CHANGE2}),tile=${TILE2}:padding=5:color=white[digest]" \
        -map '[v1]' -vsync vfr -frames:v 600 -c:v libx264 -crf 18 -r 20 -pix_fmt yuv420p -preset veryfast -profile:v baseline -f mp4 $DIR.mp4 \
        -map '[preview]' -vsync vfr -q:v 1 -frames:v 2 -f image2 "$DIR/preview-%03d.jpg" \
        -map '[digest]' -vsync vfr -q:v 1 -frames:v 2 -f image2 "$DIR/tile-%03d.jpg"
    # video recorded
    # stop audio recorder
    if test -n "$PA_SOURCE"; then
      kill $AUDIO_WRITER_PID
    fi
    # report records
    cp $DIR/tile-001.jpg $DIR.jpg
    $pub -t "report/image/$CAM/keyboard/@video=${STORAGE}$DIR.mp4|audio=${STORAGE}$DIR.mp3|image=${STORAGE}$DIR.jpg|live=${STREAM}" -f $DIR.jpg
    rsync -a -R -e 'ssh -- copy' $DIR.jpg :Videos && rm -f $DIR.jpg
    rsync -a -R -e 'ssh -- copy' $DIR.mp4 :Videos && rm -f $DIR.mp4
    rsync -a -R -e 'ssh -- copy' $DIR.mp3 :Videos && rm -f $DIR.mp3
    rm -f $DIR/preview-*.jpg
    rm -f $DIR/tile-*.jpg
    rmdir $DIR || true # NB: fails if something now pushed online
  ;;

esac
