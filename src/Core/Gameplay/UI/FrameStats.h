#pragma once
#include <algorithm>

// Rolling window of frame times in milliseconds, behind the debug panel's FPS
// readout and graph. Fixed capacity and allocation-free, and fed the REAL frame
// time rather than the time-scaled one, so the readout stays honest while the
// scene is paused or running in slow motion.
class FrameStats {
public:
    static constexpr int kCapacity = 128;

    void Push(float deltaSeconds) {
        m_Last = deltaSeconds * 1000.0f;
        m_Samples[m_Head] = m_Last;
        m_Head = (m_Head + 1) % kCapacity;
        ++m_Frames;

        // Recomputed over the whole window instead of tracked incrementally:
        // 128 floats a frame is nothing, and a running min/max would need a
        // second pass anyway the moment the extreme scrolls out of the window.
        float total = 0.0f;
        float low = m_Samples[0];
        float high = m_Samples[0];
        int counted = 0;
        for (float sample : m_Samples) {
            if (sample <= 0.0f) continue; // slot never written yet
            total += sample;
            low = std::min(low > 0.0f ? low : sample, sample);
            high = std::max(high, sample);
            ++counted;
        }
        m_Average = counted > 0 ? total / static_cast<float>(counted) : 0.0f;
        m_Min = low;
        m_Max = high;
    }

    // Oldest-first iteration is ring[(Oldest() + i) % Count()]; the next slot to
    // be written is also the oldest one still in the window.
    const float* Ring() const { return m_Samples; }
    int Count() const { return kCapacity; }
    int Oldest() const { return m_Head; }

    float LastMs() const { return m_Last; }
    float AverageMs() const { return m_Average; }
    float MinMs() const { return m_Min; }
    float MaxMs() const { return m_Max; }

    // From the window average, not the last frame, so the number is readable
    // instead of flickering every frame.
    float Fps() const { return m_Average > 0.0f ? 1000.0f / m_Average : 0.0f; }
    float WorstFps() const { return m_Max > 0.0f ? 1000.0f / m_Max : 0.0f; }

    unsigned long long FrameIndex() const { return m_Frames; }

private:
    float m_Samples[kCapacity] = {};
    int m_Head = 0;
    unsigned long long m_Frames = 0;

    float m_Last = 0.0f;
    float m_Average = 0.0f;
    float m_Min = 0.0f;
    float m_Max = 0.0f;
};
