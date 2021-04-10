mjpg-streamer input plugin: input_http
======================================

This plugin provides JPEG data from MJPEG compatible webcams.

Usage
=====

    mjpg_streamer -i 'input_http.so [options]'
    
```
 ---------------------------------------------------------------
 Help for input plugin..: HTTP Input plugin
 ---------------------------------------------------------------
 The following parameters can be passed to this plugin:

 [-v | --version ]........: current SVN Revision
 [-h | --help]............: show this message
 [-H | --host]............: host, defaults to localhost
 [-p | --port]............: port, defaults to 8080
 [-r | --request].........: request, defaults to /?action=stream
 [-c | --credentials].....: credentials, defaults to NULL
 [-b | --boundary]........: boundary, defaults to --boundarydonotcross
 ---------------------------------------------------------------
```

Originally this plugin only supported other mjpg-streamer instances. It becomes inherently more useful if you can stream any
MJPEG stream. In my case some Annke and Hikvision cameras only allow a single connection to the MJPEG substream. Default behavior remains the same if no parameters are passed.

Basic Authentication is handled via credentials parameter as user:password. Please be aware of the security implications of passing credentials clear text (albeit Base64) over HTTP.

To find boundary string use (modify URL to match your stream source):

```
curl "http://user:pass@192.168.1.69/ISAPI/Streaming/channels/102/httpPreview" -m 1 > out.mjpg
grep -a "\-\-" out.mjpg

--boundarySample
--boundarySample
--boundarySample
```
