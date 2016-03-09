
import cv2
import numpy as np

class MyFilter:
    
    def process(self, img):
        '''
            :param img: A numpy array representing the input image
            :returns: A numpy array to send to the mjpg-streamer output plugin
        '''
        
        # silly routine that overlays a really large crosshair over the image
        h = img.shape[0]
        w = img.shape[1]
        
        w2 = int(w/2)
        h2 = int(h/2)
        
        cv2.line(img, (int(w/4), h2), (int(3*(w/4)), h2), (0xff, 0, 0), thickness=3)
        cv2.line(img, (w2, int(h/4)), (w2, int(3*(h/4))), (0xff, 0, 0), thickness=3)
        
        return img
        
def init_filter():
    '''
        This function is called after the filter module is imported. It MUST
        return a callable object (such as a function or bound method). 
    '''
    f = MyFilter()
    return f.process

