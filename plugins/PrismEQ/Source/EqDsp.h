#pragma once

#include <array>
#include <cmath>
#include <complex>
#include <juce_core/juce_core.h>

/*  PrismEQ DSP building blocks.

    Every band is a cascade of up to 12 sections. A section is either a trapezoidal
    state-variable filter (Cytomic "SvfLinearTrapOptimised2") or a TPT one-pole. Both
    modulate cleanly, so coefficients can be recomputed every few samples (smoothing,
    dynamic EQ) without zipper noise or instability.

    The same design is used for audio, for the display curve and for the linear-phase
    FIR, so what you see is exactly what you hear.
*/
namespace prism
{
    constexpr int numBands = 24;

    enum FilterType
    {
        Bell = 0, LowShelf, LowCut, HighShelf, HighCut, Notch, BandPass, TiltShelf, FlatTilt, AllPass,
        numTypes
    };

    enum Placement { Stereo = 0, Left, Right, Mid, Side };

    inline const juce::StringArray typeNames { "Bell", "Low Shelf", "Low Cut", "High Shelf", "High Cut",
                                               "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt", "All Pass" };
    inline const juce::StringArray slopeNames { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "30 dB/oct",
                                                "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct" };
    constexpr int slopeOrders[] = { 1, 2, 3, 4, 5, 6, 8, 12, 16 };
    inline const juce::StringArray placementNames { "Stereo", "Left", "Right", "Mid", "Side" };

    inline bool typeHasGain (int t)    { return t == Bell || t == LowShelf || t == HighShelf || t == TiltShelf || t == FlatTilt; }
    inline bool typeHasSlope (int t)   { return t == LowCut || t == HighCut; }
    inline bool typeCanDynamic (int t) { return t == Bell || t == LowShelf || t == HighShelf || t == TiltShelf; }

    /** Plain (denormalised) parameter values of one band. */
    struct BandState
    {
        bool used = false, on = true;
        int type = Bell, slope = 1, place = Stereo;
        float freq = 1000.0f, gain = 0.0f, q = 1.0f;
        bool dyn = false, sc = false;
        float range = -6.0f, thr = -30.0f, att = 5.0f, rel = 80.0f;
    };

    //==========================================================================
    struct Section
    {
        enum Kind : int { svf = 0, onePole = 1 };
        int kind = svf;
        double g = 0.1, k = 1.0, m0 = 1.0, m1 = 0.0, m2 = 0.0;
        // derived (svf)
        double a1 = 0.0, a2 = 0.0, a3 = 0.0;
        // derived (one-pole)
        double G = 0.0;

        void finalise()
        {
            if (kind == svf)
            {
                a1 = 1.0 / (1.0 + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }
            else
            {
                G = g / (1.0 + g);
            }
        }

        /** Exact digital response at frequency f (the trapezoidal integrator maps
            s -> tan(pi f / fs), so evaluating the analog prototype there is exact). */
        std::complex<double> response (double f, double fs) const
        {
            const double w = std::tan (juce::MathConstants<double>::pi * juce::jmin (f, fs * 0.49995) / fs);
            const std::complex<double> s (0.0, w / g);
            if (kind == svf)
                return (m0 * s * s + (m0 * k + m1) * s + (m0 + m2)) / (s * s + k * s + 1.0);
            return (m0 * (s + 1.0) + m1) / (s + 1.0);
        }
    };

    struct SectionState { double s1 = 0.0, s2 = 0.0; };

    inline float processSection (const Section& c, SectionState& st, float xin) noexcept
    {
        const double x = xin;
        if (c.kind == Section::svf)
        {
            const double v3 = x - st.s2;
            const double v1 = c.a1 * st.s1 + c.a2 * v3;
            const double v2 = st.s2 + c.a2 * st.s1 + c.a3 * v3;
            st.s1 = 2.0 * v1 - st.s1;
            st.s2 = 2.0 * v2 - st.s2;
            return (float) (c.m0 * x + c.m1 * v1 + c.m2 * v2);
        }
        const double v  = (x - st.s1) * c.G;
        const double lp = v + st.s1;
        st.s1 = lp + v;
        return (float) (c.m0 * x + c.m1 * lp);
    }

    //==========================================================================
    struct BandDesign
    {
        static constexpr int maxSections = 12;
        std::array<Section, maxSections> sec {};
        int num = 0;
        double outGain = 1.0;

        std::complex<double> response (double f, double fs) const
        {
            std::complex<double> h (outGain, 0.0);
            for (int i = 0; i < num; ++i)
                h *= sec[(size_t) i].response (f, fs);
            return h;
        }

        double magnitudeDb (double f, double fs) const
        {
            return 20.0 * std::log10 (juce::jmax (1.0e-9, std::abs (response (f, fs))));
        }
    };

    inline double prewarp (double f, double fs)
    {
        return std::tan (juce::MathConstants<double>::pi * juce::jlimit (1.0, fs * 0.4999, f) / fs);
    }

