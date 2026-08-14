#ifndef FACE_H
#define FACE_H

#include <stdint.h>
#include <string.h>


enum EyeShape : uint8_t {
    EYE_RECT,
    EYE_ARC
};


enum MouthShape : uint8_t {
    MOUTH_BAR,
    MOUTH_CURVE,
    MOUTH_D_OUTLINE
};


struct EyeParams {
    float x;
    float y;
    float width;
    float height;
    float angle;
    float strokeWidth;
    EyeShape shape;
};


struct EyebrowParams {
    float x1;
    float y1;
    float x2;
    float y2;
    float strokeWidth;
};


struct MouthParams {
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;
    float curve = 0.f;
    float strokeWidth = 0.f;
    MouthShape shape;
};


struct Colour {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};


struct FaceOffset {
    float x = 0.f;
    float y = 0.f;
};


enum BlinkPhase : uint8_t {
    BLINK_IDLE,
    BLINK_CLOSING,
    BLINK_CLOSED,
    BLINK_OPENING
};


struct BlinkParams {
    uint32_t timer = 0;
    uint32_t nextTime = 0;
    BlinkPhase phase = BLINK_IDLE;
};


enum HopPhase : uint8_t {
    HOP_IDLE,
    HOP_UP,
    HOP_DOWN,
    HOP_SQUASH
};


struct HopParams {
    uint32_t hopTimer = 0;
    uint32_t nextHopTime = 0;
    float hopHeight = 0.f;
    int16_t hopsRemaining = 0;
    HopPhase phase = HOP_IDLE;
};


struct GlanceParams {
    uint32_t nextGlanceTime = 0;
    uint32_t glanceReturnTime = 0;
    float glanceOffset = 0.f;
    bool glancing = false;
};


struct ClenchParams {
    uint32_t nextClenchTime = 0;
    uint32_t clenchReturnTime = 0;
    bool clenching = false;
};


enum FaceState : uint8_t {
    FACE_NEUTRAL,
    FACE_CELEBRATING,
    FACE_ANGRY
};


struct NeutralFaceParams {
    BlinkParams blink;
    GlanceParams glance;
};


struct CelebratingFaceParams {
    HopParams hop;
};


struct AngryFaceParams {
    BlinkParams blink;
    ClenchParams clench;
};


union FaceInfo {
    NeutralFaceParams neutral;
    CelebratingFaceParams celebrating;
    AngryFaceParams angry;

    FaceInfo() {
        memset(static_cast<void*>(this), 0, sizeof(FaceInfo));
    }

    ~FaceInfo() {}
};


class Face {
    private:
        EyeParams _leftEye;
        EyeParams _rightEye;
        EyebrowParams _leftBrow;
        EyebrowParams _rightBrow;
        MouthParams _mouth;
        FaceOffset _offset;
        Colour _colour;
        FaceInfo _info;
        FaceState _state;

        void _updateBlink(BlinkParams& blink, uint32_t now);
        void _scheduleBlink(BlinkParams& blink, uint32_t now);
        void _applyBlink(const BlinkParams& blink, uint32_t now);
        void _updateGlance(GlanceParams& glance, uint32_t now);
        void _updateHop(HopParams& hop, uint32_t now);
        void _updateClench(ClenchParams& clench, uint32_t now);

        Face();
    public:
        static Face& getFace();
        void update(uint32_t now);

        void setState(FaceState state);
        inline FaceState getState() const {return _state;}

        inline const EyeParams& getLeftEyeParams() const {return _leftEye;}
        inline const EyeParams& getRightEyeParams() const {return _rightEye;}
        inline const EyebrowParams& getLeftBrowParams()  const {return _leftBrow;}
        inline const EyebrowParams& getRightBrowParams() const {return _rightBrow;}
        inline const MouthParams& getMouthParams() const {return _mouth;}
        inline Colour getColour() const {return _colour;}
        inline FaceOffset getOffset() const {return _offset;}
};

#endif // FACE_H
