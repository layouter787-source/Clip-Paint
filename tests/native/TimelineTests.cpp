#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "Timeline.h"

using namespace ClipPaint;

int main() {
    AnimationTimeline timeline;
    assert(timeline.fps() == 24);
    assert(timeline.currentFrame() == 0);
    assert(timeline.frameCount() == 1);

    const uint64_t track = timeline.addTrack("Layer 1");
    assert(track != 0);
    const uint64_t firstFrame = timeline.insertFrame(track, 0);
    assert(firstFrame != 0);
    assert(timeline.tracks().size() == 1);
    assert(timeline.tracks()[0].frames.size() == 1);

    const uint64_t firstCel = timeline.tracks()[0].frames[0].celId;
    assert(firstCel != 0);

    assert(timeline.setFrameCount(3));
    assert(timeline.duplicateFrame(track, 0, 1));
    assert(timeline.tracks()[0].frames.size() == 2);
    const uint64_t duplicateCel = timeline.tracks()[0].frames[1].celId;
    assert(duplicateCel != 0);
    assert(duplicateCel != firstCel);

    assert(timeline.setExposure(track, 0, 2));
    assert(timeline.tracks()[0].frames[0].exposure == 2);
    assert(!timeline.setExposure(track, 0, 0));

    AnimationKeyframe key;
    key.frame = 2;
    key.property = AnimatedProperty::Opacity;
    key.value = 0.5f;
    key.interpolation = KeyInterpolation::EaseInOut;
    assert(timeline.setKeyframe(track, key));
    assert(timeline.tracks()[0].keys.size() == 1);
    assert(timeline.tracks()[0].keys[0].frame == 2);
    assert(std::fabs(timeline.tracks()[0].keys[0].value - 0.5f) < 0.0001f);

    assert(timeline.setFps(20));
    assert(!timeline.setFps(0));
    assert(!timeline.setFps(121));
    assert(timeline.setPlaying(true));
    assert(timeline.advance(0.05));
    assert(timeline.currentFrame() == 1);
    assert(timeline.advance(0.05));
    assert(timeline.currentFrame() == 2);
    assert(timeline.advance(0.05));
    assert(timeline.currentFrame() == 0);
    assert(timeline.setPlaying(false));
    assert(!timeline.advance(0.1));

    assert(timeline.removeFrame(track, 1));
    assert(timeline.tracks()[0].frames.size() == 1);
    assert(timeline.removeTrack(track));
    assert(timeline.tracks().empty());

    // Imported sidecar cel IDs must not collide with IDs allocated afterward.
    AnimationTimeline imported;
    const uint64_t importedTrack = imported.addTrack("Imported");
    assert(imported.insertFrame(importedTrack, 0, 50000) != 0);
    const uint64_t nextFrame = imported.insertFrame(importedTrack, 1);
    assert(nextFrame != 0);
    assert(imported.tracks()[0].frames[1].celId != 50000);

    std::cout << "Timeline model tests passed\n";
    return 0;
}
