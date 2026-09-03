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
    MOUTH_D_OUTLINE,
    MOUTH_SMIRK,
    MOUTH_DOTS
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


struct TriangleParams {
    float x1 = 0.f;
    float y1 = 0.f;
    float x2 = 0.f;
    float y2 = 0.f;
    float x3 = 0.f;
    float y3 = 0.f;
    float strokeWidth = 0.f;
    bool filled = false;
};


struct DecorationLineParams {
    float x1 = 0.f;
    float y1 = 0.f;
    float x2 = 0.f;
    float y2 = 0.f;
    float strokeWidth = 0.f;
};


struct TeardropParams {
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;
    bool visible = false;
};


struct FaceDecorations {
    TriangleParams leftEar;
    TriangleParams rightEar;
    TriangleParams nose;
    TriangleParams accent;
    DecorationLineParams accentLine1;
    DecorationLineParams accentLine2;
    TeardropParams teardrop;
};


inline bool operator==(const EyeParams& a, const EyeParams& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height &&
           a.angle == b.angle && a.strokeWidth == b.strokeWidth && a.shape == b.shape;
}


inline bool operator==(const EyebrowParams& a, const EyebrowParams& b) {
    return a.x1 == b.x1 && a.y1 == b.y1 && a.x2 == b.x2 && a.y2 == b.y2 && a.strokeWidth == b.strokeWidth;
}


inline bool operator==(const MouthParams& a, const MouthParams& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height &&
           a.curve == b.curve && a.strokeWidth == b.strokeWidth && a.shape == b.shape;
}


inline bool operator==(const Colour& a, const Colour& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}


inline bool operator==(const FaceOffset& a, const FaceOffset& b) {
    return a.x == b.x && a.y == b.y;
}


inline bool operator==(const TriangleParams& a, const TriangleParams& b) {
    return a.x1 == b.x1 && a.y1 == b.y1 && a.x2 == b.x2 && a.y2 == b.y2 &&
           a.x3 == b.x3 && a.y3 == b.y3 && a.strokeWidth == b.strokeWidth &&
           a.filled == b.filled;
}


inline bool operator==(const DecorationLineParams& a, const DecorationLineParams& b) {
    return a.x1 == b.x1 && a.y1 == b.y1 && a.x2 == b.x2 && a.y2 == b.y2 &&
           a.strokeWidth == b.strokeWidth;
}


inline bool operator==(const TeardropParams& a, const TeardropParams& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height &&
           a.visible == b.visible;
}


inline bool operator==(const FaceDecorations& a, const FaceDecorations& b) {
    return a.leftEar == b.leftEar && a.rightEar == b.rightEar && a.nose == b.nose &&
           a.accent == b.accent && a.accentLine1 == b.accentLine1 &&
           a.accentLine2 == b.accentLine2 && a.teardrop == b.teardrop;
}


// Everything drawFace() reads, so a match means the rendered face is identical.
struct FaceSnapshot {
    EyeParams leftEye;
    EyeParams rightEye;
    EyebrowParams leftBrow;
    EyebrowParams rightBrow;
    MouthParams mouth;
    FaceOffset offset;
    Colour colour;
    FaceDecorations decorations;
};


inline bool operator==(const FaceSnapshot& a, const FaceSnapshot& b) {
    return a.leftEye == b.leftEye && a.rightEye == b.rightEye &&
           a.leftBrow == b.leftBrow && a.rightBrow == b.rightBrow &&
           a.mouth == b.mouth && a.offset == b.offset && a.colour == b.colour &&
           a.decorations == b.decorations;
}


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
    float glanceOffsetX = 0.f;
    float glanceOffsetY = 0.f;
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
    FACE_ANGRY,
    FACE_THINKING,
    FACE_PLEASED,
    FACE_WORRIED
};


// Personality is persistent; FaceState is a temporary emotional expression.
// These remain independent of Player and are mapped at the game/UI boundary.
enum FacePersonality : uint8_t {
    FACE_PERSONALITY_STANDARD,
    FACE_PERSONALITY_MISTAKES,
    FACE_PERSONALITY_PROLONG,
    FACE_PERSONALITY_CENTER,
    FACE_PERSONALITY_EDGE,
    FACE_PERSONALITY_STACKER,
    FACE_PERSONALITY_SPREADER,
    FACE_PERSONALITY_PACIFIST,
    FACE_PERSONALITY_COPYCAT,
    FACE_PERSONALITY_TRAP,
    FACE_PERSONALITY_COUNT
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


struct ThinkingFaceParams {
    BlinkParams blink;
};


struct PleasedFaceParams {
    BlinkParams blink;
};


struct WorriedFaceParams {
    BlinkParams blink;
};


union FaceInfo {
    NeutralFaceParams neutral;
    CelebratingFaceParams celebrating;
    AngryFaceParams angry;
    ThinkingFaceParams thinking;
    PleasedFaceParams pleased;
    WorriedFaceParams worried;

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
        FaceDecorations _decorations;
        FaceInfo _info;
        FaceState _state;
        FacePersonality _personality;
        uint32_t _stateExpiresAt = 0;
        bool _stateTimed = false;
        FaceSnapshot _previous = {};
        bool _requiresDraw = true;

        void _loadPose();
        void _resetStateInfo();
        void _updateRequiresDraw();
        void _updateBlink(BlinkParams& blink, uint32_t now);
        void _scheduleBlink(BlinkParams& blink, uint32_t now);
        void _applyBlink(const BlinkParams& blink, uint32_t now);
        void _updateGlance(GlanceParams& glance, uint32_t now);
        void _updateThinking(uint32_t now);
        void _updatePleased(uint32_t now);
        void _updateWorried(uint32_t now);
        void _updateHop(HopParams& hop, uint32_t now);
        void _updateClench(ClenchParams& clench, uint32_t now);

        Face();
    public:
        static Face& getFace();
        void update(uint32_t now);

        void setState(FaceState state);
        void triggerReaction(FaceState state, uint32_t now, uint32_t durationMs);
        void clearReaction();
        inline FaceState getState() const {return _state;}
        void setPersonality(FacePersonality personality);
        inline FacePersonality getPersonality() const {return _personality;}
        inline bool requiresDraw() const {return _requiresDraw;}

        inline const EyeParams& getLeftEyeParams() const {return _leftEye;}
        inline const EyeParams& getRightEyeParams() const {return _rightEye;}
        inline const EyebrowParams& getLeftBrowParams()  const {return _leftBrow;}
        inline const EyebrowParams& getRightBrowParams() const {return _rightBrow;}
        inline const MouthParams& getMouthParams() const {return _mouth;}
        inline const FaceDecorations& getDecorations() const {return _decorations;}
        inline Colour getColour() const {return _colour;}
        inline FaceOffset getOffset() const {return _offset;}
};

#endif // FACE_H
