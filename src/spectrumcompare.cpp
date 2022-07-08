#include "spectrumcompare.h"
#include "hash.h"
#include "log.h"


SpectrumCompare::SpectrumCompare(unsigned long nSampleRate,unsigned long nFramesForGood, unsigned long nFramesForCurrent, double dMaxAllowedLevelDiff, unsigned long nMaxAllowedBandsDiff) :
    m_nSampleRate(nSampleRate),
    m_dFramesForGood(nFramesForGood),
    m_dFramesForCurrent(nFramesForCurrent),
    m_dMaxLevel(dMaxAllowedLevelDiff),
    m_nMaxBands(nMaxAllowedBandsDiff),
    m_vSpectrumGood(1024, 0.0),
    m_vSpectrumCurrent(1024,0.0), m_result{0,0.0}
{
    m_fftOut.first.resize(1024);
    m_fftOut.second.resize(1024);
}

SpectrumCompare::~SpectrumCompare()
{

}

hashresult SpectrumCompare::AddAudio(const std::deque<float>& bufferA, const std::deque<float>& bufferB)
{
    // add audio to our buffer
    std::copy(bufferA.begin(), bufferA.end(), std::back_inserter(m_Buffer.first));
    std::copy(bufferB.begin(), bufferB.end(), std::back_inserter(m_Buffer.second));

    if(!m_bOneShot || !m_bCalculated)
    {
        CalculateChannelOffset();
    }

    if(m_Buffer.first.size() >= bufferA.size()+m_nWindowSamples && m_Buffer.second.size() >= bufferB.size()+m_nWindowSamples)
    {
        ProcessAudio();
    }

    return m_result;
}


void SpectrumCompare::ProcessAudio()
{
    unsigned long nMaxBands = 0;

    while(m_Buffer.first.size() > (m_fftOut.first.size()-1)*2)
    {
        if(!m_bLive)
        {
            CalculateSpectrum(m_vSpectrumGood);
            m_result.second = 0.0;
            CheckGoLive();
        }
        else
        {
            CalculateSpectrum(m_vSpectrumCurrent);
            //Compare this spectrum against the one we've stored
            if(m_dTotalFrames > m_dFramesForCurrent)
            {
                nMaxBands  = std::max(nMaxBands, CompareSpectrums());

                pmlLog(pml::LOG_INFO) << "SpectrumCompare\t Good=" << m_vSpectrumGood[128] << "\tCurrent=" << m_vSpectrumCurrent[128] << "\tBands=" << nMaxBands;


                if(nMaxBands <= m_nMaxBands)
                {
                    m_result.second = 1.0;
                }
                else
                {
                    m_result.second = 0.0;
                }

                m_vSpectrumCurrent = std::vector<float>(1024,0.0);
                m_dTotalFrames = 0.0;
            }
        }
    }




}

void SpectrumCompare::CalculateSpectrum(std::vector<float>& vSpectrum)
{
    FFT(m_Buffer.first, m_fftOut.first);
    FFT(m_Buffer.second, m_fftOut.second);

    //m_dBinSize = static_cast<double>(m_nSampleRate)/static_cast<double>((m_fftOut.first.size()-1)*2);

    for(size_t i = 0; i < m_fftOut.first.size(); i++)
    {
        auto dAmplitudeA = abs(sqrt( (m_fftOut.first[i].r*m_fftOut.first[i].r) + (m_fftOut.first[i].i*m_fftOut.first[i].i)))/static_cast<float>(m_fftOut.first.size());
        auto dAmplitudeB = abs(sqrt( (m_fftOut.second[i].r*m_fftOut.second[i].r) + (m_fftOut.second[i].i*m_fftOut.second[i].i)))/static_cast<float>(m_fftOut.second.size());

        if(dAmplitudeA > 0.00003 && dAmplitudeB > 0.00003)
        {
            float dLogA = (20*log10(dAmplitudeA))* 0.754;
            float dLogB = (20*log10(dAmplitudeB))* 0.754;

            float dLog = -dLogA+dLogB;

            auto oldAverage = vSpectrum[i];
            vSpectrum[i] = oldAverage + ((dLog-oldAverage)/(m_dTotalFrames+1));
        }
    }


    m_dTotalFrames++;
}


void SpectrumCompare::FFT(std::list<float>& buffer, std::vector<kiss_fft_cpx>& vOut)
{
    pmlLog(pml::LOG_TRACE) << "SpectrumCompare\t FFT";
    std::vector<kiss_fft_scalar> vfft_in((vOut.size()-1)*2);

    for(size_t i = 0; i < vfft_in.size(); i++)
    {
        if(buffer.empty() == false)
        {
            vfft_in[i] = HannWindow(buffer.front(), i, vfft_in.size());
            buffer.pop_front();
        }
        else
        {
            vfft_in[i] = 0.0;
        }
    }

    //Now do the check
    kiss_fftr_cfg cfg;

    pmlLog(pml::LOG_TRACE) << "SpectrumCompare\t FFT Kiss";
    if ((cfg = kiss_fftr_alloc(vfft_in.size(), 0, NULL, NULL)) != NULL)
    {
        kiss_fftr(cfg, vfft_in.data(), vOut.data());
    }
    free(cfg);

    pmlLog(pml::LOG_TRACE) << "SpectrumCompare\t FFT: Done";
}



void SpectrumCompare::CalculateChannelOffset()
{
    pmlLog(pml::LOG_TRACE) << "SpectrumCompare\tCalculateChannelOffset";

    if(m_Buffer.first.size() >= m_nWindowSamples && m_Buffer.second.size() >= m_nWindowSamples)
    {
        // calculate delay
        m_result.first = CalculateOffset(std::vector<float>(m_Buffer.first.begin(), std::next(m_Buffer.first.begin(),m_nWindowSamples)),
                                      std::vector<float>(m_Buffer.second.begin(), std::next(m_Buffer.second.begin(), m_nWindowSamples)));

        // remove samples from leading side so that buffer is aligned
        if(m_result.first < -static_cast<int>(m_nAccuracy))
        {
            m_Buffer.second.erase(m_Buffer.second.begin(), std::next(m_Buffer.second.begin(), -m_result.first));
        }
        else if(m_result.first > static_cast<int>(m_nAccuracy))
        {
            m_Buffer.first.erase(m_Buffer.first.begin(), std::next(m_Buffer.first.begin(), m_result.first));
        }

        m_bCalculated = true;
    }
}


unsigned long SpectrumCompare::CompareSpectrums()
{
    unsigned long nBands = 0;
    for(size_t i = 0; i < m_vSpectrumGood.size(); i++)
    {
        if(abs(m_vSpectrumGood[i] - m_vSpectrumCurrent[i]) > m_dMaxLevel)
        {
            nBands++;
        }
    }


    return nBands;

}


void SpectrumCompare::CheckGoLive()
{
    if(m_dTotalFrames > m_dFramesForGood)
    {
        pmlLog(pml::LOG_INFO) << "SpectrumCompare\tGoLive";
        m_bLive = true;
        m_dTotalFrames = 0;
        m_bCalculated = false;
        m_Buffer.first.clear();
        m_Buffer.second.clear();
    }
}
