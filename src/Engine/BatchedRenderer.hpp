#ifndef SIRPG_BATCHED_RENDERER_HPP
#define SIRPG_BATCHED_RENDERER_HPP

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <vector>
#include <algorithm>
#include <span>
#include <expected>
#include "../Core/Logger.hpp"

namespace sirpg::engine {

struct RenderQuadCommand {
    SDL_FRect srcRect;
    SDL_FRect dstRect;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float rotation{0.0f};
    int zIndex{0};
    bool flipHorizontal{false};
    bool flipVertical{false};
};

class BatchedRenderer {
public:
    static constexpr std::size_t MAX_QUADS_PER_BATCH = 2048;

    BatchedRenderer() {
        m_quadBuffer.reserve(MAX_QUADS_PER_BATCH);
    }

    void beginBatch() noexcept {
        m_quadBuffer.clear();
    }

    void submitQuad(const RenderQuadCommand& quad) noexcept {
        if (m_quadBuffer.size() < MAX_QUADS_PER_BATCH) {
            m_quadBuffer.push_back(quad);
        }
    }

    void flush(SDL_Renderer* renderer, SDL_Texture* atlasTexture) noexcept {
        if (m_quadBuffer.empty()) return;

        // Sort quads by Z-Index for correct layered rendering
        std::sort(m_quadBuffer.begin(), m_quadBuffer.end(), [](const RenderQuadCommand& a, const RenderQuadCommand& b) {
            return a.zIndex < b.zIndex;
        });

        for (const auto& quad : m_quadBuffer) {
            SDL_SetTextureColorMod(atlasTexture,
                static_cast<Uint8>(quad.color.r * 255.0f),
                static_cast<Uint8>(quad.color.g * 255.0f),
                static_cast<Uint8>(quad.color.b * 255.0f));
            SDL_SetTextureAlphaMod(atlasTexture, static_cast<Uint8>(quad.color.a * 255.0f));

            SDL_FlipMode flipMode = SDL_FLIP_NONE;
            if (quad.flipHorizontal && quad.flipVertical) {
                flipMode = static_cast<SDL_FlipMode>(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL);
            } else if (quad.flipHorizontal) {
                flipMode = SDL_FLIP_HORIZONTAL;
            } else if (quad.flipVertical) {
                flipMode = SDL_FLIP_VERTICAL;
            }

            if (quad.rotation != 0.0f) {
                SDL_FPoint center{quad.dstRect.w * 0.5f, quad.dstRect.h * 0.5f};
                SDL_RenderTextureRotated(renderer, atlasTexture, &quad.srcRect, &quad.dstRect, quad.rotation, &center, flipMode);
            } else if (flipMode != SDL_FLIP_NONE) {
                SDL_FPoint center{quad.dstRect.w * 0.5f, quad.dstRect.h * 0.5f};
                SDL_RenderTextureRotated(renderer, atlasTexture, &quad.srcRect, &quad.dstRect, 0.0, &center, flipMode);
            } else {
                SDL_RenderTexture(renderer, atlasTexture, &quad.srcRect, &quad.dstRect);
            }
        }

        m_quadBuffer.clear();
    }

private:
    std::vector<RenderQuadCommand> m_quadBuffer;
};

} // namespace sirpg::engine

#endif // SIRPG_BATCHED_RENDERER_HPP
