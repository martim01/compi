#include "hash.h"
#include "audiophash.h"
#include <thread>
#include <iomanip>
#include "log.h"
#include "kiss_xcorr.h"

const unsigned long SAMPLE_RATE = 48000;




hashresult CalculateHash(const std::deque<float>& bufferA, const std::deque<float>& bufferB, size_t nSampleSize, bool bLocked)
{

    std::vector<float> vBufferA(std::begin(bufferA), std::end(bufferA));
    std::vector<float> vBufferB(std::begin(bufferB), std::end(bufferB));


    pml::Log::Get(pml::Log::LOG_DEBUG) << "CalculateHash\tCheck if tone" << std::endl;
    if(CheckForTone(vBufferA, vBufferB))
    {
        pml::Log::Get(pml::Log::LOG_DEBUG) << "CalculateHash\tTONE" << std::endl;
        return std::make_pair(0, 1.0);
    }

    hashresult result = std::make_pair(0,-1.0);

    pml::Log::Get(pml::Log::LOG_DEBUG) << "CalculateHash\tGet Offset: Window size=" << nSampleSize << std::endl;


    size_t nOffsetA(0);
    size_t nOffsetB(0);

    //if(!bLocked)
    {
        result.first = CalculateOffset(vBufferA, vBufferB);
        if(result.first < 0)
        {
            nOffsetB = static_cast<size_t>(-result.first);
        }
        else
        {
            nOffsetA = static_cast<size_t>(result.first);
        }

        pml::Log::Get(pml::Log::LOG_DEBUG) << "CalculateHash\tOffsetA=" << nOffsetA << "\tOffsetB=" << nOffsetB << std::endl;
    }


    pml::Log::Get(pml::Log::LOG_DEBUG) << "CalculateHash\tStarting Hash Check" << std::endl;

    if(nOffsetA+nSampleSize <= vBufferA.size() && nOffsetB+nSampleSize <= vBufferB.size())
    {
        int nSamplesA(std::min(vBufferA.size()-nOffsetA, nSampleSize));
        int nSamplesB(std::min(vBufferB.size()-nOffsetB, nSampleSize));
        int nSamples(std::min(nSamplesA, nSamplesB));

        pml::Log::Get(pml::Log::LOG_DEBUG) << "CalculateHash\tComparing "<< nSamples << " samples [" << nSamplesA << "," << nSamplesB << "]" << std::endl;

        if(nSamples > 0)
        {

            int nHashA;
            int nHashB;

            //get the hash numbers ofr left and right channels

//            std::vector<float> vTempA(vBufferA.begin()+nOffsetA, vBufferA.begin()+nOffsetA+nSamples);
//            std::vector<float> vTempB(vBufferB.begin()+nOffsetB, vBufferB.begin()+nOffsetB+nSamples);

            //copy and check for silence...
            std::vector<float> vTempA(nSamples);
            std::vector<float> vTempB(nSamples);

            for(int i = 0; i < nSamples; i++)
            {
                vTempA[i] = vBufferA[i+nOffsetA];
                vTempB[i] = vBufferB[i+nOffsetB];
            }

            uint32_t* pHashA(ph_audiohash(vTempA.data(), vTempA.size(), SAMPLE_RATE, nHashA));
            uint32_t* pHashB(ph_audiohash(vTempB.data(), vTempB.size(), SAMPLE_RATE, nHashB));

            if(pHashB && pHashA && nHashA > 0 && nHashB > 0)
            {
                int nConfidenceLength;
                int nFrames = std::min(nHashA, nHashB);
                pml::Log::Get(pml::Log::LOG_DEBUG) << "CalculateHash\tHash size: A=" << nHashA << "\tB=" << nHashB << std::endl;
                double* pResult =ph_audio_distance_ber(pHashB, nHashB, pHashA, nHashA, 0.30, nFrames, nConfidenceLength);

                for (int i=0;i<nConfidenceLength;i++)
                {
                    if (pResult[i] > result.second)
                    {
                        result.second = pResult[i];
                    }
                }
                free(pResult);
                free(pHashB);
                free(pHashA);

            }
        }
        else
        {
            pml::Log::Get(pml::Log::LOG_WARN) << "CalculateHash\tSample size too small for offset: Sample Size: " << nSampleSize << ", OffsetA " <<  nOffsetA
            << ", BufferA " << vBufferA.size() << ", OffsetB " << nOffsetB << ", BufferB " << vBufferB.size() << std::endl;
        }
    }
    else
    {
        pml::Log::Get(pml::Log::LOG_WARN) << "CalculateHash\tSample size too small for offset: Sample Size: " << nSampleSize << ", OffsetA " <<  nOffsetA
            << ", BufferA " << vBufferA.size() << ", OffsetB " << nOffsetB << ", BufferB " << vBufferB.size() << std::endl;
    }

    pml::Log::Get(pml::Log::LOG_DEBUG) << "CalculateHash\tHash Check Finished: Confidence: " << result.second << std::endl;

    return result;

}


void WindowData(std::vector<float>& vData)
{
	double angle;
	for(size_t i = 0; i < vData.size(); i++)
    {
        angle = (2*M_PI)*i/(vData.size()-1);
        vData[i] *= 0.5*(1.0 - cos(angle));
    }

}


