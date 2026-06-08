#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace pcolonist {

using AudioHandle = std::uint32_t;

struct AudioSource {
    std::string asset;
    float volume = 1.0F;
    bool looping = false;
    bool playing = false;
};

class AudioSystem {
public:
    AudioHandle createSource(std::string asset, bool looping = false);
    void play(AudioHandle handle);
    void stop(AudioHandle handle);
    void update(float deltaTime);
    [[nodiscard]] std::size_t playingCount() const;

private:
    AudioHandle nextHandle_ = 1;
    std::unordered_map<AudioHandle, AudioSource> sources_;
};

} // namespace pcolonist
