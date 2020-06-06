# Script files for mjpg-streamer.

Assumes mjpg-streamer is installed at /usr/local/bin/mjpg_streamer and
that there is a user account `webcam` for running these scripts.

If you have a different setup you'll need to adjust
mjpg-streamer.default (init) or mjpg-streamer.service (systemd)
accordingly.

The `webcam` user will need access to /dev/video*. To add this run:

```sh
sudo adduser webcam video
sudo usermod -a -G video webcam
```

## init

```
mjpg-streamer.default   => /etc/default/mjpg-streamer
mjpg-streamer.init      => /etc/init.d/mjpg-streamer
```

After copying the above files run:

```sh
sudo update-rc.d mjpg-streamer defaults
sudo systemctl daemon-reload
```

## systemd

```
mjpg-streamer.service   => /etc/systemd/system/mjpg-streamer.service
```
