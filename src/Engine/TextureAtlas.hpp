#ifndef SIRPG_TEXTURE_ATLAS_HPP
#define SIRPG_TEXTURE_ATLAS_HPP

#include <SDL3/SDL.h>
#include <vector>
#include <cstdint>
#include <cmath>
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

        SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA8888);
        if (!surface) {
            LOG_ERROR("Failed to create surface for texture atlas: {}", SDL_GetError());
            return std::unexpected(TextureError::CreationFailed);
        }

        SDL_FillSurfaceRect(surface, nullptr, SDL_MapRGBA(SDL_GetPixelFormatDetails(surface->format), nullptr, 0, 0, 0, 0));

        uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);
        int pitchPixels = surface->pitch / sizeof(uint32_t);

        auto setPixel = [pixels, pitchPixels, width, height](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
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

        // 2. Tile 1: Lush Grass & Rich Soil with Wild Flowers (32,0,32,32)
        drawRect(32, 0, 32, 32, 85, 52, 26); // Rich brown soil
        drawRect(32, 0, 32, 7, 34, 139, 34);  // Deep forest green top
        drawRect(32, 0, 32, 3, 50, 205, 50);  // Bright grass blade highlights
        // Grass tuftshanging down into soil
        for (int i = 0; i < 32; i += 4) {
            drawRect(32 + i, 6, 2, 3 + (i % 3), 34, 139, 34);
        }
        // Wild red and yellow flower petals on grass
        drawRect(36, 2, 2, 2, 255, 69, 0);   // Red flower
        drawRect(37, 1, 1, 1, 255, 255, 0);  // Yellow center
        drawRect(52, 3, 2, 2, 255, 215, 0);  // Gold flower
        drawRect(53, 2, 1, 1, 255, 255, 255);
        // Soil pebbles
        for (int i = 0; i < 32; i += 8) {
            drawRect(32 + i + 2, 14 + (i % 5), 3, 2, 60, 38, 18);
            drawRect(32 + i + 4, 24 - (i % 7), 2, 2, 110, 75, 40);
        }

        // 3. Tile 2: Ancient Mossy Stone Bricks with Cracks (64,0,32,32)
        drawRect(64, 0, 32, 32, 90, 95, 100); // Slate stone grey
        // Brick mortar grooves
        for (int y = 0; y < 32; y += 8) {
            drawRect(64, y, 32, 1, 45, 50, 55);
            int xOffset = (y % 16 == 0) ? 0 : 8;
            for (int x = xOffset; x < 32; x += 16) {
                drawRect(64 + x, y, 1, 8, 45, 50, 55);
            }
        }
        // Green moss highlights in corners
        drawRect(64, 0, 8, 4, 46, 139, 87);
        drawRect(64 + 20, 16, 6, 3, 46, 139, 87);
        // Crack lines
        drawRect(64 + 10, 4, 1, 3, 30, 30, 30);
        drawRect(64 + 11, 7, 2, 1, 30, 30, 30);

        // 4. Tile 3: Iron Hazard Spikes with Bloodstains (96,0,32,32)
        drawRect(96, 0, 32, 32, 0, 0, 0, 0);
        for (int i = 0; i < 4; ++i) {
            int spikeBaseX = 96 + i * 8;
            for (int sy = 0; sy < 26; ++sy) {
                int halfW = (26 - sy) / 3;
                uint8_t steelShade = static_cast<uint8_t>(140 + sy * 4);
                drawRect(spikeBaseX + 4 - halfW, 32 - sy, halfW * 2 + 1, 1, steelShade, steelShade, steelShade + 15);
            }
            // Bloodstain tip
            drawRect(spikeBaseX + 3, 7, 3, 4, 178, 34, 34);
        }

        // 5. Tile 4: Ancient Background Ruin Wall (128,0,32,32)
        drawRect(128, 0, 32, 32, 40, 42, 50); // Darker indoor ruin stone
        for (int y = 0; y < 32; y += 8) {
            drawRect(128, y, 32, 1, 25, 27, 32);
        }

        // 6. Tile 5: Rustic Wooden Bridge Planks (160,0,32,32)
        drawRect(160, 0, 32, 32, 0, 0, 0, 0);
        drawRect(160, 8, 32, 16, 139, 69, 19); // Wood brown base
        for (int x = 0; x < 32; x += 8) {
            drawRect(160 + x, 8, 1, 16, 80, 40, 10); // Plank splits
            drawRect(160 + x + 2, 10, 2, 2, 192, 192, 192); // Iron bolts
            drawRect(160 + x + 2, 20, 2, 2, 192, 192, 192);
        }

        // 7. Tile 6: Wall Torch with Iron Bracket (192,0,32,32)
        drawRect(192, 0, 32, 32, 0, 0, 0, 0);
        drawRect(192 + 14, 14, 4, 14, 105, 105, 105); // Iron mount
        drawRect(192 + 13, 10, 6, 6, 139, 69, 19);   // Torch handle
        drawRect(192 + 12, 4, 8, 6, 255, 140, 0);    // Orange flame core
        drawRect(192 + 13, 2, 6, 4, 255, 215, 0);    // Yellow inner flame

        // 8. Tile 7: Foreground Hanging Vines & Leaves (224,0,32,32)
        drawRect(224, 0, 32, 32, 0, 0, 0, 0);
        for (int i = 0; i < 32; i += 6) {
            int vineLen = 12 + (i * 7 % 18);
            drawRect(224 + i, 0, 2, vineLen, 34, 139, 34);
            for (int l = 2; l < vineLen; l += 4) {
                drawRect(224 + i - 2, l, 6, 2, 50, 205, 50); // Leaf cluster
            }
        }

        // 9. Player Sprites (0, 32 to 128, 64)
        for (int f = 0; f < 4; ++f) {
            int px = f * 32;
            int py = 32;
            drawRect(px + 10, py + 8, 12, 16, 30, 144, 255);
            drawRect(px + 11, py + 2, 10, 8, 192, 192, 192);
            drawRect(px + 13, py + 5, 6, 2, 10, 10, 10);
            drawRect(px + 10, py + 24, 4, 8, 50, 50, 50);
            drawRect(px + 18, py + 24, 4, 8, 50, 50, 50);
        }

        for (int f = 0; f < 4; ++f) {
            int px = f * 32;
            int py = 64;
            drawRect(px + 10, py + 8, 12, 16, 30, 144, 255);
            drawRect(px + 11, py + 2, 10, 8, 192, 192, 192);
            drawRect(px + 10, py + 24, 5, 8, 50, 50, 50);
            drawRect(px + 17, py + 24, 5, 8, 50, 50, 50);
            int swordX = px + 22 + f * 2;
            int swordY = py + 4 + (3 - f) * 3;
            drawRect(swordX, swordY, 10, 4, 240, 240, 255);
            drawRect(swordX - 2, swordY + 1, 3, 2, 139, 69, 19);
        }

        // 10. Skeleton Sprites (y=96, 4 frames 32x32)
        for (int f = 0; f < 4; ++f) {
            int px = f * 32;
            int py = 96;
            drawRect(px + 11, py + 10, 10, 14, 220, 220, 210);
            drawRect(px + 10, py + 2, 12, 8, 240, 240, 230);
            drawRect(px + 12, py + 5, 2, 2, 10, 10, 10);
            drawRect(px + 17, py + 5, 2, 2, 10, 10, 10);
            drawRect(px + 12, py + 12, 8, 2, 50, 50, 50);
            drawRect(px + 12, py + 16, 8, 2, 50, 50, 50);
            drawRect(px + 11, py + 24, 3, 8, 200, 200, 190);
            drawRect(px + 18, py + 24, 3, 8, 200, 200, 190);
        }

        // 11. Wizard Sprites (y=128, 4 frames 32x32)
        for (int f = 0; f < 4; ++f) {
            int px = f * 32;
            int py = 128;
            drawRect(px + 8, py + 10, 16, 20, 128, 0, 128);
            drawRect(px + 9, py + 2, 14, 10, 75, 0, 130);
            drawRect(px + 12, py + 6, 8, 4, 20, 20, 20);
            drawRect(px + 13, py + 7, 2, 2, 255, 215, 0);
            drawRect(px + 17, py + 7, 2, 2, 255, 215, 0);
            drawRect(px + 25, py + 4, 3, 26, 139, 69, 19);
            drawRect(px + 24, py + 2, 5, 5, 147, 112, 219);
        }

        // 12. Projectiles & Collectibles (y=160)
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

        // Gold Coin (224, 160, 32, 32)
        for (int r = 10; r >= 0; --r) {
            uint8_t cr = 255;
            uint8_t cg = static_cast<uint8_t>(215 - r * 10);
            uint8_t cb = 0;
            for (int y = -r; y <= r; ++y) {
                for (int x = -r; x <= r; ++x) {
                    if (x * x + y * y <= r * r) {
                        setPixel(224 + 16 + x, 160 + 16 + y, cr, cg, cb, 255);
                    }
                }
            }
        }

        // Health Potion Bottle (256, 160, 32, 32)
        drawRect(256 + 12, 160 + 6, 8, 4, 180, 180, 180);
        drawRect(256 + 8, 160 + 10, 16, 16, 220, 20, 60);

        // Gem (288, 160, 32, 32)
        for (int i = 0; i < 12; ++i) {
            drawRect(288 + 16 - i, 160 + 8 + i, i * 2 + 1, 1, 0, 255, 255);
            drawRect(288 + 16 - i, 160 + 24 - i, i * 2 + 1, 1, 0, 200, 255);
        }

        // 13. Parallax Background Textures (y=256 to 512)
        // Layer 1: Distant Mountains (256x128 at 0, 256)
        drawRect(0, 256, 256, 128, 25, 25, 60);
        for (int x = 0; x < 256; ++x) {
            int peak1 = static_cast<int>(30.0 * std::sin(x * 0.05) + 60.0);
            drawRect(x, 256 + peak1, 1, 128 - peak1, 45, 50, 85);
        }

        // Layer 2: Near Trees / Hills (256x128 at 256, 256)
        drawRect(256, 256, 256, 128, 0, 0, 0, 0);
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
        LOG_INFO("Successfully generated realistic procedural texture atlas ({}x{})", m_width, m_height);

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
