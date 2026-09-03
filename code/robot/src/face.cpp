#include "face.h"
#include "platform.h"

#include <stdlib.h>
#include <math.h>

#define BLINK_CLOSE_MS 25
#define BLINK_HOLD_MS 60
#define BLINK_OPEN_MS 25
#define BLINK_MIN_HEIGHT 2.f

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

#define THINKING_SCAN_PERIOD_MS 1600.f
#define PLEASED_BOB_PERIOD_MS 900.f
#define PLEASED_BOB_HEIGHT 1.5f
#define WORRIED_SHAKE_PERIOD_MS 180.f
#define WORRIED_SHAKE_WIDTH 1.5f

static constexpr float FACE_TWO_PI = 6.28318530718f;


struct PersonalityProfile {
    EyeParams leftEye;
    EyeParams rightEye;
    EyebrowParams leftBrow;
    EyebrowParams rightBrow;
    MouthParams mouth;
    Colour colour;
    FaceDecorations decorations;
    uint16_t blinkMinWait;
    uint16_t blinkMaxWait;
    uint16_t glanceMinWait;
    uint16_t glanceMaxWait;
    uint16_t glanceHoldMin;
    uint16_t glanceHoldMax;
    float glanceMinX;
    float glanceMaxX;
    float glanceMinY;
    float glanceMaxY;
};