    /** Builds the section cascade for a band. gainDb already includes gain scale and dynamics. */
    inline void designBand (BandDesign& d, int type, double freq, double gainDb, double q, int slopeIdx, double fs)
    {
        d.num = 0;
        d.outGain = 1.0;
        q = juce::jlimit (0.025, 100.0, q);
        const double w = prewarp (freq, fs);

        auto add = [&d] (int kind, double g, double k, double m0, double m1, double m2)
        {
            auto& s = d.sec[(size_t) d.num++];
            s.kind = kind; s.g = g; s.k = k; s.m0 = m0; s.m1 = m1; s.m2 = m2;
            s.finalise();
        };

        switch (type)
        {
            case Bell:
            {
                const double A = std::pow (10.0, gainDb / 40.0);
                const double k = 1.0 / (q * A);
                add (Section::svf, w, k, 1.0, k * (A * A - 1.0), 0.0);
                break;
            }
            case LowShelf:
            {
                const double A = std::pow (10.0, gainDb / 40.0);
                const double k = 1.0 / q;
                add (Section::svf, w / std::sqrt (A), k, 1.0, k * (A - 1.0), A * A - 1.0);
                break;
            }
            case HighShelf:
            case TiltShelf:
            {
                const double A = std::pow (10.0, gainDb / 40.0);
                const double k = 1.0 / q;
                add (Section::svf, w * std::sqrt (A), k, A * A, k * (1.0 - A) * A, 1.0 - A * A);
                if (type == TiltShelf)
                    d.outGain = 1.0 / A; // -gain/2 below, +gain/2 above
                break;
            }
            case LowCut:
            case HighCut:
            {
                const bool hp = type == LowCut;
                const int order = slopeOrders[juce::jlimit (0, 8, slopeIdx)];
                const int pairs = order / 2;
                if (order % 2 == 1)
                    add (Section::onePole, w, 0.0, hp ? 1.0 : 0.0, hp ? -1.0 : 1.0, 0.0);

                // Butterworth pole Qs; the most resonant pair takes the user's Q.
                int maxIdx = -1; double maxQ = 0.0;
                std::array<double, 8> qs {};
                for (int i = 0; i < pairs; ++i)
                {
                    const double theta = (order % 2 == 0)
                        ? (2.0 * (i + 1) - 1.0) * juce::MathConstants<double>::pi / (2.0 * order)
                        : (i + 1) * juce::MathConstants<double>::pi / order;
                    qs[(size_t) i] = 1.0 / (2.0 * std::cos (theta));
                    if (qs[(size_t) i] > maxQ) { maxQ = qs[(size_t) i]; maxIdx = i; }
                }
                if (maxIdx >= 0)
                    qs[(size_t) maxIdx] *= q / 0.70710678;

                for (int i = 0; i < pairs; ++i)
                {
                    const double k = 1.0 / qs[(size_t) i];
                    if (hp) add (Section::svf, w, k, 1.0, -k, -1.0);
                    else    add (Section::svf, w, k, 0.0, 0.0, 1.0);
                }
                break;
            }
            case Notch:
            {
                const double k = 1.0 / q;
                add (Section::svf, w, k, 1.0, -k, 0.0);
                break;
            }
            case BandPass:
            {
                const double k = 1.0 / q;
                add (Section::svf, w, k, 0.0, k, 0.0);
                break;
            }
            case AllPass:
            {
                const double k = 1.0 / q;
                add (Section::svf, w, k, 1.0, -2.0 * k, 0.0);
                break;
            }
            case FlatTilt:
            {
                // Constant slope (gain / 10 dB per octave) built from first-order shelves one
                // octave apart, normalised to 0 dB at the pivot frequency.
                const double perOct = gainDb / 10.0;
                const double Gs = std::pow (10.0, perOct / 20.0);
                for (int i = 0; i < 11 && d.num < BandDesign::maxSections; ++i)
                {
                    const double fc = 20.0 * std::pow (2.0, (double) i);
                    if (fc > fs * 0.45)
                        break;
                    add (Section::onePole, prewarp (fc, fs) * std::sqrt (Gs), 0.0, Gs, 1.0 - Gs, 0.0);
                }
                const double atPivot = std::abs (d.response (juce::jlimit (20.0, fs * 0.45, freq), fs));
                d.outGain = atPivot > 1.0e-9 ? 1.0 / atPivot : 1.0;
                break;
            }
            default: break;
        }
    }

    /** Filter used to listen to a band (solo) and to detect its level (dynamic EQ). */
    inline void designListenFilter (Section& s, int type, double freq, double q, double fs)
    {
        const double w = prewarp (freq, fs);
        s.kind = Section::svf;
        s.g = w;
        switch (type)
        {
            case LowShelf: case LowCut:            // the region below the corner
                s.k = 1.4142; s.m0 = 0.0; s.m1 = 0.0; s.m2 = 1.0; break;
            case HighShelf: case HighCut: case TiltShelf:
                s.k = 1.4142; s.m0 = 1.0; s.m1 = -1.4142; s.m2 = -1.0; break;
            default:
            {
                const double k = 1.0 / juce::jlimit (0.3, 30.0, q);
                s.k = k; s.m0 = 0.0; s.m1 = k; s.m2 = 0.0; break;
            }
        }
        s.finalise();
    }
}
