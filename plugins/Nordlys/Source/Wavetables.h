#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <complex>
#include <vector>
#include "Params.h"

/*  Procedurally generated, band-limited wavetable banks (no samples needed).

    Every bank has up to maxFrames single-cycle frames that the Shape control morphs between.
    Each frame is stored as numMips versions: mip k contains harmonics up to 1024 >> k, so the
    oscillator can always pick a version that has nothing above Nyquist (no aliasing).
    Generated once per process with an inverse FFT and shared by all plugin instances.
*/
namespace nordlys
{
    class WaveBank
    {
    public:
        static constexpr int tableSize = 2048;
        static constexpr int numMips   = 11;
        static constexpr int maxFrames = 8;
        static constexpr int numBanks  = 8;

        static const WaveBank& get()
        {
            static const WaveBank instance;
            return instance;
        }

        int numFrames (int bank) const { return bank == 0 ? 4 : maxFrames; }

        /** Table with tableSize + 1 samples (last one wraps). */
        const float* table (int bank, int frame, int mip) const
        {
            return data.data() + (size_t) (((bank * maxFrames + frame) * numMips + mip) * (tableSize + 1));
        }

        /** Mip level for a phase increment (cycles per sample). */
        static int mipFor (float inc)
        {
            const float x = 1024.0f * inc / 0.45f;
            if (x <= 1.0f) return 0;
            return juce::jlimit (0, numMips - 1, (int) std::ceil (std::log2 (x)));
        }

        static float read (const float* t, float phase)
        {
            const float p = phase * (float) tableSize;
            const int i = juce::jlimit (0, tableSize - 1, (int) p);
            const float fr = p - (float) i;
            return t[i] + fr * (t[i + 1] - t[i]);
        }

        /** Morphed read: pos 0..1 across the bank's frames. */
        float readMorph (int bank, float pos, int mip, float phase) const
        {
            const int nf = numFrames (bank);
            const float fp = juce::jlimit (0.0f, 1.0f, pos) * (float) (nf - 1);
            const int f0 = juce::jmin ((int) fp, nf - 1);
            const int f1 = juce::jmin (f0 + 1, nf - 1);
            const float fr = fp - (float) f0;
            const float a = read (table (bank, f0, mip), phase);
            if (fr < 1.0e-4f || f0 == f1)
                return a;
            return a + fr * (read (table (bank, f1, mip), phase) - a);
        }

        /** Plain sine lookup (phase in cycles, any value). */
        float sine (float phase) const
        {
            phase -= std::floor (phase);
            return read (sineTable.data(), phase);
        }

    private:
        using Spectrum = std::vector<float>; // amplitude per harmonic, index 1..1023

        WaveBank()
        {
            data.resize ((size_t) (numBanks * maxFrames * numMips * (tableSize + 1)), 0.0f);
            sineTable.resize (tableSize + 1);
            for (int i = 0; i <= tableSize; ++i)
                sineTable[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * i / tableSize);

            std::vector<float> phases (1024, 0.0f);
            std::vector<float> randomPhases (1024, 0.0f);
            juce::Random rp (1234);
            for (auto& p : randomPhases) p = rp.nextFloat();

            for (int b = 0; b < numBanks; ++b)
                for (int f = 0; f < numFrames (b); ++f)
                {
                    const float t = numFrames (b) > 1 ? (float) f / (float) (numFrames (b) - 1) : 0.0f;
                    const auto spec = spectrum (b, f, t);
                    const auto& ph = (b == 5 || b == 7) ? randomPhases : phases;
                    float scale = 0.0f;
                    for (int m = 0; m < numMips; ++m)
                    {
                        float* dst = const_cast<float*> (table (b, f, m));
                        synthesise (spec, ph, 1024 >> m, dst);
                        if (m == 0)
                        {
                            float peak = 0.0f;
                            for (int i = 0; i < tableSize; ++i) peak = std::max (peak, std::abs (dst[i]));
                            scale = peak > 1.0e-6f ? 0.9f / peak : 0.0f;
                        }
                        for (int i = 0; i <= tableSize; ++i) dst[i] *= scale;
                    }
                }
        }

        static float gauss (float x, float c, float w) { return std::exp (-0.5f * (x - c) * (x - c) / (w * w)); }

