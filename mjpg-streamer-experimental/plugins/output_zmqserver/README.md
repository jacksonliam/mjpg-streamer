# mjpg-streamer output plugin: output_zmqserver

This plugin streams out the video data via
[ZeroMQ](http://zeromq.org/) serialized as
[Protobuf](https://developers.google.com/protocol-buffers/) data.

Please take a look at the protobuf definition for more details about the message format:
[package.proto](package.proto)


You must have libzmq-dev and libprobot-c0-dev installed (or similar)
in order for this plugin to be compiled & installed.

On debian you need following libraries:

```bash
sudo apt-get install libzmq4-dev libprotobuf-dev libprotobuf-c0-dev protobuf-c-compiler
```

## Usage

```bash
mjpg_streamer [input plugin options] -o 'output_zmqserver.so --address [zmq-uri] --buffer_size [output ring buffer size]'
```


## Examples

The plugin was created for [Machinekit](http://machinekit.io) and
[QtQuickVcp](https://github.com/machinekit/qtquickvcp).

You can find the Qt/QML counterpart here:
[videoview](https://github.com/machinekit/QtQuickVcp/tree/master/src/videoview)

Additionally, Machinekit contains a tool that wraps mjpg-streamer and
the plugin invokation:
[videoserver](https://github.com/machinekit/machinekit/tree/master/src/machinetalk/videoserver)
