#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

// Pitch + formant shifter (STFT phase vocoder with cepstral envelope).
//  1. The spectral envelope (formants) is estimated by cepstral smoothing.
//  2. The spectrum is "flattened" (divided by the envelope) and pitch-shifted
//     with a phase vocoder -> pitch moves, formants stay put.
//  3. The envelope is re-applied, resampled by the formant ratio.
class PitchFormantShifter
{
public:
    static constexpr int order   = 11;
    static constexpr int N       = 1 << order;   // 2048
    static constexpr int overlap = 4;
    static constexpr int hop     = N / overlap;  // 512
    static constexpr int bins    = N / 2 + 1;
    static constexpr int latency = N;            // samples

    PitchFormantShifter()
        : fft (order),
          window ((size_t) N, juce::dsp::WindowingFunction<float>::hann, false)
    {
        // Calibrate the FFT round-trip gain so we do not depend on JUCE's scaling convention.
        std::vector<float> t ((size_t) 2 * N, 0.0f);
        t[0] = 1.0f;
        fft.performRealOnlyForwardTransform (t.data());
        fft.performRealOnlyInverseTransform (t.data());
        fftScale = std::abs (t[0]) > 1.0e-6f ? 1.0f / t[0] : 1.0f;

        data.assign ((size_t) 2 * N, 0.f);
        work.assign ((size_t) 2 * N, 0.f);
        cepLift.assign ((size_t) N, 0.f);
        inFifo.assign ((size_t) N, 0.f);
        outFifo.assign ((size_t) hop, 0.f);
        outAccum.assign ((size_t) 2 * N, 0.f);
        for (auto* v : { &mag, &phase, &lg, &env, &magf, &trueBin, &synMag, &synBin, &lastPhase, &sumPhase })
            v->assign ((size_t) bins, 0.f);
        reset();
    }

    void prepare (double sampleRate)
    {
        lifter = std::max (8, std::min (200, (int) std::lround (sampleRate / 1000.0)));
        reset();
    }

    void reset()
    {
        std::fill (inFifo.begin(), inFifo.end(), 0.f);
        std::fill (outFifo.begin(), outFifo.end(), 0.f);
        std::fill (outAccum.begin(), outAccum.end(), 0.f);
        std::fill (lastPhase.begin(), lastPhase.end(), 0.f);
        std::fill (sumPhase.begin(), sumPhase.end(), 0.f);
        rover = N - hop;
    }

    // One sample in, one sample out (delayed by `latency`).
    float process (float x, float pitchRatio, float formantRatio)
    {
        inFifo[(size_t) rover] = x;
        const float y = outFifo[(size_t) (rover - (N - hop))];
        ++rover;
        if (rover >= N)
        {
            rover = N - hop;
            processFrame (pitchRatio, formantRatio);
        }
        return y;
    }

private:
    static float wrapPi (float x)
    {
        const float twoPi = juce::MathConstants<float>::twoPi;
        return x - twoPi * std::round (x / twoPi);
    }

