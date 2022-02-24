#include <atomic>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <limits.h>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <sstream>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <mutex>
#include <variant>
#include <condition_variable>

#include <libcamera/controls.h>
#include <libcamera/control_ids.h>
#include <libcamera/property_ids.h>
#include <libcamera/libcamera.h>
#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>
#include <libcamera/formats.h>
#include <libcamera/transform.h>

using namespace libcamera;

typedef struct {
    uint8_t *imageData;
    int size;
    uint64_t request;
    uint32_t width;
    uint32_t height;
} LibcameraOutData;

struct CompletedRequest
{
	using BufferMap = libcamera::Request::BufferMap;
	using ControlList = libcamera::ControlList;
	CompletedRequest() {}
	CompletedRequest(unsigned int seq, BufferMap const &b, ControlList const &m)
		: sequence(seq), buffers(b), metadata(m)
	{
	}
	unsigned int sequence;
	BufferMap buffers;
	ControlList metadata;
	float framerate;
};

class LibCamera {

    struct QuitPayload
	{
	};
    enum class MsgType
	{
		RequestComplete,
		Quit
	};
	typedef std::variant<CompletedRequest, QuitPayload> MsgPayload;
	struct Msg
	{
		Msg(MsgType const &t, MsgPayload const &p) : type(t), payload(p) {}
		MsgType type;
		MsgPayload payload;
	};

    public:
        LibCamera(){};
        ~LibCamera(){};
        
        int initCamera(int *width, int *height, PixelFormat format, int buffercount, int rotation);
        
        int startCamera();
        bool readFrame(LibcameraOutData *frameData);
        void returnFrameBuffer(LibcameraOutData frameData);

        void set(ControlList controls);
        void stopCamera();
        void closeCamera();

        libcamera::Stream *VideoStream(int *w, int *h, int *stride) const;
        char * getCameraId();

    private:

        int startCapture();
        int queueRequest(Request *request);
        void requestComplete(Request *request);
        void processRequest(Request *request);
        void StreamDimensions(Stream const *stream, int *w, int *h, int *stride) const;

        unsigned int cameraIndex_;
	    uint64_t last_;
        std::unique_ptr<CameraManager> cm;
        std::shared_ptr<Camera> camera_;
        bool camera_acquired_ = false;
        bool camera_started_ = false;
	    std::unique_ptr<CameraConfiguration> config_;
        std::unique_ptr<FrameBufferAllocator> allocator_;
        std::vector<std::unique_ptr<Request>> requests_;
        // std::map<std::string, Stream *> stream_;
        std::map<int, std::pair<void *, unsigned int>> mappedBuffers_;

        std::queue<Request *> requestQueue;

        ControlList controls_;
        std::mutex control_mutex_;
        std::mutex camera_stop_mutex_;
        std::mutex free_requests_mutex_;

        Stream *viewfinder_stream_ = nullptr;
        std::string cameraId;
};