static const PersonalityProfile PERSONALITY_PROFILES[FACE_PERSONALITY_COUNT] = {
    // Standard: calm and attentive.
    {
        {43.f, 43.f, 26.f, 26.f, 0.f, 0.f, EYE_RECT},
        {97.f, 43.f, 26.f, 26.f, 0.f, 0.f, EYE_RECT},
        {}, {},
        {70.f, 85.f, 60.f, 6.f, 0.f, 0.f, MOUTH_BAR},
        {91, 196, 216}, {},
        2500, 5000, 5000, 9000, 400, 700,
        -6.f, -3.f, 2.f, 4.f
    },
    // Mistakes: asymmetrical and a little sheepish.
    {
        {42.f, 44.f, 30.f, 24.f, -5.f, 0.f, EYE_RECT},
        {98.f, 48.f, 20.f, 20.f, 7.f, 0.f, EYE_RECT},
        {24.f, 24.f, 57.f, 20.f, 4.f},
        {84.f, 22.f, 111.f, 29.f, 4.f},
        {70.f, 88.f, 30.f, 13.f, 0.f, 4.f, MOUTH_D_OUTLINE},
        {244, 174, 66},
        {{}, {}, {}, {}, {24.f, 72.f, 34.f, 80.f, 3.f}, {34.f, 72.f, 24.f, 80.f, 3.f}, {}},
        1700, 3500, 3000, 6000, 350, 650,
        -8.f, -3.f, 2.f, 5.f
    },
    // Prolong: relaxed, patient, and teasing.
    {
        {43.f, 48.f, 30.f, 12.f, -2.f, 0.f, EYE_RECT},
        {97.f, 48.f, 30.f, 12.f, 2.f, 0.f, EYE_RECT},
        {25.f, 27.f, 59.f, 25.f, 4.f},
        {81.f, 23.f, 113.f, 28.f, 4.f},
        {70.f, 88.f, 48.f, 0.f, 10.f, 5.f, MOUTH_CURVE},
        {172, 126, 235}, {},
        4500, 7500, 6500, 10000, 650, 1000,
        -6.f, -2.f, 1.f, 3.f
    },
    // Center: precise, stable, and symmetrical.
    {
        {43.f, 43.f, 24.f, 30.f, 0.f, 0.f, EYE_RECT},
        {97.f, 43.f, 24.f, 30.f, 0.f, 0.f, EYE_RECT},
        {27.f, 22.f, 59.f, 22.f, 4.f},
        {81.f, 22.f, 113.f, 22.f, 4.f},
        {70.f, 88.f, 50.f, 5.f, 0.f, 0.f, MOUTH_BAR},
        {88, 161, 244},
        {{}, {}, {}, {64.f, 68.f, 70.f, 76.f, 76.f, 68.f, 0.f, true}, {}, {}, {}},
        3000, 5200, 5500, 8500, 350, 600,
        -5.f, -3.f, 2.f, 3.f
    },
    // Edge: mischievous and quick to look across the board.
    {
        {43.f, 47.f, 28.f, 17.f, -6.f, 0.f, EYE_RECT},
        {97.f, 47.f, 28.f, 17.f, 6.f, 0.f, EYE_RECT},
        {24.f, 22.f, 58.f, 28.f, 5.f},
        {83.f, 28.f, 116.f, 20.f, 5.f},
        {70.f, 88.f, 52.f, 0.f, 12.f, 5.f, MOUTH_CURVE},
        {225, 112, 190}, {},
        2300, 4300, 3200, 6000, 450, 750,
        -9.f, -5.f, 2.f, 4.f
    },
    // Stacker: determined and upward-driven.
    {
        {43.f, 46.f, 27.f, 22.f, 5.f, 0.f, EYE_RECT},
        {97.f, 46.f, 27.f, 22.f, -5.f, 0.f, EYE_RECT},
        {24.f, 27.f, 59.f, 21.f, 5.f},
        {81.f, 21.f, 116.f, 27.f, 5.f},
        {70.f, 89.f, 52.f, 6.f, 0.f, 0.f, MOUTH_BAR},
        {240, 145, 70},
        {{}, {}, {}, {}, {64.f, 68.f, 70.f, 76.f, 4.f}, {70.f, 76.f, 76.f, 68.f, 4.f}, {}},
        2400, 4300, 4000, 7000, 350, 600,
        -6.f, -3.f, 1.f, 3.f
    },
    // Spreader: wide-eyed, curious, and scanning.
    {
        {43.f, 43.f, 32.f, 31.f, 0.f, 0.f, EYE_RECT},
        {97.f, 43.f, 32.f, 31.f, 0.f, 0.f, EYE_RECT},
        {}, {},
        {70.f, 89.f, 28.f, 12.f, 0.f, 4.f, MOUTH_D_OUTLINE},
        {72, 211, 184}, {},
        2100, 4000, 2400, 5000, 300, 550,
        -9.f, -2.f, 2.f, 5.f
    },
    // Pacifist: gentle, concerned, and soft-spoken.
    {
        {43.f, 49.f, 34.f, 18.f, 0.f, 5.f, EYE_ARC},
        {97.f, 49.f, 34.f, 18.f, 0.f, 5.f, EYE_ARC},
        {25.f, 24.f, 58.f, 20.f, 4.f},
        {82.f, 20.f, 115.f, 24.f, 4.f},
        {70.f, 90.f, 48.f, 0.f, 9.f, 5.f, MOUTH_CURVE},
        {137, 210, 145},
        {{}, {}, {}, {}, {}, {}, {120.f, 27.f, 9.f, 17.f, true}},
        3200, 5600, 5200, 8800, 500, 850,
        -5.f, -2.f, 2.f, 4.f
    },
    // Copycat: unmistakably feline, including ears and a triangle nose.
    {
        {43.f, 48.f, 27.f, 18.f, -8.f, 0.f, EYE_RECT},
        {97.f, 48.f, 27.f, 18.f, 8.f, 0.f, EYE_RECT},
        {}, {},
        {70.f, 96.f, 38.f, 0.f, 10.f, 4.f, MOUTH_CURVE},
        {239, 151, 72},
        {
            {8.f, 27.f, 20.f, 2.f, 51.f, 19.f, 5.f, false},
            {89.f, 19.f, 120.f, 2.f, 132.f, 27.f, 5.f, false},
            {64.f, 73.f, 76.f, 73.f, 70.f, 81.f, 0.f, true},
            {}, {}, {}, {}
        },
        1900, 3800, 3000, 5500, 400, 700,
        -8.f, -3.f, 2.f, 5.f
    },
    // Trap: narrow eyes, one raised brow, and a restrained smirk.
    {
        {43.f, 49.f, 29.f, 14.f, 5.f, 0.f, EYE_RECT},
        {97.f, 49.f, 29.f, 14.f, -5.f, 0.f, EYE_RECT},
        {24.f, 27.f, 58.f, 24.f, 4.f},
        {82.f, 17.f, 115.f, 25.f, 5.f},
        {73.f, 90.f, 42.f, 8.f, 6.f, 5.f, MOUTH_SMIRK},
        {137, 205, 92},
        {},
        3000, 5200, 4800, 7800, 500, 850,
        -8.f, -4.f, 2.f, 4.f
    }
};


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


