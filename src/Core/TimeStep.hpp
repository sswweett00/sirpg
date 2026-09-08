#ifndef SIRPG_TIMESTEP_HPP
#define SIRPG_TIMESTEP_HPP

#include <chrono>

namespace sirpg::core {

class TimeStep {
public:
    explicit TimeStep(double fixedDeltaTime = 1.0 / 60.0)
        : m_fixedDeltaTime(fixedDeltaTime),
          m_accumulator(0.0),
          m_lastTime(Clock::now()) {}

    void tick() noexcept {
        auto currentTime = Clock::now();
        std::chrono::duration<double> frameTime = currentTime - m_lastTime;
        m_lastTime = currentTime;

        double delta = frameTime.count();
        // Cap max frame time to avoid spiral of death (e.g. max 0.25s)
        if (delta > 0.25) {
            delta = 0.25;
        }

        m_accumulator += delta;
        m_frameTime = delta;
    }

    [[nodiscard]] bool checkStep() noexcept {
        if (m_accumulator >= m_fixedDeltaTime) {
            m_accumulator -= m_fixedDeltaTime;
            return true;
        }
        return false;
    }

    [[nodiscard]] float getAlpha() const noexcept {
        return static_cast<float>(m_accumulator / m_fixedDeltaTime);
    }

    [[nodiscard]] float getFixedDeltaTime() const noexcept {
        return static_cast<float>(m_fixedDeltaTime);
    }

    [[nodiscard]] float getFrameTime() const noexcept {
        return static_cast<float>(m_frameTime);
    }

private:
    using Clock = std::chrono::high_resolution_clock;

    double m_fixedDeltaTime;
    double m_accumulator{0.0};
    double m_frameTime{0.0};
    Clock::time_point m_lastTime;
};

} // namespace sirpg::core

#endif // SIRPG_TIMESTEP_HPP