        static Spectrum spectrum (int bank, int frame, float t)
        {
            Spectrum a (1024, 0.0f);
            switch (bank)
            {
                case 0: // Analog: sine, triangle, saw, square
                    for (int h = 1; h < 1024; ++h)
                    {
                        const bool odd = (h & 1) != 0;
                        switch (frame)
                        {
                            case 0: a[(size_t) h] = h == 1 ? 1.0f : 0.0f; break;
                            case 1: a[(size_t) h] = odd ? (((h - 1) / 2) % 2 == 0 ? 1.0f : -1.0f) / (float) (h * h) : 0.0f; break;
                            case 2: a[(size_t) h] = 1.0f / (float) h; break;
                            default: a[(size_t) h] = odd ? 1.0f / (float) h : 0.0f; break;
                        }
                    }
                    break;

                case 1: // Glass: bright, sparse comb that opens up across the table
                    for (int h = 1; h < 1024; ++h)
                    {
                        const float comb = std::pow (0.5f + 0.5f * std::cos ((float) h * (0.7f + 2.2f * t)), 6.0f);
                        a[(size_t) h] = (h == 1 ? 0.8f : 0.0f) + comb / std::pow ((float) h, 1.1f - 0.4f * t)
                                        * std::exp (-(float) h / (12.0f + 80.0f * t));
                    }
                    break;

                case 2: // Choir: vowel formants  a – o – u – e – i
                {
                    struct V { float f1, f2, f3; };
                    const V vowels[] = { { 800, 1150, 2900 }, { 450, 800, 2830 }, { 325, 700, 2530 },
                                         { 400, 1600, 2700 }, { 280, 2250, 3000 } };
                    const float vp = t * 4.0f;
                    const int v0 = std::min (3, (int) vp);
                    const float vf = vp - (float) v0;
                    auto lerp = [&] (float x, float y) { return x + vf * (y - x); };
                    const V v { lerp (vowels[v0].f1, vowels[v0 + 1].f1), lerp (vowels[v0].f2, vowels[v0 + 1].f2),
                                lerp (vowels[v0].f3, vowels[v0 + 1].f3) };
                    const float f0 = 196.0f;
                    for (int h = 1; h < 1024; ++h)
                    {
                        const float hz = f0 * (float) h;
                        a[(size_t) h] = (gauss (hz, v.f1, 90.0f) + 0.6f * gauss (hz, v.f2, 110.0f) + 0.25f * gauss (hz, v.f3, 150.0f)
                                         + 0.03f / (float) h) * (h == 1 ? 0.6f : 1.0f);
                    }
                    break;
                }

                case 3: // Strings: bowed saw with moving bow position
                {
                    const float beta = 0.08f + 0.35f * t;
                    for (int h = 1; h < 1024; ++h)
                        a[(size_t) h] = (0.25f + std::abs (std::sin (juce::MathConstants<float>::pi * (float) h * beta)))
                                        / (float) h * std::exp (-(float) h / (18.0f + 70.0f * t));
                    break;
                }

                case 4: // Organ: drawbar registrations from flute to full
                {
                    const int foot[] = { 1, 2, 3, 4, 5, 6, 8 };
                    const float regs[8][7] = { { 1, 0, 0, 0, 0, 0, 0 },     { 1, 0.6f, 0, 0, 0, 0, 0 },
                                               { 1, 0.8f, 0.5f, 0, 0, 0, 0 },{ 1, 0.7f, 0, 0.6f, 0, 0, 0.3f },
                                               { 1, 1, 0.6f, 0.5f, 0, 0, 0 },{ 1, 0.8f, 0.8f, 0.8f, 0.4f, 0.3f, 0.3f },
                                               { 0.8f, 1, 0.7f, 1, 0.5f, 0.6f, 0.7f }, { 1, 1, 1, 1, 0.8f, 0.8f, 1 } };
                    for (int i = 0; i < 7; ++i)
                        a[(size_t) foot[i]] = regs[frame][i];
                    break;
                }

                case 5: // Digital: seeded random spectra
                {
                    juce::Random r (100 + frame * 7);
                    const int top = 16 + frame * 12;
                    for (int h = 1; h < 1024; ++h)
                    {
                        const float x = r.nextFloat();
                        a[(size_t) h] = h <= top ? x * x * x / std::pow ((float) h, 0.6f) : 0.0f;
                    }
                    a[1] = std::max (a[1], 0.5f);
                    break;
                }

                case 6: // Air: soft, rounded tones getting slowly brighter
                    for (int h = 1; h < 1024; ++h)
                        a[(size_t) h] = std::exp (-(float) (h - 1) / (0.6f + 5.0f * t)) * ((h & 1) ? 1.0f : 0.5f + 0.4f * t);
                    break;

                default: // Metal: prime-numbered partials
                {
                    const int top = 3 + (int) (t * 60.0f);
                    for (int h = 1; h < 1024; ++h)
                    {
                        bool prime = h > 1;
                        for (int d = 2; d * d <= h && prime; ++d) prime = h % d != 0;
                        if (h == 1 || (prime && h <= top))
                            a[(size_t) h] = 1.0f / std::pow ((float) h, 0.55f);
                    }
                    break;
                }
            }
            return a;
        }

        /** x[n] = sum_h a_h sin(2 pi (h n / N + phase_h)), h <= maxH, via an inverse complex FFT. */
        static void synthesise (const Spectrum& amp, const std::vector<float>& phase, int maxH, float* out)
        {
            constexpr int N = tableSize;
            std::vector<std::complex<double>> X ((size_t) N);
            for (int h = 1; h < std::min (maxH + 1, N / 2); ++h)
                if (amp[(size_t) h] != 0.0f)
                    X[(size_t) h] = std::polar ((double) amp[(size_t) h], juce::MathConstants<double>::twoPi * phase[(size_t) h]);

            // in-place iterative radix-2 inverse FFT (no 1/N scaling)
            for (int i = 1, j = 0; i < N; ++i)
            {
                int bit = N >> 1;
                for (; j & bit; bit >>= 1) j ^= bit;
                j ^= bit;
                if (i < j) std::swap (X[(size_t) i], X[(size_t) j]);
            }
            for (int len = 2; len <= N; len <<= 1)
            {
                const auto w = std::polar (1.0, juce::MathConstants<double>::twoPi / len);
                for (int i = 0; i < N; i += len)
                {
                    std::complex<double> wk (1.0, 0.0);
                    for (int k = 0; k < len / 2; ++k)
                    {
                        const auto u = X[(size_t) (i + k)], v = X[(size_t) (i + k + len / 2)] * wk;
                        X[(size_t) (i + k)] = u + v;
                        X[(size_t) (i + k + len / 2)] = u - v;
                        wk *= w;
                    }
                }
            }
            for (int n = 0; n < N; ++n)
                out[n] = (float) X[(size_t) n].imag();
            out[N] = out[0];
        }

        std::vector<float> data;
        std::vector<float> sineTable;
    };
}