static bool timeReached(uint32_t now, uint32_t target) {
    return static_cast<int32_t>(now - target) >= 0;
}


static Colour blendColour(const Colour& base, const Colour& tint, uint8_t tintAmount) {
    uint16_t baseAmount = 255 - tintAmount;
    return {
        static_cast<uint8_t>((base.r * baseAmount + tint.r * tintAmount) / 255),
        static_cast<uint8_t>((base.g * baseAmount + tint.g * tintAmount) / 255),
        static_cast<uint8_t>((base.b * baseAmount + tint.b * tintAmount) / 255)
    };
}


static void applyThinking(EyeParams& le, EyeParams& re, EyebrowParams& lb, EyebrowParams& rb,
                          MouthParams& mouth, Colour& colour) {
    le = {43.f, 47.f, 28.f, 21.f, -2.f, 0.f, EYE_RECT};
    re = {97.f, 47.f, 28.f, 21.f, 2.f, 0.f, EYE_RECT};
    lb = {25.f, 25.f, 59.f, 20.f, 4.f};
    rb = {81.f, 20.f, 115.f, 25.f, 4.f};
    mouth = {70.f, 91.f, 22.f, 5.f, 0.f, 0.f, MOUTH_DOTS};
    colour = blendColour(colour, {118, 149, 245}, 72);
}


static void applyPleased(EyeParams& le, EyeParams& re, EyebrowParams& lb, EyebrowParams& rb,
                         MouthParams& mouth, Colour& colour) {
    le = {43.f, 49.f, 35.f, 19.f, 0.f, 5.f, EYE_ARC};
    re = {97.f, 49.f, 35.f, 19.f, 0.f, 5.f, EYE_ARC};
    lb = {};
    rb = {};
    mouth = {70.f, 87.f, 54.f, 0.f, 14.f, 5.f, MOUTH_CURVE};
    colour = blendColour(colour, {111, 214, 124}, 72);
}


static void applyWorried(EyeParams& le, EyeParams& re, EyebrowParams& lb, EyebrowParams& rb,
                         MouthParams& mouth, Colour& colour) {
    le = {43.f, 49.f, 31.f, 29.f, 0.f, 0.f, EYE_RECT};
    re = {97.f, 49.f, 31.f, 29.f, 0.f, 0.f, EYE_RECT};
    lb = {24.f, 24.f, 59.f, 17.f, 5.f};
    rb = {81.f, 17.f, 116.f, 24.f, 5.f};
    mouth = {70.f, 96.f, 46.f, 0.f, -13.f, 5.f, MOUTH_CURVE};
    colour = blendColour(colour, {238, 163, 73}, 80);
}


static void applyCelebrating(EyeParams& le, EyeParams& re, EyebrowParams& lb, EyebrowParams& rb,
                             MouthParams& mouth, Colour& colour) {
    le = {43.f, 48.f, 38.f, 24.f, 0.f, 5.f, EYE_ARC};
    re = {97.f, 48.f, 38.f, 24.f, 0.f, 5.f, EYE_ARC};
    lb = {0.f, 0.f, 0.f, 0.f, 0.f};
    rb = {0.f, 0.f, 0.f, 0.f, 0.f};
    mouth = {70.f, 78.f, 80.f, 32.f, 0.f, 5.f, MOUTH_D_OUTLINE};
    colour = blendColour(colour, {93, 202, 101}, 96);
}


