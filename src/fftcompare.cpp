#include "fftcompare.h"
#include <math.h>
#include <iostream>
#include "hash.h"
#include "log.h"

float KaiserBesselWindow(float dIn, float dSample, float dTotal)
{
    return (0.402-0.498 * cos(2*M_PI*dSample/dTotal)
                +0.098 * cos(4*M_PI*dSample/dTotal)
                +0.001 * cos(6*M_PI*dSample/dTotal))*dIn;
}

std::vector<float> ConvertToAmplitude(const std::vector<kiss_fft_cpx>& vfft_out)
{
    std::vector<float> vAmplitude(vfft_out.size());

    for(size_t i = 0; i < vfft_out.size(); i++)
    {
        float dAmplitude(sqrt( (vfft_out[i].r*vfft_out[i].r) + (vfft_out[i].i*vfft_out[i].i)));
        if(dAmplitude<0)
        {
            dAmplitude=-dAmplitude;
        }
        dAmplitude /= static_cast<float>(vfft_out.size());
        vAmplitude[i] = 20*log10(dAmplitude);
    }
}

std::pair<double, double> CalculateFundamentalFrequencyAmplitude(const std::vector<float>& vAmplitude, unsigned long nSampleRate)
{
    std::pair<double, double> fundamental = {0.0, -80.0};
    double dBinSize = static_cast<double>(nSampleRate)/static_cast<double>((vAmplitude.size()-1)*2);

    double dMaxBin(0);
    for(size_t i = 0; i < vAmplitude.size(); i++)
    {
        if(fundamental.second < vAmplitude[i])
        {
            fundamental.second = vAmplitude[i];
            dMaxBin = i;
        }
    }

    fundamental.first = dMaxBin*dBinSize;
    return fundamental;
}

void Normalize(std::vector<float>& vAmplitude, double dMax)
{
    for(auto& dAmp : vAmplitude)
    {
        dAmp -= dMax;
    }
}

std::vector<float> Minus(const std::vector<float>& vBufferA, const std::vector<float>& vBufferB)
{
    std::vector<float> vResult(vBufferA.size());
    for(size_t i = 0; i < vBufferA.size(); i++)
    {
        vResult[i] = vBufferA[i]-vBufferB[i];
    }
    return vResult;
}

std::vector<kiss_fft_cpx> DoFFT(std::vector<float>& vBuffer, unsigned long nSampleRate, unsigned int nBins)
{
    std::vector<kiss_fft_scalar> vfft_in((nBins-1)*2);
    std::vector<kiss_fft_cpx> vfft_out(nBins);

    for(size_t nSample = 0; nSample < vfft_in.size(); nSample++)
    {
        vfft_in[nSample] = KaiserBesselWindow(vBuffer[nSample], nSample, vfft_in.size()-1);
    }

    //Now do the check
    kiss_fftr_cfg cfg;

    if ((cfg = kiss_fftr_alloc(vfft_in.size(), 0, NULL, NULL)) != NULL)
    {
        kiss_fftr(cfg, vfft_in.data(), vfft_out.data());
    }
    free(cfg);


    return vfft_out;
}


hashresult CalculateFFT(const std::deque<float>& bufferA, const std::deque<float>& bufferB, size_t nSampleSize, bool bLocked)
{

    std::vector<float> vBufferA(std::begin(bufferA), std::end(bufferA));
    std::vector<float> vBufferB(std::begin(bufferB), std::end(bufferB));


    pmlLog(pml::LOG_DEBUG) << "CalculateFFT\tCheck if tone";
    if(CheckForTone(vBufferA, vBufferB))
    {
        pmlLog(pml::LOG_DEBUG) << "CalculateFFT\tTONE";
        return std::make_pair(0, 1.0);
    }

    hashresult result = std::make_pair(0,-1.0);

    pmlLog(pml::LOG_DEBUG) << "CalculateFFT\tGet Offset: Window size=" << nSampleSize;


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

    pmlLog(pml::LOG_DEBUG) << "CalculateFFT\tOffsetA=" << nOffsetA << "\tOffsetB=" << nOffsetB;


    pmlLog(pml::LOG_DEBUG) << "CalculateFFT\tStarting FFT Check";

    if(nOffsetA+nSampleSize <= vBufferA.size() && nOffsetB+nSampleSize <= vBufferB.size())
    {
        int nSamplesA(std::min(vBufferA.size()-nOffsetA, nSampleSize));
        int nSamplesB(std::min(vBufferB.size()-nOffsetB, nSampleSize));
        int nSamples(std::min(nSamplesA, nSamplesB));

        pmlLog(pml::LOG_DEBUG) << "CalculateFFT\tComparing "<< nSamples << " samples [" << nSamplesA << "," << nSamplesB << "]";

        if(nSamples >= nSampleSize)
        {
            std::vector<float> vTempA(nSamples);
            std::vector<float> vTempB(nSamples);

            for(int i = 0; i < nSamples; i++)
            {
                vTempA[i] = vBufferA[i+nOffsetA];
                vTempB[i] = vBufferB[i+nOffsetB];
            }

            auto vAmpLeft = ConvertToAmplitude(DoFFT(vTempA, 48000, 1024));
            auto vAmpRight = ConvertToAmplitude(DoFFT(vTempB, 48000, 1024));

            auto fundLeft = CalculateFundamentalFrequencyAmplitude(vAmpLeft, 48000);
            auto fundRight = CalculateFundamentalFrequencyAmplitude(vAmpRight, 48000);

            Normalize(vAmpLeft, fundLeft.second);
            Normalize(vAmpRight, fundRight.second);

            auto vMinus = Minus(vAmpLeft, vAmpRight);

            auto log  = pmlLog();
            for(size_t i = 0; i < vMinus.size(); ++i)
            {
                log << vMinus[i] << ",";
            }

        }
        else
        {
            pmlLog(pml::LOG_WARN) << "CalculateFFT\tSample size too small for offset: Sample Size: " << nSampleSize << ", OffsetA " <<  nOffsetA
            << ", BufferA " << vBufferA.size() << ", OffsetB " << nOffsetB << ", BufferB " << vBufferB.size();
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN) << "CalculateFFT\tSample size too small for offset: Sample Size: " << nSampleSize << ", OffsetA " <<  nOffsetA
            << ", BufferA " << vBufferA.size() << ", OffsetB " << nOffsetB << ", BufferB " << vBufferB.size();
    }

    pmlLog(pml::LOG_DEBUG) << "CalculateFFT\tFFT Check Finished: Confidence: " << result.second;

    return result;

}

