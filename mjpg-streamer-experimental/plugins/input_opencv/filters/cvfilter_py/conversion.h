# ifndef __COVERSION_OPENCV_H__
# define __COVERSION_OPENCV_H__

#include <Python.h>
#include <opencv2/core/core.hpp>

class NDArrayConverter {
public:
    static bool init_numpy();
    
    bool toMat(PyObject* o, cv::Mat &m);
    PyObject* toNDArray(const cv::Mat& mat);
};

# endif