static void applyAngry(EyeParams& le, EyeParams& re, EyebrowParams& lb, EyebrowParams& rb,
                       MouthParams& mouth, Colour& colour) {
    le = {43.f, 54.f, 26.f, 16.f,  6.f, 0.f, EYE_RECT};
    re = {97.f, 54.f, 26.f, 16.f, -6.f, 0.f, EYE_RECT};
    lb = {22.f, 22.f, 62.f, 36.f, 6.f};
    rb = {118.f, 22.f, 78.f, 36.f, 6.f};
    mouth = {70.f, 100.f, 64.f, 0.f, -20.f, 6.f, MOUTH_CURVE};
    colour = blendColour(colour, {226, 75, 74}, 112);
}


static void loadAngryClench(EyebrowParams& lb, EyebrowParams& rb, MouthParams& mouth) {
    lb = {22.f, 18.f, 62.f, 38.f, 8.f};
    rb = {118.f, 18.f, 78.f, 38.f, 8.f};
    mouth = {70.f, 100.f, 68.f, 0.f, -24.f, 7.f, MOUTH_CURVE};
}


Face::Face() : _state(FACE_NEUTRAL), _personality(FACE_PERSONALITY_STANDARD) {
    _offset = {0.f, 0.f};
    _loadPose();
    _info.neutral = {};
}


void Face::_loadPose() {
    const PersonalityProfile& profile = PERSONALITY_PROFILES[_personality];
    _leftEye = profile.leftEye;
    _rightEye = profile.rightEye;
    _leftBrow = profile.leftBrow;
    _rightBrow = profile.rightBrow;
    _mouth = profile.mouth;
    _colour = profile.colour;
    _decorations = profile.decorations;

    switch (_state) {
        case FACE_NEUTRAL:
            break;
        case FACE_CELEBRATING:
            applyCelebrating(_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _colour);
            break;
        case FACE_ANGRY:
            applyAngry(_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _colour);
            break;
        case FACE_THINKING:
            applyThinking(_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _colour);
            break;
        case FACE_PLEASED:
            applyPleased(_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _colour);
            break;
        case FACE_WORRIED:
            applyWorried(_leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _colour);
            break;
    }
}


void Face::_resetStateInfo() {
    switch (_state) {
        case FACE_NEUTRAL:
            _info.neutral = {};
            break;
        case FACE_CELEBRATING:
            _info.celebrating = {};
            break;
        case FACE_ANGRY:
            _info.angry = {};
            break;
        case FACE_THINKING:
            _info.thinking = {};
            break;
        case FACE_PLEASED:
            _info.pleased = {};
            break;
        case FACE_WORRIED:
            _info.worried = {};
            break;
    }
}


void Face::setState(FaceState state) {
    bool changed = state != _state;
    _stateTimed = false;
    _stateExpiresAt = 0;
    if (!changed) return;

    _state = state;
    _offset = {0.f, 0.f};
    _loadPose();
    _resetStateInfo();
    _requiresDraw = true;
}


void Face::triggerReaction(FaceState state, uint32_t now, uint32_t durationMs) {
    if (durationMs == 0) {
        setState(state);
        return;
    }

    _state = state;
    _stateTimed = true;
    _stateExpiresAt = now + durationMs;
    _offset = {0.f, 0.f};
    _loadPose();
    _resetStateInfo();
    _requiresDraw = true;
}


void Face::clearReaction() {
    setState(FACE_NEUTRAL);
}


void Face::setPersonality(FacePersonality personality) {
    if (personality >= FACE_PERSONALITY_COUNT || personality == _personality) return;

    _personality = personality;
    _offset = {0.f, 0.f};
    _loadPose();
    _resetStateInfo();
    _requiresDraw = true;
}


