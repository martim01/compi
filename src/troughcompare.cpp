#include "troughcompare.h"
#include "hash.h"
#include <math.h>
#include <iostream>
#include "log.h"
#include "kiss_fft.h"
#include "kiss_fftr.h"
#include <algorithm>


double g_confFFT = 0.0;

std::vector<kiss_fft_cpx> DoFFT(std::vector<float>& buffer, unsigned int nBins)

{
    std::vector<kiss_fft_scalar> vfft_in((nBins-1)*2);
    std::vector<kiss_fft_cpx> vfft_out;
    vfft_out.resize(nBins);

    pmlLog(pml::LOG_TRACE) << "CalculateTroughs\tCopy Samples";

    for(size_t i = 0; i < vfft_in.size(); i++)
    {
        vfft_in[i] = HannWindow(buffer[i], i, vfft_in.size());
    }

    //Now do the check
    kiss_fftr_cfg cfg;
    pmlLog(pml::LOG_TRACE) << "CalculateTroughs\tFFT";

    if ((cfg = kiss_fftr_alloc(vfft_in.size(), 0, NULL, NULL)) != NULL)
    {
        kiss_fftr(cfg, vfft_in.data(), vfft_out.data());
    }
    free(cfg);

    return vfft_out;

}


std::vector<std::pair<size_t, float>> GetSpectrumDiff(std::vector<float>& bufferA, std::vector<float>& bufferB, unsigned long nSampleRate,unsigned int nBins)
{
    auto vfft_outA = DoFFT(bufferA, nBins);
    auto vfft_outB = DoFFT(bufferB, nBins);


    double dBinSize = static_cast<double>(nSampleRate)/static_cast<double>((vfft_outA.size()-1)*2);
    int nBinEnd = 12000/dBinSize;


    std::vector<std::pair<size_t, float>> vSpectrum;
    vSpectrum.reserve(vfft_outA.size());

    for(size_t i = 0; i < nBinEnd; i++)
    {
        auto dAmplitudeA = abs(sqrt( (vfft_outA[i].r*vfft_outA[i].r) + (vfft_outA[i].i*vfft_outA[i].i)))*2.0;
        dAmplitudeA /= static_cast<float>(vfft_outA.size());
        auto dLogA = 20*log10(dAmplitudeA);

        auto dAmplitudeB = abs(sqrt( (vfft_outB[i].r*vfft_outB[i].r) + (vfft_outB[i].i*vfft_outB[i].i)))*2.0;
        dAmplitudeB /= static_cast<float>(vfft_outB.size());
        auto dLogB = 20*log10(dAmplitudeB);

        auto dDiff = abs(-dLogA+dLogB);
        if(dDiff > 3.0 && dLogA > -80.0)
        {
            vSpectrum.push_back({i, dDiff});
        }
    }
    return vSpectrum;
}

std::vector<std::pair<int, double>>  GetTroughs(std::vector<float>& buffer, unsigned long nSampleRate, unsigned int nBins)
{
    pmlLog(pml::LOG_TRACE) << "CalculateTroughs\GetTroughs";
    auto vfft_out = DoFFT(buffer, nBins);
    pmlLog(pml::LOG_TRACE) << "CalculateTroughs\GetTroughs: FFT Done";


    double dBinSize = static_cast<double>(nSampleRate)/static_cast<double>((vfft_out.size()-1)*2);
    int nBinEnd = 12000/dBinSize;

    std::vector<std::pair<int, double>> vSort;//(vfft_out.size());
    vSort.reserve(vfft_out.size());

    double dLastBin(0);
    float dLastPeak(-90.0);
    int nPeaks(0);
    bool bUp(false);

    for(size_t i = 0; i <= nBinEnd; i++)     //ignore abouve 12kHz
    {
        auto dAmplitude = abs(sqrt( (vfft_out[i].r*vfft_out[i].r) + (vfft_out[i].i*vfft_out[i].i)));
        dAmplitude /= static_cast<float>(vfft_out.size());
        auto dLog = 20*log10(dAmplitude);

        dLastPeak = std::max(dLog, dLastPeak);

        if(dLog > dLastBin)
        {
            if(bUp == false)  //we were going down, now we are going up so the last bin must be a trough
            {
                if(i > 1 && dLastBin > -60.0)
                {
                    vSort.push_back(std::make_pair(i, dLastBin));
                }
            }
            bUp = true;
        }
        else
        {
            bUp = false;
        }

        dLastBin = dLog;
    }


    std::sort(vSort.begin(), vSort.end(), [](const std::pair<int, double>& valueA, const std::pair<int, double>& valueB){ return valueA.second < valueB.second;});


    return vSort;

//
//
//    pmlLog() << dBinSize;
//

//    return mTroughs;
}

