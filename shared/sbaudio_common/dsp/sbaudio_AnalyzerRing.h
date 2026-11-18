#pragma once

namespace sbaudio
{
    /** Simple lock-free single-writer ring for spectrum analyzers (display only). */
    struct AnalyzerRing
    {
        static constexpr int size = 1 << 15;
        std::vector<float> data = std::vector<float> ((size_t) size, 0.0f);
        std::atomic<int> writePos { 0 };

        void push (const float* x, int n) noexcept
        {
            int w = writePos.load (std::memory_order_relaxed);
            for (int i = 0; i < n; ++i)
            {
                data[(size_t) w] = x[i];
                w = (w + 1) & (size - 1);
            }
            writePos.store (w, std::memory_order_release);
        }

        /** Copies the latest n samples (oldest first). */
        void read (float* dest, int n) const noexcept
        {
            int r = (writePos.load (std::memory_order_acquire) - n) & (size - 1);
            for (int i = 0; i < n; ++i)
            {
                dest[i] = data[(size_t) r];
                r = (r + 1) & (size - 1);
            }
        }
    };
}