void Face::_scheduleBlink(BlinkParams& blink, uint32_t now) {
    const PersonalityProfile& profile = PERSONALITY_PROFILES[_personality];
    blink.nextTime = now + randRange(profile.blinkMinWait, profile.blinkMaxWait);
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
    const PersonalityProfile& profile = PERSONALITY_PROFILES[_personality];

    if (glance.nextGlanceTime == 0) {
        glance.nextGlanceTime = now + randRange(profile.glanceMinWait, profile.glanceMaxWait);
    }

    if (glance.glancing) {
        if (now >= glance.glanceReturnTime) {
            glance.glanceOffsetX = 0.f;
            glance.glanceOffsetY = 0.f;
            glance.glancing = false;
            glance.nextGlanceTime = now + randRange(profile.glanceMinWait, profile.glanceMaxWait);
        }
    } else if (now >= glance.nextGlanceTime) {
        // The display sits above and to the right of the physical board, so
        // negative X and positive Y make idle glances meet the play area.
        glance.glanceOffsetX = randFloat(profile.glanceMinX, profile.glanceMaxX);
        glance.glanceOffsetY = randFloat(profile.glanceMinY, profile.glanceMaxY);
        glance.glanceReturnTime = now + randRange(profile.glanceHoldMin, profile.glanceHoldMax);
        glance.glancing = true;
    }

    _leftEye.x += glance.glanceOffsetX;
    _leftEye.y += glance.glanceOffsetY;
    _rightEye.x += glance.glanceOffsetX;
    _rightEye.y += glance.glanceOffsetY;
}


void Face::_updateThinking(uint32_t now) {
    float phase = FACE_TWO_PI * (static_cast<float>(now % static_cast<uint32_t>(THINKING_SCAN_PERIOD_MS))
                                 / THINKING_SCAN_PERIOD_MS);
    float x = -5.f + 2.5f * sinf(phase);
    float y = 3.f + cosf(phase);
    _leftEye.x += x;
    _leftEye.y += y;
    _rightEye.x += x;
    _rightEye.y += y;
}


void Face::_updatePleased(uint32_t now) {
    float phase = FACE_TWO_PI * (static_cast<float>(now % static_cast<uint32_t>(PLEASED_BOB_PERIOD_MS))
                                 / PLEASED_BOB_PERIOD_MS);
    _offset.y = -0.5f * PLEASED_BOB_HEIGHT * (1.f + sinf(phase));
}


void Face::_updateWorried(uint32_t now) {
    float phase = FACE_TWO_PI * (static_cast<float>(now % static_cast<uint32_t>(WORRIED_SHAKE_PERIOD_MS))
                                 / WORRIED_SHAKE_PERIOD_MS);
    _offset.x = WORRIED_SHAKE_WIDTH * sinf(phase);
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
    FaceSnapshot current = {
        _leftEye, _rightEye, _leftBrow, _rightBrow, _mouth, _offset, _colour, _decorations
    };

    _requiresDraw = !(current == _previous);
    _previous = current;
}


void Face::update(uint32_t now) {
    if (_stateTimed && timeReached(now, _stateExpiresAt)) {
        clearReaction();
    }

    _loadPose();

    switch (_state) {
        case FACE_NEUTRAL:
            _updateBlink(_info.neutral.blink, now);
            _applyBlink(_info.neutral.blink, now);
            _updateGlance(_info.neutral.glance, now);
            break;

        case FACE_CELEBRATING:
            _updateHop(_info.celebrating.hop, now);
            break;

        case FACE_ANGRY:
            _updateBlink(_info.angry.blink, now);
            _applyBlink(_info.angry.blink, now);
            _updateClench(_info.angry.clench, now);
            break;

        case FACE_THINKING:
            _updateBlink(_info.thinking.blink, now);
            _applyBlink(_info.thinking.blink, now);
            _updateThinking(now);
            break;

        case FACE_PLEASED:
            _updateBlink(_info.pleased.blink, now);
            _applyBlink(_info.pleased.blink, now);
            _updatePleased(now);
            break;

        case FACE_WORRIED:
            _updateBlink(_info.worried.blink, now);
            _applyBlink(_info.worried.blink, now);
            _updateWorried(now);
            break;
    }

    _updateRequiresDraw();
}


Face& Face::getFace() {
    static Face face;
    return face;
}
