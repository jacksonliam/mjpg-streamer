input_opencv filter plugin: cvfilter_py
=======================================

This plugin allows you to use a Python 3.x script to process images received by
mjpg-streamer. This has been tested with Python 3.4.

To run a Python 3.x script, you can do the following:

    mjpg_streamer -i "input_opencv.so --filter cvfilter_py.so --fargs path/to/filter.py"

Filter script
-------------

Your script MUST define a function called 'init_filter', that takes zero
arguments and returns a single callable. This returned callable must take
a single argument (a numpy array), and returns a single object (a numpy array).
A simple example follows:

```

def filter_fn(img):
    '''
        :param img: A numpy array representing the input image
        :returns: A numpy array to send to the mjpg-streamer output plugin
    '''
    return img
    
def init_filter():
    return filter_fn

```

For a more complex example, see the included example_filter.py

Known Issues
------------

When mjpg-streamer is terminated, a `KeyError` is raised in the threading
module. While annoying, it's harmless. Most likely it happens because the
python interpreter is destroyed on the wrong thread.

TODO
----

After going through all of this effort to create the code for this module, I
bet it can be done a lot simpler via cython.

Authors
-------

Dustin Spicuzza (dustin@virtualroadside.com)
