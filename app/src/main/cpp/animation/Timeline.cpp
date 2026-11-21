#include "Timeline.h"

#include <cmath>
#include <utility>

namespace ClipPaint {

bool AnimationTimeline::setFps(int fps) {
    if (fps < kMinTimelineFps || fps > kMaxTimelineFps) return false;
    fps_ = fps;
    playbackRemainder_ = 0.0;
    return true;
}

bool AnimationTimeline::setFrameCount(int count) {
    if (count < 1 || count > kMaxTimelineFrames) return false;
    frameCount_ = count;
    currentFrame_ = std::min(currentFrame_, frameCount_ - 1);
    for (auto& track : tracks_) {
        if (track.frames.size() > static_cast<size_t>(count)) track.frames.resize(static_cast<size_t>(count));
        for (auto& frame : track.frames)
            frame.exposure = std::max(1, std::min(frame.exposure, count));
        track.keys.erase(std::remove_if(track.keys.begin(), track.keys.end(),
            [count](const AnimationKeyframe& key) { return key.frame >= count; }), track.keys.end());
    }
    return true;
}

bool AnimationTimeline::seek(int frame) {
    if (frame < 0 || frame >= frameCount_) return false;
    const bool changed = currentFrame_ != frame;
    currentFrame_ = frame;
    playbackRemainder_ = 0.0;
    return changed;
}

uint64_t AnimationTimeline::addTrack(const std::string& name) {
    AnimationTrack track;
    track.id = nextId_++;
    track.name = name.empty() ? "Track " + std::to_string(tracks_.size() + 1) : name;
    tracks_.push_back(std::move(track));
    return tracks_.back().id;
}

AnimationTrack* AnimationTimeline::findTrack(uint64_t id) {
    for (auto& track : tracks_) if (track.id == id) return &track;
    return nullptr;
}

const AnimationTrack* AnimationTimeline::findTrack(uint64_t id) const {
    for (const auto& track : tracks_) if (track.id == id) return &track;
    return nullptr;
}

bool AnimationTimeline::removeTrack(uint64_t id) {
    const auto it = std::find_if(tracks_.begin(), tracks_.end(),
        [id](const AnimationTrack& track) { return track.id == id; });
    if (it == tracks_.end()) return false;
    tracks_.erase(it);
    return true;
}

bool AnimationTimeline::moveTrack(uint64_t id, int destination) {
    if (destination < 0 || destination >= static_cast<int>(tracks_.size())) return false;
    const auto it = std::find_if(tracks_.begin(), tracks_.end(),
        [id](const AnimationTrack& track) { return track.id == id; });
    if (it == tracks_.end()) return false;
    const int from = static_cast<int>(it - tracks_.begin());
    if (from == destination) return true;
    AnimationTrack moved = std::move(*it);
    tracks_.erase(tracks_.begin() + from);
    tracks_.insert(tracks_.begin() + destination, std::move(moved));
    return true;
}

uint64_t AnimationTimeline::insertFrame(uint64_t id, int atFrame, uint64_t celId) {
    AnimationTrack* track = findTrack(id);
    if (!track || track->locked || atFrame < 0 || atFrame >= kMaxTimelineFrames) return 0;
    AnimationFrame frame;
    frame.id = nextId_++;
    frame.celId = celId == 0 ? frame.id : celId;
    const int insertAt = std::min(atFrame, static_cast<int>(track->frames.size()));
    track->frames.insert(track->frames.begin() + insertAt, frame);
    if (static_cast<int>(track->frames.size()) > frameCount_)
        setFrameCount(static_cast<int>(track->frames.size()));
    for (auto& key : track->keys) if (key.frame >= insertAt) ++key.frame;
    return frame.id;
}

bool AnimationTimeline::duplicateFrame(uint64_t id, int sourceFrame, int destinationFrame) {
    AnimationTrack* track = findTrack(id);
    if (!track || track->locked || sourceFrame < 0 ||
        sourceFrame >= static_cast<int>(track->frames.size()) ||
        destinationFrame < 0 || destinationFrame >= kMaxTimelineFrames) return false;
    AnimationFrame copy = track->frames[static_cast<size_t>(sourceFrame)];
    copy.id = nextId_++;
    copy.celId = nextId_++; // duplicated frames start as independent editable cels
    const int at = std::min(destinationFrame, static_cast<int>(track->frames.size()));
    track->frames.insert(track->frames.begin() + at, copy);
    for (auto& key : track->keys) if (key.frame >= at) ++key.frame;
    if (static_cast<int>(track->frames.size()) > frameCount_)
        setFrameCount(static_cast<int>(track->frames.size()));
    return true;
}

bool AnimationTimeline::removeFrame(uint64_t id, int frame) {
    AnimationTrack* track = findTrack(id);
    if (!track || track->locked || frame < 0 || frame >= static_cast<int>(track->frames.size())) return false;
    track->frames.erase(track->frames.begin() + frame);
    for (auto& key : track->keys) if (key.frame > frame) --key.frame;
    track->keys.erase(std::remove_if(track->keys.begin(), track->keys.end(),
        [frame](const AnimationKeyframe& key) { return key.frame == frame; }), track->keys.end());
    return true;
}

bool AnimationTimeline::setExposure(uint64_t id, int frame, int exposure) {
    AnimationTrack* track = findTrack(id);
    if (!track || track->locked || frame < 0 || frame >= static_cast<int>(track->frames.size()) ||
        exposure < 1 || exposure > kMaxTimelineFrames) return false;
    track->frames[static_cast<size_t>(frame)].exposure = exposure;
    return true;
}

bool AnimationTimeline::setKeyframe(uint64_t id, const AnimationKeyframe& value) {
    AnimationTrack* track = findTrack(id);
    if (!track || track->locked || value.frame < 0 || value.frame >= frameCount_ ||
        !std::isfinite(value.value)) return false;
    auto it = std::find_if(track->keys.begin(), track->keys.end(),
        [&value](const AnimationKeyframe& key) {
            return key.frame == value.frame && key.property == value.property;
        });
    AnimationKeyframe key = value;
    if (it == track->keys.end()) {
        key.id = nextId_++;
        track->keys.push_back(key);
    } else {
        key.id = it->id;
        *it = key;
    }
    std::sort(track->keys.begin(), track->keys.end(),
        [](const AnimationKeyframe& a, const AnimationKeyframe& b) { return a.frame < b.frame; });
    return true;
}

bool AnimationTimeline::removeKeyframe(uint64_t id, uint64_t keyId) {
    AnimationTrack* track = findTrack(id);
    if (!track || track->locked) return false;
    const auto it = std::find_if(track->keys.begin(), track->keys.end(),
        [keyId](const AnimationKeyframe& key) { return key.id == keyId; });
    if (it == track->keys.end()) return false;
    track->keys.erase(it);
    return true;
}

bool AnimationTimeline::setPlaying(bool playing) {
    if (playing && frameCount_ < 2) return false;
    playing_ = playing;
    playbackRemainder_ = 0.0;
    return true;
}

bool AnimationTimeline::advance(double deltaSeconds) {
    if (!playing_ || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0) return false;
    // Limita grandes saltos causados por pausa/suspensao do app.
    playbackRemainder_ += std::min(deltaSeconds, 0.25) * fps_;
    const int steps = static_cast<int>(playbackRemainder_);
    if (steps <= 0) return false;
    playbackRemainder_ -= steps;
    const int next = (currentFrame_ + steps) % frameCount_;
    const bool changed = next != currentFrame_;
    currentFrame_ = next;
    return changed;
}

void AnimationTimeline::reset() {
    fps_ = 24;
    currentFrame_ = 0;
    frameCount_ = 1;
    playing_ = false;
    playbackRemainder_ = 0.0;
    nextId_ = 1;
    tracks_.clear();
    onion_ = OnionSkinSettings{};
}

} // namespace ClipPaint
