#ifndef __HVA_DETECTION_HELPER__
#define __HVA_DETECTION_HELPER__

struct DetectedObject_t {
    int x;
    int y;
    int width;
    int height;
    float confidence;
    int labelId;
    explicit DetectedObject_t(float x, float y, float h, float w, float confidence, int id = -1, float h_scale = 1.f,
                            float w_scale = 1.f)
        : x(static_cast<int>((x - w / 2) * w_scale)), y(static_cast<int>((y - h / 2) * h_scale)),
        width(static_cast<int>(w * w_scale)), height(static_cast<int>(h * h_scale)), confidence(confidence) {
    }
    DetectedObject_t() = default;
    ~DetectedObject_t() = default;
    DetectedObject_t(const DetectedObject_t &) = default;
    DetectedObject_t(DetectedObject_t &&) = default;
    DetectedObject_t &operator=(const DetectedObject_t &) = default;
    DetectedObject_t &operator=(DetectedObject_t &&) = default;
    bool operator<(const DetectedObject_t &other) const {
        return this->confidence > other.confidence; //TODO fix me
    }
};

// struct InfoROI_t {
//     int widthImage = 0;
//     int heightImage = 0;
//     int x = 0;
//     int y = 0;
//     int width = 0;
//     int height = 0;
//     int indexROI = 0;
//     int totalROINum = 0;
//     int frameId = 0;

//     int idx = 0;
//     float confidence = 0.0f;
//     std::string label = "";
// };

#endif //#ifndef __HVA_DETECTION_HELPER__