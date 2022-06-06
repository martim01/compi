#include "fftcompare.h"
#include <math.h>
#include <iostream>
#include "hash.h"
#include "log.h"
#include <algorithm>

double g_conf = 0.0;

void Normalize(std::vector<float>& vAmplitude, double dMax)
{
    if(dMax == 0.0)
    {
        return;
    }

    for(auto& dAmp : vAmplitude)
    {
        dAmp/=dMax;
    }
}

std::vector<float> Minus(const std::vector<float>& vBufferA, const std::vector<float>& vBufferB)
{
    std::vector<float> vResult(vBufferA.size());
    for(size_t i = 0; i < vBufferA.size(); i++)
    {
        vResult[i] = abs(vBufferA[i]-vBufferB[i]);

    }
    return vResult;
}


hashresult CalculateMinus(const std::deque<float>& bufferA, const std::deque<float>& bufferB, const peak& thePeak, size_t nSampleSize, bool bLocked, const std::pair<int, double>& last)
{

    std::vector<float> vBufferA(std::begin(bufferA), std::end(bufferA));
    std::vector<float> vBufferB(std::begin(bufferB), std::end(bufferB));


    pmlLog(pml::LOG_DEBUG) << "CalculateMinus\tCheck if tone";
    if(CheckForTone(vBufferA, vBufferB))
    {
        pmlLog(pml::LOG_DEBUG) << "CalculateMinus\tTONE";
        g_conf = std::min(g_conf+0.1, 1.0);
        return std::make_pair(0, g_conf);
    }

    hashresult result = std::make_pair(0,-1.0);


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

    pmlLog(pml::LOG_DEBUG) << "CalculateMinus\tOffsetA=" << nOffsetA << "\tOffsetB=" << nOffsetB;



    if(nOffsetA+nSampleSize <= vBufferA.size() && nOffsetB+nSampleSize <= vBufferB.size())
    {
        int nSamplesA(std::min(vBufferA.size()-nOffsetA, nSampleSize));
        int nSamplesB(std::min(vBufferB.size()-nOffsetB, nSampleSize));
        int nSamples(std::min(nSamplesA, nSamplesB));



        if(nSamples >= nSampleSize)
        {
            std::vector<float> vTempA(nSamples);
            std::vector<float> vTempB(nSamples);

            for(int i = 0; i < nSamples; i++)
            {
                vTempA[i] = vBufferA[i+nOffsetA];
                vTempB[i] = vBufferB[i+nOffsetB];
            }

            pmlLog(pml::LOG_DEBUG) << "CalculateMinus\tComparing "<< nSamples << " samples [" << nSamplesA << "," << nSamplesB << "]";

            auto vMinus = Minus(vTempA, vTempB);
            auto itMax = std::max_element(vMinus.begin(), vMinus.end());
            auto diff = std::max(thePeak.first, thePeak.second)/(*itMax);

            if(diff > 100 || (*itMax) < 0.0005) //ignore when less than 75dB
            {
                g_conf = std::min(g_conf+0.1, 1.0);
                result.second = g_conf;
                pmlLog(pml::LOG_DEBUG) << "CalculateMinus\tSAME\tMax difference = " << diff << "\t" << std::max(thePeak.first, thePeak.second) << ":" << (*itMax);
            }
            else
            {
                g_conf = std::max(g_conf-0.1, 0.0);
                result.second = g_conf;
                pmlLog(pml::LOG_DEBUG) << "CalculateMinus\tDIFF\tMax difference = " << diff << "\t" << std::max(thePeak.first, thePeak.second) << ":" << (*itMax);
            }
        }
        else
        {
            pmlLog(pml::LOG_WARN) << "CalculateMinus\tSample size too small for offset: Sample Size: " << nSampleSize << ", OffsetA " <<  nOffsetA
            << ", BufferA " << vBufferA.size() << ", OffsetB " << nOffsetB << ", BufferB " << vBufferB.size();
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN) << "CalculateMinus\tSample size too small for offset: Sample Size: " << nSampleSize << ", OffsetA " <<  nOffsetA
            << ", BufferA " << vBufferA.size() << ", OffsetB " << nOffsetB << ", BufferB " << vBufferB.size();
    }

    pmlLog(pml::LOG_DEBUG) << "CalculateMinus\tMinus Check Finished: Confidence: " << result.second;

    return result;

}