int CalculateOffset(std::vector<float> vBufferA, std::vector<float> vBufferB)
{

    WindowData(vBufferA);
    WindowData(vBufferB);

    std::vector<float> vfft_out;
    vfft_out.resize(vBufferA.size());

    rfft_xcorr(vBufferA.size(), vBufferA.data(), vBufferB.data(), vfft_out.data(),KISS_XCORR);



    long pos_peak_pos = 0;
    long neg_peak_pos = 0;
    double biggest = vfft_out[0];
    double smallest = vfft_out[0];

    // now search for positive and negative peaks in correlation function
    for(unsigned int n=0; n<vfft_out.size(); n++)
    {
        if(vfft_out[n] > biggest)
        {
            biggest = vfft_out[n];
            pos_peak_pos = n;
        }
        if(vfft_out[n] < smallest)
        {
            smallest = vfft_out[n];
            neg_peak_pos = n;
        }
    }


    size_t nBlockSize = vBufferA.size();

    int offset =  ((biggest < fabs(smallest)) ?  neg_peak_pos : pos_peak_pos) ;
    if ((size_t)offset > nBlockSize/2)
    {
        offset = offset - nBlockSize;
    }

    pml::Log::Get(pml::Log::LOG_DEBUG) << "CalculateHash\tOffset=" << offset << " samples" << std::endl;

    return offset;
}



std::map<int, double> GetPeaks(const std::vector<kiss_fft_cpx>& vfft_out)
{

    std::map<int, double> mPeaks;

    double dLastBin(-800);
    int nPeaks(0);
    bool bDown(false);

    for(size_t i = 0; i < vfft_out.size(); i++)
    {
        float dAmplitude(sqrt( (vfft_out[i].r*vfft_out[i].r) + (vfft_out[i].i*vfft_out[i].i)));
        if(dAmplitude<0)
        {
            dAmplitude=-dAmplitude;
        }
        dAmplitude /= static_cast<float>(vfft_out.size());
        double dLog = 20*log10(dAmplitude);


        if(dLog < dLastBin)
        {
            if(bDown == false)  //we were going up, now we are going down so the last bin must be a peak
            {
                if(dLog > -80)
                {
                    mPeaks.insert(make_pair(i-1, dLastBin));
                }
            }
            bDown = true;
        }
        else
        {
            bDown = false;
        }

        dLastBin = dLog;
    }

    return mPeaks;

}


float HannWindow(float dIn, size_t nSample, size_t nSize)
{
    return 0.5*(1-cos((2*M_PI*static_cast<float>(nSample))/static_cast<float>(nSize-1)))*dIn;
}

bool CheckForTone(std::vector<float> vBufferA, std::vector<float> vBufferB)
{
    if(vBufferA.size() < 2046 || vBufferB.size() < 2046)
    {
        pml::Log::Get(pml::Log::LOG_WARN) << "CalculateHash\tNo enough samples to check tone" << std::endl;
        return false;
    }

    size_t nBins = 1024;
    std::vector<kiss_fft_scalar> vfftL((nBins-1*2));
    std::vector<kiss_fft_scalar> vfftR((nBins-1*2));

    std::vector<kiss_fft_cpx> vfft_outL(nBins);
    std::vector<kiss_fft_cpx> vfft_outR(nBins);


    for(size_t i = 0; i < vfftL.size(); i++)
    {
        vfftL[i] = HannWindow(vBufferA[i], i, vfftL.size());
        vfftR[i] = HannWindow(vBufferB[i], i, vfftL.size());
    }

    //Now do the check
    kiss_fftr_cfg cfgL;
    kiss_fftr_cfg cfgR;


    if ((cfgL = kiss_fftr_alloc(vfftL.size(), 0, NULL, NULL)) != NULL)
    {
        kiss_fftr(cfgL, vfftL.data(), vfft_outL.data());
    }
    free(cfgL);

    if ((cfgR = kiss_fftr_alloc(vfftR.size(), 0, NULL, NULL)) != NULL)
    {
        kiss_fftr(cfgR, vfftR.data(), vfft_outR.data());
    }
    free(cfgR);


    std::map<int, double> mPeaksL(GetPeaks(vfft_outL));
    std::map<int, double> mPeaksR(GetPeaks(vfft_outR));

    if(mPeaksL.size() != 1 || mPeaksR.size() != 1)
    {
        pml::Log::Get(pml::Log::LOG_DEBUG) << "CalculateHash\tCheckForTone - " << mPeaksL.size() << ", " << mPeaksR.size() << std::endl;
        return false;
    }

    float dMaxL(-120.0);
    float dMaxR(-120.0);
    int nMaxL, nMaxR;

    for(auto pairPeak : mPeaksL)
    {
        if(pairPeak.second > dMaxL)
        {
            nMaxL = pairPeak.first;
        }
    }

    for(auto pairPeak : mPeaksR)
    {
        if(pairPeak.second > dMaxR)
        {
            nMaxR = pairPeak.first;
        }
    }

    return (nMaxL == nMaxR);
}


