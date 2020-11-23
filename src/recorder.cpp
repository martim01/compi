#include "recorder.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include "log.h"
#include <thread>

#include "hash.h"


int paCallback( const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData )
{
    if(input)
    {
        reinterpret_cast<Recorder*>(userData)->Callback(reinterpret_cast<const float*>(input), frameCount);
    }
    return 0;
}


Recorder::Recorder(const std::string& sDeviceName, unsigned long nSampleRate, const std::chrono::milliseconds& startDelay, const std::chrono::milliseconds& maxDelay, const std::chrono::milliseconds& minWindow) :
m_sDeviceName(sDeviceName),
m_nDeviceId(-1),
m_nSampleRate(nSampleRate),
m_nStartSamplesForDelay(startDelay.count()*m_nSampleRate/500),
m_nSamplesForDelay(m_nStartSamplesForDelay),
m_nMaxSamplesForDelay(maxDelay.count()*m_nSampleRate/500),
m_nSamplesToHash((minWindow.count()*m_nSampleRate)/1000),
m_peak({0.0,0.0}),
m_nOffset(0),
m_bLocked(false),
m_bReady(true)
{

}

Recorder::Recorder(unsigned short nDeviceId, unsigned long nSampleRate, const std::chrono::milliseconds& startDelay, const std::chrono::milliseconds& maxDelay, const std::chrono::milliseconds& minWindow) :
m_sDeviceName(""),
m_nDeviceId(nDeviceId),
m_nSampleRate(nSampleRate),
m_nStartSamplesForDelay(startDelay.count()*m_nSampleRate/500),
m_nSamplesForDelay(m_nStartSamplesForDelay),
m_nMaxSamplesForDelay(maxDelay.count()*m_nSampleRate/500),
m_nSamplesToHash((minWindow.count()*m_nSampleRate)/1000),
m_peak({0.0,0.0}),
m_nOffset(0),
m_bLocked(false),
m_bReady(true)
{
}


Recorder::~Recorder()
{
    Pa_Terminate();
}


bool Recorder::Init()
{

    if(!InitPortAudio())
    {
        pml::Log::Get(pml::Log::LOG_CRITICAL) << "Recorder\tCould not init port audio for some reason" << std::endl;
        return false;
    }
    return StartRecording();

}



