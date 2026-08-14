#include "face.h"
#include "platform.h"

#include <stdlib.h>
#include <math.h>

#define BLINK_CLOSE_MS 25
#define BLINK_HOLD_MS 60
#define BLINK_OPEN_MS 25
#define BLINK_MIN_WAIT 2500
#define BLINK_MAX_WAIT 5000
#define BLINK_MIN_HEIGHT 2.f

#define GLANCE_MIN_WAIT 5000
#define GLANCE_MAX_WAIT 9000
#define GLANCE_HOLD_MIN 400
#define GLANCE_HOLD_MAX 700
#define GLANCE_MIN_OFFSET 3.f
#define GLANCE_MAX_OFFSET 6.f

#define HOP_UP_MS 80
#define HOP_DOWN_MS 120
#define HOP_SQUASH_MS 60
#define HOP_MIN_HEIGHT 10.f
#define HOP_MAX_HEIGHT 14.f
#define HOP_MIN_WAIT 1800
#define HOP_MAX_WAIT 3300
#define HOP_BURST_MIN 2
#define HOP_BURST_MAX 4
#define HOP_BETWEEN_MS 180
#define HOP_SQUASH_SCALE 0.15f
#define HOP_SQUASH_DIP 4.f

#define CLENCH_MIN_WAIT 1500
#define CLENCH_MAX_WAIT 3500
#define CLENCH_HOLD_MS 350


static uint32_t randRange(uint32_t low, uint32_t high) {
    return low + (randomU32() % (high - low));
}


static float randFloat(float low, float high) {
    return low + (high - low) * (static_cast<float>(randomU32()) / static_cast<float>(UINT32_MAX));
}


static float phaseProgress(uint32_t now, uint32_t start, uint32_t durationMs) {
    float t = static_cast<float>(now - start) / static_cast<float>(durationMs);
    return (t > 1.f) ? 1.f : t;
}


static void loadNeutral(EyeParams& le, EyeParams& re, EyebrowParams& lb, EyebrowParams& rb, MouthParams& mouth, Colour& colour) {
    le = {43.f, 43.f, 26.f, 26.f, 0.f, 0.f, EYE_RECT};
    re = {97.f, 43.f, 26.f, 26.f, 0.f, 0.f, EYE_RECT};
    lb = {0.f, 0.f, 0.f, 0.f, 0.f};
    rb = {0.f, 0.f, 0.f, 0.f, 0.f};
    mouth = {70.f, 85.f, 60.f, 6.f, 0.f, 0.f, MOUTH_BAR};
    colour = {91, 196, 216};
}


static void loadCelebrating(EyeParams& le, EyeParams& re, EyebrowParams& lb, EyebrowParams& rb, MouthParams& mouth, Colour& colour) {
    le = {43.f, 48.f, 38.f, 24.f, 0.f, 5.f, EYE_ARC};
    re = {97.f, 48.f, 38.f, 24.f, 0.f, 5.f, EYE_ARC};
    lb = {0.f, 0.f, 0.f, 0.f, 0.f};
    rb = {0.f, 0.f, 0.f, 0.f, 0.f};
    mouth = {70.f, 78.f, 80.f, 32.f, 0.f, 5.f, MOUTH_D_OUTLINE};
    colour = {93, 202, 101};
}


static void loadAngry(EyeParams& le, EyeParams& re, EyebrowParams& lb, EyebrowParams& rb, MouthParams& mouth, Colour& colour) {
    le = {43.f, 54.f, 26.f, 16.f,  6.f, 0.f, EYE_RECT};
    re = {97.f, 54.f, 26.f, 16.f, -6.f, 0.f, EYE_RECT};
    lb = {22.f, 22.f, 62.f, 36.f, 6.f};
    rb = {118.f, 22.f, 78.f, 36.f, 6.f};
    mouth = {70.f, 100.f, 64.f, 0.f, -20.f, 6.f, MOUTH_CURVE};
    colour = {226, 75, 74};
}


