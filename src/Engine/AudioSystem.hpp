#ifndef SIRPG_AUDIO_SYSTEM_HPP
#define SIRPG_AUDIO_SYSTEM_HPP

#include <SDL3/SDL.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include "../Core/Logger.hpp"

namespace sirpg::engine {

enum class SoundEffect {
    Jump,
    SwordSwing,
    Fireball,
    Hit,
    Coin,
    HealthPotion,
    LevelUp
};

class AudioSystem {
public:
    AudioSystem() = default;
    ~AudioSystem() {
        shutdown();
    }

    bool init() {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            LOG_WARN("Audio subsystem unavailable ({}); sound disabled.", SDL_GetError());
            return false;
        }

        SDL_AudioSpec spec;
        spec.format = SDL_AUDIO_F32;
        spec.channels = 1;
        spec.freq = 44100;

        m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!m_stream) {
            LOG_WARN("Failed to open audio device stream: {}. Audio disabled.", SDL_GetError());
            return false;
        }

        SDL_ResumeAudioStreamDevice(m_stream);
        m_initialized = true;
        LOG_INFO("Procedural Audio System initialized successfully at 44100Hz (SDL3 Audio Stream).");
        return true;
    }

    void shutdown() {
        if (m_stream) {
            SDL_DestroyAudioStream(m_stream);
            m_stream = nullptr;
        }
        if (m_initialized) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            m_initialized = false;
        }
    }

    void playSound(SoundEffect sfx) {
        if (!m_initialized || !m_stream) return;

        constexpr int sampleRate = 44100;
        std::vector<float> samples;

        switch (sfx) {
            case SoundEffect::Jump: {
                int totalSamples = static_cast<int>(0.15 * sampleRate);
                samples.resize(totalSamples);
                float phase = 0.0f;
                for (int i = 0; i < totalSamples; ++i) {
                    float t = static_cast<float>(i) / totalSamples;
                    float freq = 250.0f + t * 350.0f;
                    phase += (2.0f * 3.14159f * freq) / sampleRate;
                    float sample = (std::sin(phase) > 0.0f) ? 0.2f : -0.2f;
                    samples[i] = sample * (1.0f - t);
                }
                break;
            }
            case SoundEffect::SwordSwing: {
                int totalSamples = static_cast<int>(0.12 * sampleRate);
                samples.resize(totalSamples);
                for (int i = 0; i < totalSamples; ++i) {
                    float t = static_cast<float>(i) / totalSamples;
                    float noise = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * 0.25f;
                    samples[i] = noise * std::sin(t * 3.14159f);
                }
                break;
            }
            case SoundEffect::Fireball: {
                int totalSamples = static_cast<int>(0.2 * sampleRate);
                samples.resize(totalSamples);
                float phase = 0.0f;
                for (int i = 0; i < totalSamples; ++i) {
                    float t = static_cast<float>(i) / totalSamples;
                    float freq = 180.0f - t * 100.0f;
                    phase += (2.0f * 3.14159f * freq) / sampleRate;
                    float sine = std::sin(phase) * 0.3f;
                    float noise = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * 0.15f;
                    samples[i] = (sine + noise) * (1.0f - t);
                }
                break;
            }
            case SoundEffect::Hit: {
                int totalSamples = static_cast<int>(0.18 * sampleRate);
                samples.resize(totalSamples);
                float phase = 0.0f;
                for (int i = 0; i < totalSamples; ++i) {
                    float t = static_cast<float>(i) / totalSamples;
                    float freq = 180.0f * (1.0f - t * 0.7f);
                    phase += (2.0f * 3.14159f * freq) / sampleRate;
                    float sample = std::sin(phase) * 0.4f;
                    samples[i] = sample * (1.0f - t);
                }
                break;
            }
            case SoundEffect::Coin: {
                int totalSamples = static_cast<int>(0.18 * sampleRate);
                samples.resize(totalSamples);
                float phase = 0.0f;
                int half = totalSamples / 2;
                for (int i = 0; i < totalSamples; ++i) {
                    float freq = (i < half) ? 987.77f : 1318.51f;
                    phase += (2.0f * 3.14159f * freq) / sampleRate;
                    float sample = (std::sin(phase) > 0.0f) ? 0.2f : -0.2f;
                    samples[i] = sample * (1.0f - static_cast<float>(i) / totalSamples);
                }
                break;
            }
            case SoundEffect::HealthPotion: {
                int totalSamples = static_cast<int>(0.24 * sampleRate);
                samples.resize(totalSamples);
                float phase = 0.0f;
                int third = totalSamples / 3;
                for (int i = 0; i < totalSamples; ++i) {
                    float freq = 523.25f;
                    if (i >= third && i < 2 * third) freq = 659.25f;
                    else if (i >= 2 * third) freq = 783.99f;

                    phase += (2.0f * 3.14159f * freq) / sampleRate;
                    float sample = std::sin(phase) * 0.3f;
                    samples[i] = sample * (1.0f - static_cast<float>(i) / totalSamples);
                }
                break;
            }
            case SoundEffect::LevelUp: {
                int totalSamples = static_cast<int>(0.36 * sampleRate);
                samples.resize(totalSamples);
                float phase = 0.0f;
                int third = totalSamples / 3;
                for (int i = 0; i < totalSamples; ++i) {
                    float freq = 523.25f;
                    if (i >= third && i < 2 * third) freq = 783.99f;
                    else if (i >= 2 * third) freq = 1046.50f;

                    phase += (2.0f * 3.14159f * freq) / sampleRate;
                    float sample = (std::sin(phase) > 0.0f) ? 0.25f : -0.25f;
                    samples[i] = sample * (1.0f - static_cast<float>(i) / totalSamples);
                }
                break;
            }
        }

        if (!samples.empty()) {
            SDL_PutAudioStreamData(m_stream, samples.data(), static_cast<int>(samples.size() * sizeof(float)));
        }
    }

private:
    SDL_AudioStream* m_stream{nullptr};
    bool m_initialized{false};
};

} // namespace sirpg::engine

#endif // SIRPG_AUDIO_SYSTEM_HPP
