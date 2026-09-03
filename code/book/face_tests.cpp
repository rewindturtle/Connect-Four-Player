#include "face.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>


static bool isVisible(const TriangleParams& triangle) {
    return triangle.filled || triangle.strokeWidth >= 1.f;
}


static void testProfilesAreDistinct() {
    Face& face = Face::getFace();
    face.setState(FACE_NEUTRAL);

    uint32_t colours[FACE_PERSONALITY_COUNT] = {};
    for (uint8_t i = 0; i < FACE_PERSONALITY_COUNT; ++i) {
        FacePersonality personality = static_cast<FacePersonality>(i);
        face.setPersonality(personality);
        face.update(0);

        assert(face.getPersonality() == personality);
        Colour colour = face.getColour();
        colours[i] = (static_cast<uint32_t>(colour.r) << 16)
                   | (static_cast<uint32_t>(colour.g) << 8)
                   | colour.b;

        for (uint8_t previous = 0; previous < i; ++previous) {
            assert(colours[i] != colours[previous]);
        }
    }
}


static void testSignatureDecorations() {
    Face& face = Face::getFace();
    face.setState(FACE_NEUTRAL);

    face.setPersonality(FACE_PERSONALITY_COPYCAT);
    face.update(0);
    const FaceDecorations& cat = face.getDecorations();
    assert(isVisible(cat.leftEar));
    assert(isVisible(cat.rightEar));
    assert(cat.nose.filled);
    assert(cat.leftEar.x2 < face.getLeftEyeParams().x);
    assert(cat.rightEar.x2 > face.getRightEyeParams().x);
    assert(cat.leftEar.y3 < face.getLeftBrowParams().y1 || face.getLeftBrowParams().strokeWidth < 1.f);
    assert(cat.rightEar.y1 < face.getRightBrowParams().y1 || face.getRightBrowParams().strokeWidth < 1.f);

    face.setPersonality(FACE_PERSONALITY_MISTAKES);
    face.update(0);
    const FaceDecorations& mistakes = face.getDecorations();
    assert(mistakes.accentLine1.strokeWidth >= 1.f);
    assert(mistakes.accentLine2.strokeWidth >= 1.f);

    face.setPersonality(FACE_PERSONALITY_CENTER);
    face.update(0);
    const TriangleParams& centerNose = face.getDecorations().accent;
    assert(isVisible(centerNose));
    assert(centerNose.filled);
    assert(centerNose.y1 >= 60.f && centerNose.y2 >= 60.f && centerNose.y3 >= 60.f);

    face.setPersonality(FACE_PERSONALITY_STACKER);
    face.update(0);
    const FaceDecorations& stacker = face.getDecorations();
    assert(stacker.accentLine1.strokeWidth >= 1.f);
    assert(stacker.accentLine2.strokeWidth >= 1.f);
    assert(stacker.accentLine1.y1 >= 60.f && stacker.accentLine1.y2 >= 60.f);
    assert(stacker.accentLine2.y1 >= 60.f && stacker.accentLine2.y2 >= 60.f);

    face.setPersonality(FACE_PERSONALITY_PACIFIST);
    face.update(0);
    const TeardropParams& tear = face.getDecorations().teardrop;
    assert(tear.visible);
    assert(tear.width > 0.f && tear.height > tear.width);

    face.setPersonality(FACE_PERSONALITY_TRAP);
    face.update(0);
    const MouthParams& smirk = face.getMouthParams();
    assert(smirk.shape == MOUTH_SMIRK);
    assert(smirk.height > 0.f);
    assert(!isVisible(face.getDecorations().accent));
}


static void testDecorationsSurviveEmotions() {
    Face& face = Face::getFace();
    face.setPersonality(FACE_PERSONALITY_COPYCAT);

    face.setState(FACE_CELEBRATING);
    face.update(0);
    assert(isVisible(face.getDecorations().leftEar));
    assert(isVisible(face.getDecorations().rightEar));
    assert(face.getDecorations().nose.filled);

    face.setState(FACE_ANGRY);
    face.update(0);
    assert(isVisible(face.getDecorations().leftEar));
    assert(isVisible(face.getDecorations().rightEar));
    assert(face.getDecorations().nose.filled);
    assert(face.getDecorations().leftEar.y3 < face.getLeftBrowParams().y1);
    assert(face.getDecorations().rightEar.y1 < face.getRightBrowParams().y1);

    face.setState(FACE_NEUTRAL);
    face.setPersonality(FACE_PERSONALITY_STANDARD);
}


static void testReactionPoses() {
    Face& face = Face::getFace();
    face.setPersonality(FACE_PERSONALITY_COPYCAT);

    face.setState(FACE_THINKING);
    face.update(0);
    assert(face.getMouthParams().shape == MOUTH_DOTS);
    assert(face.getLeftEyeParams().x < 43.f);
    assert(face.getLeftEyeParams().y > 47.f);
    assert(isVisible(face.getDecorations().leftEar));

    face.setState(FACE_PLEASED);
    face.update(0);
    assert(face.getLeftEyeParams().shape == EYE_ARC);
    assert(face.getMouthParams().shape == MOUTH_CURVE);
    assert(face.getMouthParams().curve > 0.f);
    assert(isVisible(face.getDecorations().leftEar));

    face.setState(FACE_WORRIED);
    face.update(45);
    assert(face.getMouthParams().shape == MOUTH_CURVE);
    assert(face.getMouthParams().curve < 0.f);
    assert(face.getLeftBrowParams().y2 < face.getLeftBrowParams().y1);
    assert(face.getOffset().x > 0.f);
    assert(isVisible(face.getDecorations().leftEar));

    face.setState(FACE_CELEBRATING);
    face.update(0);
    assert(face.getMouthParams().shape == MOUTH_D_OUTLINE);
    assert(isVisible(face.getDecorations().leftEar));

    face.clearReaction();
    face.setPersonality(FACE_PERSONALITY_STANDARD);
}


static void testTimedReactionLifecycle() {
    Face& face = Face::getFace();
    face.setState(FACE_NEUTRAL);

    face.triggerReaction(FACE_PLEASED, 100, 100);
    assert(face.getState() == FACE_PLEASED);

    // Retriggering the same reaction restarts both its animation and lifetime.
    face.triggerReaction(FACE_PLEASED, 150, 100);
    face.update(200);
    assert(face.getState() == FACE_PLEASED);

    face.update(250);
    assert(face.getState() == FACE_NEUTRAL);

    // A zero duration is explicitly persistent, for thinking or game-over use.
    face.triggerReaction(FACE_THINKING, 300, 0);
    face.update(10000);
    assert(face.getState() == FACE_THINKING);
    face.clearReaction();
    assert(face.getState() == FACE_NEUTRAL);
}


int main() {
    testProfilesAreDistinct();
    testSignatureDecorations();
    testDecorationsSurviveEmotions();
    testReactionPoses();
    testTimedReactionLifecycle();
    std::puts("face tests passed");
    return 0;
}
