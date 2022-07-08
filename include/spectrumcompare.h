#pragma once
#include "kiss_fft.h"
#include "kiss_fftr.h"
#include "hash.h"
#include <list>
#include <vector>
#include <deque>

using nonInterlacedList = std::pair<std::list<float>, std::list<float>>;
using nonInterlacedVector = std::pair<std::vector<float>, std::vector<float>>;
using nonInterlacedFFT = std::pair<std::vector<kiss_fft_cpx>, std::vector<kiss_fft_cpx>>;

class SpectrumCompare
{
    public:
        SpectrumCompare(unsigned long nSampleRate,unsigned long nFramesForGood, unsigned long nFramesForCurrent, double dMaxAllowedLevelDiff, unsigned long nMaxAllowedBandsDiff);
        ~SpectrumCompare();

        hashresult AddAudio(const std::deque<float>& bufferA, const std::deque<float>& bufferB);

    private:

        void ProcessAudio();
        void CalculateChannelOffset();
        void CalculateSpectrum(std::vector<float>& vSpectrum);
        void FFT(std::list<float>& buffer, std::vector<kiss_fft_cpx>& vOut);
        unsigned long CompareSpectrums();

        void CheckGoLive();

        bool m_bOneShot = true;
        bool m_bCalculated = false;
        unsigned long m_nWindowSamples = 12000;
        unsigned long m_nAccuracy = 192;
        unsigned long m_nSampleRate = 48000;
        double m_dTotalFrames = 0.0;

        double m_dFramesForGood = 5000.0;
        double m_dFramesForCurrent = 5000.0;


        bool m_bLive = false;
        double m_dMaxLevel = 3.0;
        unsigned long m_nMaxBands = 20;


        std::vector<float> m_vSpectrumGood;
        std::vector<float> m_vSpectrumCurrent;

        nonInterlacedFFT m_fftOut;

        nonInterlacedList m_Buffer;

        hashresult m_result;
};
