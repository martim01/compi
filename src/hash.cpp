#include "hash.h"
#include "audiophash.h"
#include <thread>
#include <iomanip>
#include "log.h"
#include "kiss_xcorr.h"

const unsigned long SAMPLE_RATE = 48000;




hashresult CalculateHash(const std::deque<float>& bufferA, const std::deque<float>& bufferB, size_t nSampleSize, bool bLocked)
{
    pml::Log::Get(pml::Log::LOG_DEBUG) << "CalculateHash\tGet Offset: Window size=" << nSampleSize << std::endl;

    hashresult result = std::make_pair(0,-1.0);


    std::vector<float> vBufferA(std::begin(bufferA), std::end(bufferA));
    std::vector<float> vBufferB(std::begin(bufferB), std::end(bufferB));


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
                pml::Log::Get(pml::Log::LOG_TRACE) << "CalculateHash\tHash size: A=" << nHashA << "\tB=" << nHashB << std::endl;
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