hashresult CalculateFFTDiff(const std::deque<float>& bufferA, const std::deque<float>& bufferB, unsigned int nSampleSize, size_t nBands, double dChangeDown, double dChangeUp)
{
    std::vector<float> vBufferA(std::begin(bufferA), std::end(bufferA));
    std::vector<float> vBufferB(std::begin(bufferB), std::end(bufferB));

    unsigned int nBins = 512;


//    pmlLog(pml::LOG_TRACE) << "CalculateTroughs\tCheck if tone";
//    if(CheckForTone(vBufferA, vBufferB))
//    {
//        pmlLog(pml::LOG_TRACE) << "CalculateTroughs\tTONE";
//        return std::make_pair(0, 1.0);
//    }

    hashresult result = std::make_pair(0,-1.0);

    pmlLog(pml::LOG_TRACE) << "CalculateFFTDiff\tGet Offset: Window size=" << nSampleSize;


    size_t nOffsetA(0);
    size_t nOffsetB(0);

    result.first = CalculateOffset(vBufferA, vBufferB);
    if(result.first < 0)
    {
        nOffsetB = static_cast<size_t>(-result.first);
    }
    else
    {
        nOffsetA = static_cast<size_t>(result.first);
    }

    pmlLog(pml::LOG_TRACE) << "CalculateFFTDiff\tOffsetA=" << nOffsetA << "\tOffsetB=" << nOffsetB;


    if(nOffsetA+nSampleSize <= vBufferA.size() && nOffsetB+nSampleSize <= vBufferB.size())
    {
        int nSamplesA(std::min(vBufferA.size()-nOffsetA, nSampleSize));
        int nSamplesB(std::min(vBufferB.size()-nOffsetB, nSampleSize));
        int nSamples(std::min(nSamplesA, nSamplesB));

        size_t nWindow = nBins*2;
        pmlLog(pml::LOG_TRACE) << "CalculateFFTDiff\tComparing "<< nSamples << " samples [" << nSamplesA << "," << nSamplesB << "]";

        if(nSamples > 0)
        {
            pmlLog(pml::LOG_TRACE) << "CalculateFFTDiff\tCreateTemp";
            //copy and check for silence...
            std::vector<float> vTempA(nWindow, 0.0);
            std::vector<float> vTempB(nWindow, 0.0);

            //Get average level over the time
            for(int i = 0; i < nSamples; i++)
            {
                vTempA[i%nWindow] += vBufferA[i+nOffsetA];
                vTempB[i%nWindow] += vBufferB[i+nOffsetB];
            }

            for(size_t i = 0; i < nWindow; i++)
            {
                vTempA[i] /= (nSamples/nWindow);
                vTempB[i] /= (nSamples/nWindow);
            }


            auto vDiff = GetSpectrumDiff(vTempA, vTempB, 48000, nBins);
            pmlLog(pml::LOG_DEBUG) << "BANDS DIFF: " << vDiff.size();

            if(vDiff.size() < nBands)
            {
                g_confFFT = std::min(g_confFFT+dChangeUp, 1.0);
                result.second = g_confFFT;
            }
            else
            {
                g_confFFT = std::max(g_confFFT-dChangeDown, 0.0);
                result.second = g_confFFT;
            }


        }
    }
    return result;
}
