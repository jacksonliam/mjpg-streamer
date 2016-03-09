/**
    OpenCV filter plugin for mjpg-streamer that loads a python script and calls
    it directly. The python script must... TODO
    
    At the moment, only the input_opencv.so plugin supports filter plugins.
*/

#include <libgen.h>
#include <string.h>

#include "opencv2/opencv.hpp"
#include <Python.h>
#include "conversion.h"

using namespace cv;
using namespace std;

// exports for the filter
extern "C" {
    bool filter_init(const char * args, void** filter_ctx);
    Mat filter_init_frame(void* filter_ctx);
    void filter_process(void* filter_ctx, Mat &src, Mat &dst);
    void filter_free(void* filter_ctx);
}

static int python_loaded = 0;

struct Context {
    NDArrayConverter converter;
    
    PyObject *pModule;
    PyObject *filter_fn;
    PyObject *lastRetval;
    
    PyThreadState *pMainThread;
};


// exists because dirname modifies its args
static PyObject* get_dirname(const char * args) {
    char * dupargs = strdup(args);
    PyObject* obj = PyUnicode_DecodeFSDefault(dirname(dupargs));
    free(dupargs);
    return obj;
}

// exists because basename modifies its args
static PyObject* get_import(const char * args) {
    char * dupargs = strdup(args);
    char * base = basename(dupargs);
    char * ext = strrchr(base, '.');
    if (ext)
        *ext = '\0';
    PyObject* obj = PyUnicode_DecodeFSDefault(base);
    free(dupargs);
    return obj;
}

/**
    Initializes the filter. If you return something, it will be passed to the
    filter_process function, and should be freed by the filter_free function
*/
bool filter_init(const char * args, void** filter_ctx) {
    
    PyObject *sys, *sys_path = NULL;
    PyObject *pModuleDir, *pModuleName, *pFunc;
    Context * ctx;
    char * dupargs, *mod_name, *mod_dir;
    
    if (strlen(args) < 3) {
        fprintf(stderr, "Need to specify python filter module via --fargs\n");
        return false;
    }
    
    // don't initialize python more than once
    if (python_loaded == 0) {
        Py_Initialize();
        PyEval_InitThreads();
    }
    
    ctx = new Context();
    *filter_ctx = ctx;
    
    python_loaded += 1;
    
    if (!NDArrayConverter::init_numpy()) {
        fprintf(stderr, "Error loading numpy!\n");
        return false;
    }
    
    // import the module by path
    // -> add the module's path to sys.path, in case it decides to import
    //    something, and import it directly
    
    pModuleDir = get_dirname(args);
    pModuleName = get_import(args);
    
    sys = PyImport_ImportModule("sys");
    if (sys != NULL) {
        sys_path = PyObject_GetAttrString(sys, "path");
    }
    
    if (sys_path != NULL && pModuleDir != NULL && pModuleName != NULL) {            
        if (PyList_Insert(sys_path, 0, pModuleDir) == 0) {
            ctx->pModule = PyImport_Import(pModuleName);
        }
    }
    
    Py_XDECREF(pModuleName);
    Py_XDECREF(pModuleDir);
    Py_XDECREF(sys_path);
    Py_XDECREF(sys);
    
    if (ctx->pModule == NULL) {
        PyErr_Print();
        fprintf(stderr, "Failed to load \"%s\"\n", args);
        return false;
    }
    
    // load the initialize function, call it
    pFunc = PyObject_GetAttrString(ctx->pModule, "init_filter");
    if (pFunc == NULL || !PyCallable_Check(pFunc)) {
        if (PyErr_Occurred())
            PyErr_Print();
        
        fprintf(stderr, "Could not load init_filter function: %p\n", pFunc);
        return false;
    }
    
    // load the processing function
    ctx->filter_fn = PyObject_CallObject(pFunc, NULL);
    Py_DECREF(pFunc);
    
    if (ctx->filter_fn == NULL) {
        PyErr_Print();
        return false;
    }
    
    if (!PyCallable_Check(ctx->filter_fn)) {
        fprintf(stderr, "init_filter did not return a callable object\n");
        return false;
    }
    
    // done with initialization, let go of the GIL
    ctx->pMainThread = PyEval_SaveThread();
    return true;
}


void dbgMat(const char * wat, Mat &m) {
    fprintf(stderr, "%s: ref %d, alloc @ %p; ptr %p %p\n", wat, m.u ? m.u->refcount : 0, m.allocator,
            m.u ? m.u->data : NULL, m.u ? m.u->origdata : NULL);
}


Mat filter_init_frame(void *filter_ctx) {
    Context *ctx = (Context*)filter_ctx;
    
    // this function ensures that the initial mat is using the numpy allocator,
    // which avoids copies each time the source image is captured
    Mat mat;
    PyObject *mm = ctx->converter.toNDArray(mat);
    ctx->converter.toMat(mm, mat);
    Py_DECREF(mm);
    return mat;
}

/**
    Called by the OpenCV plugin upon each frame
*/
void filter_process(void* filter_ctx, Mat &src, Mat &dst) {
    
    Context *ctx = (Context*)filter_ctx;
    PyObject *ndArray, *pArgs;
    
    PyGILState_STATE gil_state = PyGILState_Ensure();
    
    ndArray = ctx->converter.toNDArray(src);
    if (ndArray == NULL) {
        PyErr_Print();
        PyGILState_Release(gil_state);
        dst = src;
        return;
    }
        
    pArgs = PyTuple_New(1);
    PyTuple_SetItem(pArgs, 0, ndArray); // takes ownership of ndarray
    
    // see below for rationale
    Py_XDECREF(ctx->lastRetval);
    
    // call the python function, store the object until next time, that way the
    // underlying Mat object doesn't get deallocated
    ctx->lastRetval = PyObject_CallObject(ctx->filter_fn, pArgs);
    if (ctx->lastRetval == NULL) {
        PyErr_Print();
        dst = src;
    } else {
        if (ctx->lastRetval != Py_None) {
            if (!ctx->converter.toMat(ctx->lastRetval, dst)) {
                PyErr_Print();
                dst = src;
            }
        }    
    }
    
    Py_DECREF(pArgs);
    
    // done with GIL
    PyGILState_Release(gil_state);
}


/**
    Called when the input plugin is cleaning up (will get called during
    initialization if initialization fails).
*/
void filter_free(void* filter_ctx) {
    
    Context * ctx = (Context*)filter_ctx;
    
    if (ctx->pMainThread != NULL) {
        PyEval_RestoreThread(ctx->pMainThread);
    }
    
    Py_XDECREF(ctx->lastRetval);
    Py_XDECREF(ctx->filter_fn);
    Py_XDECREF(ctx->pModule);
    
    delete ctx;
    
    python_loaded -= 1;
    
    if (python_loaded == 0) {
        // TODO: weird threading KeyError... probably because this is not called
        //       from the same thread as filter_init
        Py_Finalize();
    }
}


