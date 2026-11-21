// Clip Paint - modelo nativo da timeline de animacao.
// Independente da UI: a timeline e o documento animado vivem no motor C++.
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace ClipPaint {

constexpr int kMinTimelineFps = 1;
constexpr int kMaxTimelineFps = 120;
constexpr int kMaxTimelineFrames = 10000;

enum class KeyInterpolation : uint8_t { Step, Linear, EaseIn, EaseOut, EaseInOut };
enum class AnimatedProperty : uint8_t { Opacity, PositionX, PositionY, ScaleX, ScaleY, Rotation, Visibility };

struct AnimationKeyframe {
    uint64_t id = 0;
    int frame = 0;
    AnimatedProperty property = AnimatedProperty::Opacity;
    float value = 1.0f;
    KeyInterpolation interpolation = KeyInterpolation::Linear;
};

struct AnimationFrame {
    uint64_t id = 0;
    uint64_t celId = 0;       // identificador do cel raster; cels podem ser reutilizados por exposicao
    int exposure = 1;         // duracao em frames da exposicao
    bool enabled = true;
};

struct AnimationTrack {
    uint64_t id = 0;
    std::string name;
    bool visible = true;
    bool locked = false;
    float opacity = 1.0f;
    std::vector<AnimationFrame> frames;
    std::vector<AnimationKeyframe> keys;
};

struct OnionSkinSettings {
    bool enabled = true;
    int previousFrames = 2;
    int nextFrames = 1;
    float previousOpacity = 0.22f;
    float nextOpacity = 0.16f;
};

class AnimationTimeline {
public:
    int fps() const { return fps_; }
    int currentFrame() const { return currentFrame_; }
    int frameCount() const { return frameCount_; }
    bool playing() const { return playing_; }
    const std::vector<AnimationTrack>& tracks() const { return tracks_; }
    std::vector<AnimationTrack>& tracks() { return tracks_; }
    OnionSkinSettings& onionSkin() { return onion_; }
    const OnionSkinSettings& onionSkin() const { return onion_; }

    bool setFps(int fps);
    bool setFrameCount(int count);
    bool seek(int frame);
    uint64_t addTrack(const std::string& name);
    bool removeTrack(uint64_t trackId);
    bool moveTrack(uint64_t trackId, int destination);
    uint64_t insertFrame(uint64_t trackId, int atFrame, uint64_t celId = 0);
    bool duplicateFrame(uint64_t trackId, int sourceFrame, int destinationFrame);
    bool removeFrame(uint64_t trackId, int frame);
    bool setExposure(uint64_t trackId, int frame, int exposure);
    bool setKeyframe(uint64_t trackId, const AnimationKeyframe& key);
    bool removeKeyframe(uint64_t trackId, uint64_t keyId);
    bool setPlaying(bool playing);
    // Avanca em tempo real e devolve true quando o frame visivel mudou.
    bool advance(double deltaSeconds);
    void reset();

private:
    int fps_ = 24;
    int currentFrame_ = 0;
    int frameCount_ = 1;
    bool playing_ = false;
    double playbackRemainder_ = 0.0;
    uint64_t nextId_ = 1;
    std::vector<AnimationTrack> tracks_;
    OnionSkinSettings onion_{};

    AnimationTrack* findTrack(uint64_t id);
    const AnimationTrack* findTrack(uint64_t id) const;
};

} // namespace ClipPaint
