#include "LibCamera.h"

using namespace std::placeholders;

int LibCamera::initCamera(int *width, int *height, int *stride, PixelFormat format, int buffercount, int rotation) {
    int ret;
    cm = std::make_unique<CameraManager>();
    ret = cm->start();
    if (ret){
        std::cout << "Failed to start camera manager: "
              << ret << std::endl;
        return ret;
    }
    cameraId = cm->cameras()[0]->id();
    camera_ = cm->get(cameraId);
    if (!camera_) {
        std::cerr << "Camera " << cameraId << " not found" << std::endl;
        return 1;
    }

    if (camera_->acquire()) {
        std::cerr << "Failed to acquire camera " << cameraId
              << std::endl;
        return 1;
    }
    camera_acquired_ = true;

    std::unique_ptr<CameraConfiguration> config;
    config = camera_->generateConfiguration({ StreamRole::Viewfinder });
    libcamera::Size size(*width, *height);
    config->at(0).pixelFormat = format;
    config->at(0).size = size;
    if (buffercount)
        config->at(0).bufferCount = buffercount;
    Transform transform = Transform::Identity;
    bool ok;
    Transform rot = transformFromRotation(rotation, &ok);
    if (!ok)
        throw std::runtime_error("illegal rotation value, Please use 0 or 180");
    transform = rot * transform;
    if (!!(transform & Transform::Transpose))
        throw std::runtime_error("transforms requiring transpose not supported");
    config->transform = transform;

    switch (config->validate()) {
        case CameraConfiguration::Valid:
            break;

        case CameraConfiguration::Adjusted:
            std::cout << "Camera configuration adjusted" << std::endl;
            break;

        case CameraConfiguration::Invalid:
            std::cout << "Camera configuration invalid" << std::endl;
            return 1;
    }
    *width = config->at(0).size.width;
    *height = config->at(0).size.height;
    *stride = config->at(0).stride;
    config_ = std::move(config);
    return 0;
}

char * LibCamera::getCameraId(){
    return cameraId.data();
}

int LibCamera::startCamera() {
    int ret;
    ret = camera_->configure(config_.get());
    if (ret < 0) {
        std::cout << "Failed to configure camera" << std::endl;
        return ret;
    }

    camera_->requestCompleted.connect(this, &LibCamera::requestComplete);

    allocator_ = std::make_unique<FrameBufferAllocator>(camera_);

    return startCapture();
}

int LibCamera::startCapture() {
    int ret;
    unsigned int nbuffers = UINT_MAX;
    for (StreamConfiguration &cfg : *config_) {
        ret = allocator_->allocate(cfg.stream());
        if (ret < 0) {
            std::cerr << "Can't allocate buffers" << std::endl;
            return -ENOMEM;
        }

        unsigned int allocated = allocator_->buffers(cfg.stream()).size();
        nbuffers = std::min(nbuffers, allocated);
    }

    for (unsigned int i = 0; i < nbuffers; i++) {
        std::unique_ptr<Request> request = camera_->createRequest();
        if (!request) {
            std::cerr << "Can't create request" << std::endl;
            return -ENOMEM;
        }

        for (StreamConfiguration &cfg : *config_) {
            Stream *stream = cfg.stream();
            const std::vector<std::unique_ptr<FrameBuffer>> &buffers =
                allocator_->buffers(stream);
            const std::unique_ptr<FrameBuffer> &buffer = buffers[i];

            ret = request->addBuffer(stream, buffer.get());
            if (ret < 0) {
                std::cerr << "Can't set buffer for request"
                      << std::endl;
                return ret;
            }
            for (const FrameBuffer::Plane &plane : buffer->planes()) {
                void *memory = mmap(NULL, plane.length, PROT_READ, MAP_SHARED,
                            plane.fd.get(), 0);
                mappedBuffers_[plane.fd.get()] =
                    std::make_pair(memory, plane.length);
            }
        }

        requests_.push_back(std::move(request));
    }

    ret = camera_->start(&this->controls_);
    // ret = camera_->start();
    if (ret) {
        std::cout << "Failed to start capture" << std::endl;
        return ret;
    }
    controls_.clear();
    camera_started_ = true;
    for (std::unique_ptr<Request> &request : requests_) {
        ret = queueRequest(request.get());
        if (ret < 0) {
            std::cerr << "Can't queue request" << std::endl;
            camera_->stop();
            return ret;
        }
    }
    viewfinder_stream_ = config_->at(0).stream();
    return 0;
}

