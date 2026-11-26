// Pseudo-Stereo DSP core
//
// C++ port of "Pseudo-Stereo" (pseudostereo.ny), a Nyquist plug-in for Audacity
// by Steve Daulton (http://easyspacepro.com), June 2011, released under the
// GNU General Public License version 2.
//
// This header has no JUCE dependency so it can be unit-tested on its own.
//
// What the original does (per sample):
//   x      = chosen source channel (Left or Right)
//   common = lp(x,300) + lp(hp(x,600),1200) + hp(x,2400)       (same in both outputs)
//   L_wet  = common + delay(lp(hp(x,300),600),  0.20*df)
//                   + delay(lp(hp(x,1200),2400), 0.08*df)
//   R_wet  = common + delay(lp(hp(x,300),600),  0.14*df)
//                   + delay(lp(hp(x,1200),2400), 0.06*df)
//   outL   = mix*L_wet + (1-mix)*x
//   outR   = mix*R_wet + (1-mix)*x
// where lp/hp are Nyquist's first-order Butterworth filters (TONE / ATONE),
// df is the delay factor (0..1) and delays are in seconds.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace pseudostereo
{

// First-order low-pass exactly as Nyquist's TONE (-3 dB at the cutoff).
// Nyquist's high-pass (ATONE) with the same cutoff is exactly  x - lowpass(x).
class OnePole
{
public:
    void setCutoff (double hz, double sampleRate)
    {
        const double pi = 3.14159265358979323846;
        const double b  = 2.0 - std::cos (2.0 * pi * hz / sampleRate);
        c2 = b - std::sqrt (b * b - 1.0);
        c1 = 1.0 - c2;
    }

    void reset() { state = 0.0; }

    inline double lowpass (double x) noexcept
    {
        state = c1 * x + c2 * state;
        return state;
    }

private:
    double c1 = 1.0, c2 = 0.0, state = 0.0;
};

// Plain delay line with linear-interpolated fractional reads.
// push(x) then read(d) returns x delayed by d samples (d = 0 -> newest sample).
class DelayLine
{
public:
    void prepare (std::size_t maxDelaySamples)
    {
        std::size_t n = 8;
        while (n < maxDelaySamples + 4)
            n <<= 1;
        buffer.assign (n, 0.0);
        mask = n - 1;
        pos  = 0;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0);
        pos = 0;
    }

    inline void push (double x) noexcept
    {
        pos = (pos + 1) & mask;
        buffer[pos] = x;
    }

    inline double read (double delaySamples) const noexcept
    {
        const double maxD = (double) (mask - 2);
        delaySamples = std::min (std::max (delaySamples, 0.0), maxD);
        const auto   whole = (std::size_t) delaySamples;
        const double frac  = delaySamples - (double) whole;
        const double a = buffer[(pos - whole) & mask];
        const double b = buffer[(pos - whole - 1) & mask];
        return a + frac * (b - a);
    }

private:
    std::vector<double> buffer;
    std::size_t mask = 0, pos = 0;
};

class PseudoStereo
{
public:
    static constexpr double maxDelaySeconds = 0.2;   // 0.20 * df, df <= 1
    static constexpr double minSampleRate   = 4800;  // same limit as the .ny script

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        active = sampleRate >= minSampleRate;

        if (active)
        {
            const std::size_t maxSamples = (std::size_t) std::ceil (maxDelaySeconds * sampleRate) + 2;
            delayLow.prepare (maxSamples);
            delayHigh.prepare (maxSamples);

            lp300.setCutoff (300.0, sampleRate);
            lp600a.setCutoff (600.0, sampleRate);    // after hp300  (band 300-600)
            lp600b.setCutoff (600.0, sampleRate);    // on x         (for hp600)
            lp1200a.setCutoff (1200.0, sampleRate);  // after hp600  (band 600-1200)
            lp1200b.setCutoff (1200.0, sampleRate);  // on x         (for hp1200)
            lp2400a.setCutoff (2400.0, sampleRate);  // after hp1200 (band 1200-2400)
            lp2400b.setCutoff (2400.0, sampleRate);  // on x         (for hp2400)
        }

        smoothCoeff = 1.0 - std::exp (-1.0 / (0.03 * sampleRate)); // ~30 ms
        reset();
    }

    void reset()
    {
        for (auto* f : { &lp300, &lp600a, &lp600b, &lp1200a, &lp1200b, &lp2400a, &lp2400b })
            f->reset();
        delayLow.reset();
        delayHigh.reset();
        snapSmoothers = true;
    }

    // source: 0 = Left, 1 = Right.  delayPercent / mixPercent: 0..100.
    void setParameters (int source, double delayPercent, double mixPercent) noexcept
    {
        sourceChannel = source == 0 ? 0 : 1;
        targetDf  = std::min (1.0, std::max (0.0, delayPercent / 100.0));
        targetMix = std::min (1.0, std::max (0.0, mixPercent / 100.0));
        if (snapSmoothers)
        {
            df = targetDf;
            mix = targetMix;
            snapSmoothers = false;
        }
    }

    // In-place stereo processing.
    void process (float* left, float* right, int numSamples) noexcept
    {
        if (! active)
        {
            // Sample rate too low for the effect (the .ny script errors out): pass the source to both sides.
            for (int i = 0; i < numSamples; ++i)
            {
                const float x = sourceChannel == 0 ? left[i] : right[i];
                left[i] = right[i] = x;
            }
            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            df  += (targetDf  - df)  * smoothCoeff;
            mix += (targetMix - mix) * smoothCoeff;

            const double x = (double) (sourceChannel == 0 ? left[i] : right[i]);

            // Band split (all first-order, as in the original)
            const double f1    = lp300.lowpass (x);                          // lp(x,300)
            const double hp300 = x - f1;                                     // hp(x,300)
            const double band2 = lp600a.lowpass (hp300);                     // lp(hp(x,300),600)

            const double hp600 = x - lp600b.lowpass (x);                     // hp(x,600)
            const double f3    = lp1200a.lowpass (hp600);                    // lp(hp(x,600),1200)

            const double hp1200 = x - lp1200b.lowpass (x);                   // hp(x,1200)
            const double band4  = lp2400a.lowpass (hp1200);                  // lp(hp(x,1200),2400)

            const double f5 = x - lp2400b.lowpass (x);                       // hp(x,2400)

            delayLow.push (band2);
            delayHigh.push (band4);

            const double common = f1 + f3 + f5;
            const double s = sampleRate;
            const double wetL = common + delayLow.read (0.20 * df * s) + delayHigh.read (0.08 * df * s);
            const double wetR = common + delayLow.read (0.14 * df * s) + delayHigh.read (0.06 * df * s);

            left[i]  = (float) (mix * wetL + (1.0 - mix) * x);
            right[i] = (float) (mix * wetR + (1.0 - mix) * x);
        }
    }

private:
    double sampleRate = 44100.0;
    bool active = true, snapSmoothers = true;
    int sourceChannel = 0;
    double targetDf = 0.3, targetMix = 0.8, df = 0.3, mix = 0.8, smoothCoeff = 0.001;

    OnePole lp300, lp600a, lp600b, lp1200a, lp1200b, lp2400a, lp2400b;
    DelayLine delayLow, delayHigh;
};

} // namespace pseudostereo