    void processFrame (float pitchRatio, float formantRatio)
    {
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float expct = twoPi * (float) hop / (float) N;

        // --- analysis ---
        for (int i = 0; i < N; ++i) data[(size_t) i] = inFifo[(size_t) i];
        std::fill (data.begin() + N, data.end(), 0.f);
        window.multiplyWithWindowingTable (data.data(), (size_t) N);
        fft.performRealOnlyForwardTransform (data.data(), true);

        for (int k = 0; k < bins; ++k)
        {
            const float re = data[(size_t) 2 * k], im = data[(size_t) 2 * k + 1];
            mag[(size_t) k]   = std::sqrt (re * re + im * im);
            phase[(size_t) k] = std::atan2 (im, re);
            lg[(size_t) k]    = std::log (mag[(size_t) k] + 1.0e-6f);
        }

        // --- spectral envelope by cepstral smoothing ---
        for (int k = 0; k < bins; ++k)  work[(size_t) k] = lg[(size_t) k];
        for (int k = bins; k < N; ++k)  work[(size_t) k] = lg[(size_t) (N - k)];
        std::fill (work.begin() + N, work.end(), 0.f);
        fft.performRealOnlyForwardTransform (work.data(), true);

        std::fill (cepLift.begin(), cepLift.end(), 0.f);
        cepLift[0] = work[0];
        for (int q = 1; q <= lifter; ++q)
        {
            cepLift[(size_t) q]       = work[(size_t) 2 * q];
            cepLift[(size_t) (N - q)] = work[(size_t) 2 * q];
        }
        for (int i = 0; i < N; ++i) work[(size_t) i] = cepLift[(size_t) i];
        std::fill (work.begin() + N, work.end(), 0.f);
        fft.performRealOnlyForwardTransform (work.data(), true);
        for (int k = 0; k < bins; ++k)
        {
            env[(size_t) k]  = work[(size_t) 2 * k] / (float) N;               // log envelope
            magf[(size_t) k] = mag[(size_t) k] * std::exp (-env[(size_t) k]);  // flattened magnitude
        }

        // --- phase-vocoder analysis (true bin frequency) ---
        for (int k = 0; k < bins; ++k)
        {
            float d = phase[(size_t) k] - lastPhase[(size_t) k];
            lastPhase[(size_t) k] = phase[(size_t) k];
            d = wrapPi (d - (float) k * expct);
            trueBin[(size_t) k] = (float) k + d * (float) overlap / twoPi;
        }

        // --- pitch shift on the flattened spectrum (inverse mapping: no holes, no merged bins) ---
        for (int j = 0; j < bins; ++j)
        {
            const float src = (float) j / pitchRatio;
            const int i0 = (int) std::floor (src);
            if (i0 >= bins - 1) { synMag[(size_t) j] = 0.f; synBin[(size_t) j] = (float) j; continue; }
            const float f = src - (float) i0;
            synMag[(size_t) j] = magf[(size_t) i0] * (1.f - f) + magf[(size_t) (i0 + 1)] * f;
            const int nearest = std::min (bins - 1, (int) std::lround (src));
            synBin[(size_t) j] = trueBin[(size_t) nearest] * pitchRatio;
        }

        // --- re-apply envelope shifted by the formant ratio + synthesis ---
        for (int k = 0; k < bins; ++k)
        {
            const float src = (float) k / formantRatio;
            float e;
            const int i0 = (int) std::floor (src);
            if (i0 >= bins - 1) e = env[(size_t) (bins - 1)];
            else
            {
                const float f = src - (float) i0;
                e = env[(size_t) i0] * (1.f - f) + env[(size_t) (i0 + 1)] * f;
            }
            const float m = synMag[(size_t) k] * std::exp (e);

            const float dev = synBin[(size_t) k] - (float) k;
            float sp = sumPhase[(size_t) k] + twoPi * dev / (float) overlap + (float) k * expct;
            sp = wrapPi (sp);
            sumPhase[(size_t) k] = sp;

            data[(size_t) 2 * k]     = m * std::cos (sp);
            data[(size_t) 2 * k + 1] = m * std::sin (sp);
        }
        data[1] = 0.f;
        data[(size_t) N + 1] = 0.f;

        fft.performRealOnlyInverseTransform (data.data());
        window.multiplyWithWindowingTable (data.data(), (size_t) N);

        const float g = fftScale / 1.5f;   // Hann^2 overlap-add gain at 75% overlap
        for (int i = 0; i < N; ++i) outAccum[(size_t) i] += data[(size_t) i] * g;

        for (int i = 0; i < hop; ++i) outFifo[(size_t) i] = outAccum[(size_t) i];
        std::memmove (outAccum.data(), outAccum.data() + hop, (size_t) N * sizeof (float));
        std::memmove (inFifo.data(), inFifo.data() + hop, (size_t) (N - hop) * sizeof (float));
    }

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    float fftScale = 1.0f;
    int lifter = 44, rover = N - hop;

    std::vector<float> data, work, cepLift, inFifo, outFifo, outAccum;
    std::vector<float> mag, phase, lg, env, magf, trueBin, synMag, synBin, lastPhase, sumPhase;
};
