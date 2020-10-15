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


Recorder::Recorder(const std::string& sDeviceName, unsigned long nSampleRate, const std::chrono::milliseconds& maxDelay) :
m_sDeviceName(sDeviceName),
m_nDeviceId(-1),
m_nSampleRate(nSampleRate),
m_nSamplesNeeded(maxDelay.count()*m_nSampleRate/500),
m_nSamplesToHash(4096)
{

}

Recorder::Recorder(unsigned short nDeviceId, unsigned long nSampleRate, const std::chrono::milliseconds& maxDelay) :
m_sDeviceName(""),
m_nDeviceId(nDeviceId),
m_nSampleRate(nSampleRate),
m_nSamplesNeeded(maxDelay.count()*m_nSampleRate/500),
m_nSamplesToHash(4096)
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
    pml::Log::Get(pml::Log::LOG_INFO) << "Recording started" << std::endl;
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

    //std::lock_guard<std::mutex> lg(m_mutex);


    if(m_Buffer.first.size() < m_nSamplesNeeded+m_nSamplesToHash)
    {

        //store the audio
        for(size_t i = 0; i < nFrameCount*CHANNELS; i+=CHANNELS)
        {
            m_Buffer.first.push_back(pBuffer[i]);
            m_Buffer.second.push_back(pBuffer[i+1]);
        }

        //do the hash
        if(m_Buffer.first.size() >= m_nSamplesNeeded+m_nSamplesToHash)
        {
            m_cv.notify_one();
        }
    }

}

bool Recorder::BufferFull()
{
    //std::lock_guard<std::mutex> lg(m_mutex);
    return (m_Buffer.first.size() > m_nSamplesNeeded+m_nSamplesToHash);
}

void Recorder::EmptyBuffer()
{
    //std::lock_guard<std::mutex> lg(m_mutex);
    m_Buffer.first.clear();
    m_Buffer.second.clear();

    pml::Log::Get(pml::Log::LOG_DEBUG) << "Recorder\tBuffer empty" << std::endl;
}

void Recorder::SetMaxDelay(const std::chrono::milliseconds& maxDelay)
{
    m_nSamplesNeeded = maxDelay.count()*m_nSampleRate/500;
}

std::chrono::milliseconds Recorder::GetMaxDelay()
{
    return std::chrono::milliseconds(m_nSamplesNeeded*500/m_nSampleRate);
}
