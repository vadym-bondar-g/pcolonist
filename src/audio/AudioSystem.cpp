#include "pcolonist/audio/AudioSystem.hpp"

#include <stdexcept>
#include <utility>

namespace pcolonist {

AudioHandle AudioSystem::createSource(std::string asset, bool looping) {
    const AudioHandle handle = nextHandle_++;
    sources_.emplace(handle, AudioSource{std::move(asset), 1.0F, looping, false});
    return handle;
}

void AudioSystem::play(AudioHandle handle) {
    sources_.at(handle).playing = true;
}

void AudioSystem::stop(AudioHandle handle) {
    sources_.at(handle).playing = false;
}

void AudioSystem::update(float deltaTime) {
    static_cast<void>(deltaTime);
}

std::size_t AudioSystem::playingCount() const {
    std::size_t count = 0;
    for (const auto& [handle, source] : sources_) {
        static_cast<void>(handle);
        count += source.playing ? 1U : 0U;
    }
    return count;
}

} // namespace pcolonist
