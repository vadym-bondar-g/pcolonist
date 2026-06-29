#include "pcolonist/render/Renderer.hpp"

#include "pcolonist/render/Shader.hpp"
#include "pcolonist/world/WeatherSystem.hpp"

#include <glad/gl.h>

#include <cstddef>
#include <stdexcept>

namespace pcolonist {

void Renderer::createRenderTargets() {
    if (width_ <= 0 || height_ <= 0) {
        return;
    }
    unsigned int hdrFramebuffer = 0;
    glGenFramebuffers(1, &hdrFramebuffer);
    hdrFramebuffer_.reset(GlHandle::Kind::Framebuffer, hdrFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFramebuffer_);
    unsigned int hdrColor = 0;
    glGenTextures(1, &hdrColor);
    hdrColor_.reset(GlHandle::Kind::Texture, hdrColor);
    glBindTexture(GL_TEXTURE_2D, hdrColor_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrColor_, 0);

    unsigned int hdrDepth = 0;
    glGenTextures(1, &hdrDepth);
    hdrDepth_.reset(GlHandle::Kind::Texture, hdrDepth);
    glBindTexture(GL_TEXTURE_2D, hdrDepth_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width_, height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, hdrDepth_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("HDR framebuffer is incomplete");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::releaseRenderTargets() {
    hdrDepth_.reset(GlHandle::Kind::Texture);
    hdrColor_.reset(GlHandle::Kind::Texture);
    hdrFramebuffer_.reset(GlHandle::Kind::Framebuffer);
}

void Renderer::releaseShadowMap() {
    for (GlHandle& depth : shadowDepths_) {
        depth.reset(GlHandle::Kind::Texture);
    }
    for (GlHandle& framebuffer : shadowFramebuffers_) {
        framebuffer.reset(GlHandle::Kind::Framebuffer);
    }
}

void Renderer::createShadowMap() {
    releaseShadowMap();
    for (std::size_t cascade = 0; cascade < shadowDepths_.size(); ++cascade) {
        unsigned int framebuffer = 0;
        unsigned int depth = 0;
        glGenFramebuffers(1, &framebuffer);
        glGenTextures(1, &depth);
        shadowFramebuffers_[cascade].reset(GlHandle::Kind::Framebuffer, framebuffer);
        shadowDepths_[cascade].reset(GlHandle::Kind::Texture, depth);
        glBindTexture(GL_TEXTURE_2D, shadowDepths_[cascade]);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT24,
            qualityConfig_.shadowSize,
            qualityConfig_.shadowSize,
            0,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        constexpr float border[] = {1.0F, 1.0F, 1.0F, 1.0F};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffers_[cascade]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepths_[cascade], 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Shadow framebuffer is incomplete");
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::postProcess(const WeatherSystem& weather, bool underwater) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    Shader& shader = shaders_.get("post");
    shader.use();
    shader.setInt("hdrScene", 0);
    shader.setInt("sceneDepth", 1);
    shader.setInt("bloomEnabled", bloomEnabled_ ? 1 : 0);
    shader.setInt("underwater", underwater ? 1 : 0);
    shader.setFloat("exposure", 1.08F + weather.daylight() * 0.12F + weather.cloudiness() * 0.055F);
    shader.setFloat("daylight", weather.daylight());
    shader.setFloat("cloudiness", weather.cloudiness());
    shader.setFloat("stormStrength", weather.stormStrength());
    shader.setFloat("time", weather.time());
    shader.setVec3("resolution", {static_cast<float>(width_), static_cast<float>(height_), 0.0F});
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrColor_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, hdrDepth_);
    glBindVertexArray(screenVertexArray_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEnable(GL_DEPTH_TEST);
}

} // namespace pcolonist