static void loadAngryClench(EyebrowParams& lb, EyebrowParams& rb, MouthParams& mouth) {
    lb = {22.f, 18.f, 62.f, 38.f, 8.f};
    rb = {118.f, 18.f, 78.f, 38.f, 8.f};
    mouth = {70.f, 100.f, 68.f, 0.f, -24.f, 7.f, MOUTH_CURVE};
}


Face::Face() : _state(FACE_NEUTRAL) {
    _offset = {0.f, 0.f};
    loadNeutral(_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _colour);
    _info.neutral = {};
}


void Face::setState(FaceState state) {
    if (state == _state) return;

    _state = state;
    _offset = {0.f, 0.f};

    switch (state) {
        case FACE_NEUTRAL:
            loadNeutral(_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _colour);
            _info.neutral = {};
            break;
        case FACE_CELEBRATING:
            loadCelebrating(_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _colour);
            _info.celebrating = {};
            break;
        case FACE_ANGRY:
            loadAngry(_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _colour);
            _info.angry = {};
            break;
    }
}


void Face::_scheduleBlink(BlinkParams& blink, uint32_t now) {
    blink.nextTime = now + randRange(BLINK_MIN_WAIT, BLINK_MAX_WAIT);
    blink.phase = BLINK_IDLE;
}


void Face::_updateBlink(BlinkParams& blink, uint32_t now) {
    switch (blink.phase) {
        case BLINK_IDLE:
            if (blink.nextTime == 0) {
                _scheduleBlink(blink, now);
            }

            if (now >= blink.nextTime) {
                blink.phase = BLINK_CLOSING;
                blink.timer = now;
            }
            break;
        case BLINK_CLOSING:
            if (now - blink.timer >= BLINK_CLOSE_MS) {
                blink.phase = BLINK_CLOSED;
                blink.timer = now;
            }
            break;
        case BLINK_CLOSED:
            if (now - blink.timer >= BLINK_HOLD_MS) {
                blink.phase = BLINK_OPENING;
                blink.timer = now;
            }
            break;
        case BLINK_OPENING:
            if (now - blink.timer >= BLINK_OPEN_MS) {
                _scheduleBlink(blink, now);
            }
            break;
    }
}


void Face::_applyBlink(const BlinkParams& blink, uint32_t now) {
    float openness = 1.f;

    switch (blink.phase) {
        case BLINK_IDLE:
            return;
        case BLINK_CLOSING:
            openness = 1.f - phaseProgress(now, blink.timer, BLINK_CLOSE_MS);
            break;
        case BLINK_CLOSED:
            openness = 0.f;
            break;
        case BLINK_OPENING:
            openness = phaseProgress(now, blink.timer, BLINK_OPEN_MS);
            break;
    }

    _leftEye.height = BLINK_MIN_HEIGHT + (_leftEye.height - BLINK_MIN_HEIGHT) * openness;
    _rightEye.height = BLINK_MIN_HEIGHT + (_rightEye.height - BLINK_MIN_HEIGHT) * openness;
}


void Face::_updateGlance(GlanceParams& glance, uint32_t now) {
    if (glance.nextGlanceTime == 0) {
        glance.nextGlanceTime = now + randRange(GLANCE_MIN_WAIT, GLANCE_MAX_WAIT);
    }

    if (glance.glancing) {
        if (now >= glance.glanceReturnTime) {
            glance.glanceOffset = 0.f;
            glance.glancing = false;
            glance.nextGlanceTime = now + randRange(GLANCE_MIN_WAIT, GLANCE_MAX_WAIT);
        }
    } else if (now >= glance.nextGlanceTime) {
        float dir = (randomU32() % 2 == 0) ? 1.f : -1.f;

        glance.glanceOffset = dir * randFloat(GLANCE_MIN_OFFSET, GLANCE_MAX_OFFSET);
        glance.glanceReturnTime = now + randRange(GLANCE_HOLD_MIN, GLANCE_HOLD_MAX);
        glance.glancing = true;
    }

    _leftEye.x += glance.glanceOffset;
    _rightEye.x += glance.glanceOffset;
}


