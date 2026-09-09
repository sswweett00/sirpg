#ifndef SIRPG_TEXTURE_ATLAS_HPP
#define SIRPG_TEXTURE_ATLAS_HPP

#include <SDL3/SDL.h>
#include <vector>
#include <cstdint>
#include <expected>
#include "../Core/Logger.hpp"

namespace sirpg::engine {

enum class TextureError {
    CreationFailed,
    LockFailed
};

class TextureAtlas {
public:
    TextureAtlas() = default;
    ~TextureAtlas() {
        destroy();
    }

    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    std::expected<void, TextureError> generate(SDL_Renderer* renderer, int width = 512, int height = 512) {
        m_width = width;
        m_height = height;

        // Create surface with RGBA8888 pixel format
        SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA8888);
        if (!surface) {
            LOG_ERROR("Failed to create surface for texture atlas: {}", SDL_GetError());
            return std::unexpected(TextureError::CreationFailed);
        }

        // Fill background with transparent black
        SDL_FillSurfaceRect(surface, nullptr, SDL_MapRGBA(SDL_GetPixelFormatDetails(surface->format), nullptr, 0, 0, 0, 0));

        uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);
        int pitchPixels = surface->pitch / sizeof(uint32_t);

        auto setPixel = [pixels, pitchPixels, width, height](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                // RGBA8888 byte packing (R, G, B, A in system endianness or SDL_MapRGBA)
                uint32_t color = (r << 24) | (g << 16) | (b << 8) | a;
                pixels[y * pitchPixels + x] = color;
            }
        };

        auto drawRect = [&](int startX, int startY, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            for (int y = startY; y < startY + h; ++y) {
                for (int x = startX; x < startX + w; ++x) {
                    setPixel(x, y, r, g, b, a);
                }
            }
        };

        // 1. Tile 0: Air (Transparent) - 0,0,32,32

        // 2. Tile 1: Ground / Dirt with Grass Top (32,0,32,32)
        drawRect(32, 0, 32, 32, 101, 67, 33); // Brown dirt
        drawRect(32, 0, 32, 6, 34, 139, 34);   // Green grass top
        for (int i = 0; i < 32; i += 8) {
            drawRect(32 + i, 2, 4, 2, 50, 205, 50); // Grass detail
            drawRect(32 + i + 2, 12, 3, 3, 80, 50, 20); // Rock detail
        }

        // 3. Tile 2: Stone Brick Platform (64,0,32,32)
        drawRect(64, 0, 32, 32, 105, 105, 105); // Grey stone
        for (int y = 0; y < 32; y += 8) {
            drawRect(64, y, 32, 1, 60, 60, 60); // Brick lines
        }
        for (int x = 0; x < 32; x += 8) {
            drawRect(64 + x, 0, 1, 32, 60, 60, 60);
        }

        // 4. Tile 3: Spikes / Hazard (96,0,32,32)
        drawRect(96, 0, 32, 32, 0, 0, 0, 0); // Transparent base
        for (int i = 0; i < 4; ++i) {
            int spikeBaseX = 96 + i * 8;
            for (int sy = 0; sy < 24; ++sy) {
                int halfW = (24 - sy) / 3;
                drawRect(spikeBaseX + 4 - halfW, 32 - sy, halfW * 2 + 1, 1, 190, 190, 190);
            }
        }

        // 5. Player Sprites (0, 32 to 128, 64)
        // Row 1 (y=32): Player Idle (4 frames 32x32)
        for (int f = 0; f < 4; ++f) {
            int px = f * 32;
            int py = 32;
            // Body (Blue Armor)
            drawRect(px + 10, py + 8, 12, 16, 30, 144, 255);
            // Head (Helmet)
            drawRect(px + 11, py + 2, 10, 8, 192, 192, 192);
            // Visor
            drawRect(px + 13, py + 5, 6, 2, 10, 10, 10);
            // Legs
            drawRect(px + 10, py + 24, 4, 8, 50, 50, 50);
            drawRect(px + 18, py + 24, 4, 8, 50, 50, 50);
        }

        // Row 2 (y=64): Player Melee Attack Animation (4 frames 32x32)
        for (int f = 0; f < 4; ++f) {
            int px = f * 32;
            int py = 64;
            // Body
            drawRect(px + 10, py + 8, 12, 16, 30, 144, 255);
            drawRect(px + 11, py + 2, 10, 8, 192, 192, 192);
            drawRect(px + 10, py + 24, 5, 8, 50, 50, 50);
            drawRect(px + 17, py + 24, 5, 8, 50, 50, 50);
            // Sword Swing Arc
            int swordX = px + 22 + f * 2;
            int swordY = py + 4 + (3 - f) * 3;
            drawRect(swordX, swordY, 10, 4, 240, 240, 255); // Glowing blade
            drawRect(swordX - 2, swordY + 1, 3, 2, 139, 69, 19); // Hilt
        }

        // 6. Skeleton Sprites (y=96, 4 frames 32x32)
        for (int f = 0; f < 4; ++f) {
            int px = f * 32;
            int py = 96;
            // Bone white body
            drawRect(px + 11, py + 10, 10, 14, 220, 220, 210);
            // Skull
            drawRect(px + 10, py + 2, 12, 8, 240, 240, 230);
            // Eye sockets
            drawRect(px + 12, py + 5, 2, 2, 10, 10, 10);
            drawRect(px + 17, py + 5, 2, 2, 10, 10, 10);
            // Ribs detail
            drawRect(px + 12, py + 12, 8, 2, 50, 50, 50);
            drawRect(px + 12, py + 16, 8, 2, 50, 50, 50);
            // Legs
            drawRect(px + 11, py + 24, 3, 8, 200, 200, 190);
            drawRect(px + 18, py + 24, 3, 8, 200, 200, 190);
        }

        // 7. Wizard Sprites (y=128, 4 frames 32x32)
        for (int f = 0; f < 4; ++f) {
            int px = f * 32;
            int py = 128;
            // Purple Robe
            drawRect(px + 8, py + 10, 16, 20, 128, 0, 128);
            // Hood & face shadow
            drawRect(px + 9, py + 2, 14, 10, 75, 0, 130);
            drawRect(px + 12, py + 6, 8, 4, 20, 20, 20); // Dark cowl
            drawRect(px + 13, py + 7, 2, 2, 255, 215, 0); // Glowing yellow eye
            drawRect(px + 17, py + 7, 2, 2, 255, 215, 0);
            // Staff
            drawRect(px + 25, py + 4, 3, 26, 139, 69, 19);
            drawRect(px + 24, py + 2, 5, 5, 147, 112, 219); // Orb atop staff
        }

        // 8. Projectiles & Effects (y=160)
        // Fireball (160, 160, 32, 32)
        for (int r = 14; r >= 0; --r) {
            uint8_t colorR = 255;
            uint8_t colorG = static_cast<uint8_t>(200 - r * 12);
            uint8_t colorB = static_cast<uint8_t>(r * 10);
            for (int y = -r; y <= r; ++y) {
                for (int x = -r; x <= r; ++x) {
                    if (x * x + y * y <= r * r) {
                        setPixel(160 + 16 + x, 160 + 16 + y, colorR, colorG, colorB, 255);
                    }
                }
            }
        }

        // Wizard Dark Magic Orb (192, 160, 32, 32)
        for (int r = 14; r >= 0; --r) {
            uint8_t colorR = static_cast<uint8_t>(180 - r * 8);
            uint8_t colorG = static_cast<uint8_t>(20 + r * 5);
            uint8_t colorB = 255;
            for (int y = -r; y <= r; ++y) {
                for (int x = -r; x <= r; ++x) {
                    if (x * x + y * y <= r * r) {
                        setPixel(192 + 16 + x, 160 + 16 + y, colorR, colorG, colorB, 255);
                    }
                }
            }
        }

        // 9. Parallax Background Textures (y=256 to 512)
        // Layer 1: Distant Mountains (256x128 at 0, 256)
        drawRect(0, 256, 256, 128, 25, 25, 60); // Dark night sky base
        for (int x = 0; x < 256; ++x) {
            int peak1 = static_cast<int>(30.0 * std::sin(x * 0.05) + 60.0);
            drawRect(x, 256 + peak1, 1, 128 - peak1, 45, 50, 85);
        }

        // Layer 2: Near Trees / Hills (256x128 at 256, 256)
        drawRect(256, 256, 256, 128, 0, 0, 0, 0); // Transparent base
        for (int x = 0; x < 256; ++x) {
            int hill = static_cast<int>(20.0 * std::sin(x * 0.08 + 1.0) + 70.0);
            drawRect(256 + x, 256 + hill, 1, 128 - hill, 20, 70, 40);
        }

        // Create GPU Texture from Surface
        m_texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);

        if (!m_texture) {
            LOG_ERROR("Failed to create texture from generated surface: {}", SDL_GetError());
            return std::unexpected(TextureError::CreationFailed);
        }

        SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST);
        LOG_INFO("Successfully generated procedural texture atlas ({}x{})", m_width, m_height);

        return {};
    }

    void destroy() {
        if (m_texture) {
            SDL_DestroyTexture(m_texture);
            m_texture = nullptr;
        }
    }

    [[nodiscard]] SDL_Texture* getTexture() const noexcept { return m_texture; }
    [[nodiscard]] int getWidth() const noexcept { return m_width; }
    [[nodiscard]] int getHeight() const noexcept { return m_height; }

private:
    SDL_Texture* m_texture{nullptr};
    int m_width{512};
    int m_height{512};
};

} // namespace sirpg::engine

#endif // SIRPG_TEXTURE_ATLAS_HPP