void LibCamera::StreamDimensions(Stream const *stream, uint32_t *w, uint32_t *h, uint32_t *stride) const
{
	StreamConfiguration const &cfg = stream->configuration();
	if (w)
		*w = cfg.size.width;
	if (h)
		*h = cfg.size.height;
	if (stride)
		*stride = cfg.stride;
}

libcamera::Stream *LibCamera::VideoStream(uint32_t *w, uint32_t *h, uint32_t *stride) const
{
	StreamDimensions(viewfinder_stream_, w, h, stride);
	return viewfinder_stream_;
}

int LibCamera::queueRequest(Request *request) {
    std::lock_guard<std::mutex> stop_lock(camera_stop_mutex_);
    if (!camera_started_)
        return -1;
    {
        std::lock_guard<std::mutex> lock(control_mutex_);
        request->controls() = std::move(controls_);
    }
    return camera_->queueRequest(request);
}

void LibCamera::requestComplete(Request *request) {
    if (request->status() == Request::RequestCancelled)
        return;
    processRequest(request);
}

void LibCamera::processRequest(Request *request) {
    std::lock_guard<std::mutex> lock(free_requests_mutex_);
    CompletedRequest payload(request->buffers().begin()->second->metadata().sequence, request->buffers(),
							 request->metadata());
    requestQueue.push(request);
}

void LibCamera::returnFrameBuffer(LibcameraOutData frameData) {
    uint64_t request = frameData.request;
    Request * req = (Request *)request;
    req->reuse(Request::ReuseBuffers);
    queueRequest(req);
}

bool LibCamera::readFrame(LibcameraOutData *frameData){
    std::lock_guard<std::mutex> lock(free_requests_mutex_);
    // int w, h, stride;
    if (!requestQueue.empty()){
        Request *request = this->requestQueue.front();

        const Request::BufferMap &buffers = request->buffers();
        for (auto it = buffers.begin(); it != buffers.end(); ++it) {
            FrameBuffer *buffer = it->second;
            for (unsigned int i = 0; i < buffer->planes().size(); ++i) {
                const FrameBuffer::Plane &plane = buffer->planes()[i];
                const FrameMetadata::Plane &meta = buffer->metadata().planes()[i];
                
                void *data = mappedBuffers_[plane.fd.get()].first;
                int length = std::min(meta.bytesused, plane.length);

                frameData->size = length;
                frameData->imageData = (uint8_t *)data;
            }
        }
        this->requestQueue.pop();
        frameData->request = (uint64_t)request;
        return true;
    } else {
        Request *request = nullptr;
        frameData->request = (uint64_t)request;
        return false;
    }
}

void LibCamera::set(ControlList controls){
	this->controls_ = std::move(controls);
}

void LibCamera::stopCamera() {
    if (camera_){
        {
            std::lock_guard<std::mutex> lock(camera_stop_mutex_);
            if (camera_started_){
                if (camera_->stop())
                    throw std::runtime_error("failed to stop camera");
                camera_started_ = false;
            }
        }
        if (camera_started_){
            if (camera_->stop())
                throw std::runtime_error("failed to stop camera");
            camera_started_ = false;
        }
        camera_->requestCompleted.disconnect(this, &LibCamera::requestComplete);
    }
    while (!requestQueue.empty())
        requestQueue.pop();

    requests_.clear();

    allocator_.reset();

    controls_.clear();
}

void LibCamera::closeCamera(){
    if (camera_acquired_)
        camera_->release();
    camera_acquired_ = false;

    camera_.reset();

    cm.reset();
}