void Face::_updateHop(HopParams& hop, uint32_t now) {
    if (hop.nextHopTime == 0) {
        hop.nextHopTime = now + randRange(HOP_MIN_WAIT, HOP_MAX_WAIT);
    }

    switch (hop.phase) {
        case HOP_IDLE:
            _offset.y = 0.f;
            if (now >= hop.nextHopTime) {
                hop.hopsRemaining = randRange(HOP_BURST_MIN, HOP_BURST_MAX);
                hop.hopHeight = randFloat(HOP_MIN_HEIGHT, HOP_MAX_HEIGHT);
                hop.phase = HOP_UP;
                hop.hopTimer = now;
            }
            break;
        case HOP_UP: {
            if (now < hop.hopTimer) break; // still waiting out the gap between hops

            float t = phaseProgress(now, hop.hopTimer, HOP_UP_MS);
            if (t >= 1.f) {
                hop.phase = HOP_DOWN;
                hop.hopTimer = now;
            }
            float invT = 1.f - t;
            _offset.y = -hop.hopHeight * (1.f - invT * invT);
            break;
        }
        case HOP_DOWN: {
            float t = phaseProgress(now, hop.hopTimer, HOP_DOWN_MS);
            if (t >= 1.f) {
                hop.phase = HOP_SQUASH;
                hop.hopTimer = now;
            }
            _offset.y = -hop.hopHeight * (1.f - t * t);
            break;
        }
        case HOP_SQUASH: {
            float t = phaseProgress(now, hop.hopTimer, HOP_SQUASH_MS);
            if (t < 1.f) {
                float squash = (t < 0.5f) ? (1.f - HOP_SQUASH_SCALE * t) : (1.f - HOP_SQUASH_SCALE * (1.f - t));
                _offset.y = HOP_SQUASH_DIP * (1.f - squash);
                break;
            }

            _offset.y = 0.f;
            hop.hopsRemaining--;
            if (hop.hopsRemaining > 0) {
                hop.hopHeight = randFloat(HOP_MIN_HEIGHT, HOP_MAX_HEIGHT);
                hop.phase = HOP_UP;
                hop.hopTimer = now + HOP_BETWEEN_MS;
            } else {
                hop.phase = HOP_IDLE;
                hop.nextHopTime = now + randRange(HOP_MIN_WAIT, HOP_MAX_WAIT);
            }
            break;
        }
    }
}


void Face::_updateClench(ClenchParams& clench, uint32_t now) {
    if (clench.nextClenchTime == 0) {
        clench.nextClenchTime = now + randRange(CLENCH_MIN_WAIT, CLENCH_MAX_WAIT);
    }

    if (clench.clenching) {
        if (now >= clench.clenchReturnTime) {
            clench.clenching = false;
            clench.nextClenchTime = now + randRange(CLENCH_MIN_WAIT, CLENCH_MAX_WAIT);
        }
    } else if (now >= clench.nextClenchTime) {
        clench.clenching = true;
        clench.clenchReturnTime = now + CLENCH_HOLD_MS;
    }

    if (clench.clenching) {
        loadAngryClench(_leftBrow, _rightBrow, _mouth);
    }
}


void Face::_updateRequiresDraw() {
    FaceSnapshot current = {_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _offset, _colour};

    _requiresDraw = !(current == _previous);
    _previous = current;
}


void Face::update(uint32_t now) {
    switch (_state) {
        case FACE_NEUTRAL:
            loadNeutral(_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _colour);
            _updateBlink(_info.neutral.blink, now);
            _applyBlink(_info.neutral.blink, now);
            _updateGlance(_info.neutral.glance, now);
            break;

        case FACE_CELEBRATING:
            loadCelebrating(_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _colour);
            _updateHop(_info.celebrating.hop, now);
            break;

        case FACE_ANGRY:
            loadAngry(_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _colour);
            _updateBlink(_info.angry.blink, now);
            _applyBlink(_info.angry.blink, now);
            _updateClench(_info.angry.clench, now);
            break;
    }

    _updateRequiresDraw();
}


Face& Face::getFace() {
    static Face face;
    return face;
}