bool Recorder::InitPortAudio()
{
    PaError err = Pa_Initialize();
    if( err != paNoError )
    {
        pml::Log::Get(pml::Log::LOG_CRITICAL) << "Recorder\tUnable to init port audio:" <<  Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        return false;
    }

    //get the device Id or name
    if(m_nDeviceId == -1)
    {
        m_nDeviceId = GetDeviceId(m_sDeviceName);
        if(m_nDeviceId == -1)
        {
            pml::Log::Get(pml::Log::LOG_CRITICAL)  << "Recorder\tDevice " << m_sDeviceName << " does not exist" << std::endl;
            return false;
        }
    }
    else
    {
        m_sDeviceName = GetDeviceName(m_nDeviceId);
        if(m_sDeviceName.empty())
        {
            pml::Log::Get(pml::Log::LOG_CRITICAL)  << "Recorder\tDevice " << m_nDeviceId << " does not exist" << std::endl;
            return false;
        }
    }




    //Open the stream

    PaStreamParameters inputParameters;
    inputParameters.channelCount = CHANNELS;
    inputParameters.device = m_nDeviceId;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = 0;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_IsFormatSupported(&inputParameters, 0, m_nSampleRate);
    if(err != paNoError)
    {
        pml::Log::Get(pml::Log::LOG_CRITICAL)  << "Recorder\tInput paramaters are not supported: " <<  Pa_GetErrorText(err) << std::endl;
        return false;
    }

    err = Pa_OpenStream(&m_pStream, &inputParameters, 0, m_nSampleRate, 4096, paNoFlag, paCallback, reinterpret_cast<void*>(this) );
    if(err != paNoError)
    {
        pml::Log::Get(pml::Log::LOG_CRITICAL)  << "Recorder\tUnable to open stream: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    const PaStreamInfo* pInfo = Pa_GetStreamInfo(m_pStream);
    if(pInfo)
    {
        pml::Log::Get(pml::Log::LOG_INFO)  << "Recorder\tStream opened: Latency " << pInfo->inputLatency << " SampleRate " << pInfo->sampleRate << std::endl;

        m_nSampleRate = pInfo->sampleRate;
    }

    pml::Log::Get(pml::Log::LOG_INFO)  << "Recorder\tSamplesToHash=" << m_nSamplesToHash << std::endl;
    return true;
}

short Recorder::GetDeviceId(const std::string& sDeviceName)
{
    //enum the devices....
    int nDevices;
    nDevices = Pa_GetDeviceCount();
    if( nDevices < 0 )
    {
        pml::Log::Get(pml::Log::LOG_CRITICAL)  << "Recorder\tUnable to get devices: " <<  Pa_GetErrorText(nDevices);
        return -1;
    }

    for(int i = 0; i < nDevices; i++)
    {
        const PaDeviceInfo* pDevInfo = Pa_GetDeviceInfo(i);
        if(pDevInfo->name == sDeviceName)
        {
            pml::Log::Get(pml::Log::LOG_INFO) << "Recorder\tDevice " << sDeviceName << " found. Id is " <<  i << std::endl;
            return i;
        }
        else
        {
            pml::Log::Get(pml::Log::LOG_INFO) << "Recorder\tInput " << i << " is called " <<  pDevInfo->name << std::endl;
        }
    }
    return -1;
}

std::string Recorder::GetDeviceName(short nDeviceId)
{
    const PaDeviceInfo* pDevInfo = Pa_GetDeviceInfo(nDeviceId);
    if(pDevInfo)
    {
        pml::Log::Get(pml::Log::LOG_INFO) << "Recorder\tDevice " << nDeviceId << " found. Name is " <<  pDevInfo->name << std::endl;
        return pDevInfo->name;
    }
    else
    {
        pml::Log::Get(pml::Log::LOG_ERROR) << "Recorder\tCould not find device " << nDeviceId << std::endl;
        return "";
    }
}



bool Recorder::StartRecording()
{
    PaError err = Pa_StartStream(m_pStream);
    if(err != paNoError)
    {
        pml::Log::Get(pml::Log::LOG_CRITICAL)  << "Recorder\tUnable to start stream: " << Pa_GetErrorText(err) << std::endl;;
        return false;
    }
    pml::Log::Get(pml::Log::LOG_INFO) << "Recorder\tRecording started" << std::endl;
    return true;

}



void Recorder::Callback(const float* pBuffer, size_t nFrameCount)
{


    if(m_bInputOk == false)
    {
        m_bInputOk = true;
        pml::Log::Get(pml::Log::LOG_INFO) << "Recorder\tInput Okay: " << std::this_thread::get_id() << std::endl;
    }

    if(nFrameCount != 4096)
    {
        pml::Log::Get(pml::Log::LOG_ERROR) << "Recorder\tMissing frames" << std::endl;
    }

    std::lock_guard<std::mutex> lg(m_mutexInternal);


    //store the audio
    m_peak = {0.0,0.0};
    for(size_t i = 0; i < nFrameCount*CHANNELS; i+=CHANNELS)
    {
        m_Buffer.first.push_back(pBuffer[i]);
        m_Buffer.second.push_back(pBuffer[i+1]);


        m_peak.first = std::max(std::abs(pBuffer[i]), m_peak.first);
        m_peak.second = std::max(std::abs(pBuffer[i+1]), m_peak.second);
    }

    pml::Log::Get(pml::Log::LOG_DEBUG) << "Recorder\tCallback\tReady: " << m_bReady
                                       <<"\tBuffer: " << std::min(m_Buffer.first.size(), m_Buffer.second.size())
                                       << "\tNeeded: " << (m_nSamplesForDelay+m_nSamplesToHash+abs(m_nOffset)) << std::endl;

    if(m_bReady)
    {
        if((m_nOffset < 0 && m_Buffer.first.size() > m_nSamplesForDelay+m_nSamplesToHash+abs(m_nOffset)) ||
           (m_nOffset >= 0 && m_Buffer.second.size() > m_nSamplesForDelay+m_nSamplesToHash+abs(m_nOffset)))
        {
            m_bReady = false;
            pml::Log::Get(pml::Log::LOG_TRACE) << "Recorder\tBufferA=" << m_Buffer.first.size() << "\tBufferB=" << m_Buffer.second.size() << std::endl;
            m_cv.notify_one();
        }
    }

}

bool Recorder::BufferFull()
{
    std::lock_guard<std::mutex> lg(m_mutexInternal);
    return (m_bReady == false);
}

void Recorder::CompiReady()
{
    std::lock_guard<std::mutex> lg(m_mutexInternal);
    m_peak.first = m_peak.second = 0.0;
    m_bReady = true;
    pml::Log::Get(pml::Log::LOG_DEBUG) << "Recorder\tCompi Ready" << std::endl;
}


std::chrono::milliseconds Recorder::GetMaxDelay()
{
    return std::chrono::milliseconds((m_nSamplesForDelay+m_nSamplesToHash+abs(m_nOffset))*500/m_nSampleRate);
}


std::chrono::milliseconds Recorder::GetExpectedTimeToFillBuffer()
{
    return std::chrono::milliseconds(((m_nSamplesForDelay+m_nSamplesToHash)*1000/m_nSampleRate)+2000); //add 2seconds to allow for things not working right
}

size_t Recorder::Locked(bool bLocked, long nOffset)
{
    pml::Log::Get(pml::Log::LOG_DEBUG) << "Recorder\tLocked: " << bLocked<<"\tOffset=" << nOffset << std::endl;

    if(bLocked && m_bLocked == false)
    {
        m_nSamplesForDelay = m_nStartSamplesForDelay;
        m_nOffset = nOffset;
  }
    else if(!bLocked)
    {
        m_nSamplesForDelay = std::min(std::max(m_nSamplesForDelay, (size_t)m_nOffset)*2, m_nMaxSamplesForDelay);

        m_nOffset = 0;
        m_Buffer.first.clear();
        m_Buffer.second.clear();
    }


    m_bLocked = bLocked;

    return GetMaxDelay().count();

}

deinterlacedBuffer Recorder::CreateBuffer()
{
    std::lock_guard<std::mutex> lg(m_mutexInternal);

    size_t nA,nB;
    if(m_nOffset == 0)
    {
        nA = m_Buffer.first.size()-(m_nSamplesForDelay+m_nSamplesToHash);
        nB = m_Buffer.second.size()-(m_nSamplesForDelay+m_nSamplesToHash);

    }
    else if(m_nOffset < 0)
    {
        nA = m_Buffer.first.size()-(m_nSamplesForDelay+m_nSamplesToHash-m_nOffset);
        nB = m_Buffer.second.size()-(m_nSamplesForDelay+m_nSamplesToHash);
    }
    else
    {
        nA = m_Buffer.first.size()-(m_nSamplesForDelay+m_nSamplesToHash);
        nB = m_Buffer.second.size()-(m_nSamplesForDelay+m_nSamplesToHash+m_nOffset);
    }
    pml::Log::Get(pml::Log::LOG_TRACE) << "Recorder\tGetBuffer: A=" << m_Buffer.first.size() << "\tnB=" << m_Buffer.second.size() << std::endl;
    pml::Log::Get(pml::Log::LOG_TRACE) << "Recorder\tGetBuffer: nA=" << nA << "\tnB=" << nB << std::endl;

    m_Buffer.first.erase(m_Buffer.first.begin(), m_Buffer.first.begin()+nA);
    m_Buffer.second.erase(m_Buffer.second.begin(), m_Buffer.second.begin()+nB);

    pml::Log::Get(pml::Log::LOG_TRACE) << "Recorder\tGetBuffer: A=" << m_Buffer.first.size() << "\tnB=" << m_Buffer.second.size() << std::endl;

    return std::make_pair(std::deque<float>(m_Buffer.first.begin(), m_Buffer.first.begin()+(m_nSamplesForDelay+m_nSamplesToHash)),
                          std::deque<float>(m_Buffer.second.begin(), m_Buffer.second.begin()+(m_nSamplesForDelay+m_nSamplesToHash)));
}


const peak& Recorder::GetPeak()
{
    std::lock_guard<std::mutex> lg(m_mutexInternal);
    return m_peak;
}
