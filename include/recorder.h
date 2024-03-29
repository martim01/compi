#pragma once
#include "portaudio.h"
#include <string>
#include <deque>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>


using deinterlacedBuffer = std::pair<std::deque<float>, std::deque<float> >;
using peak = std::pair<float, float>;
class Recorder
{
    public:

        Recorder(const std::string& sDeviceName, unsigned long nSampleRate, const std::chrono::milliseconds& startDelay, const std::chrono::milliseconds& maxDelay, const std::chrono::milliseconds& minWindow);
        Recorder(unsigned short nDeviceId, unsigned long nSampleRate, const std::chrono::milliseconds& startDelay, const std::chrono::milliseconds& maxDelay, const std::chrono::milliseconds& minWindow);

        bool Init();
        void Exit();

        void Callback(const float* pBuffer, size_t nFrameCount);

        std::mutex& GetMutex() { return m_mutex;}
        std::condition_variable& GetConditionVariable() { return m_cv; }

        bool BufferFull();
        void CompiReady();

        deinterlacedBuffer CreateBuffer();

        const peak& GetPeak();



        size_t GetNumberOfSamplesToHash() const
        {
            return m_nSamplesToHash;
        }

        size_t Locked(bool bLocked, long nOffset);

        std::chrono::milliseconds GetMaxDelay();
        std::chrono::milliseconds GetExpectedTimeToFillBuffer();

        unsigned int GetMaxSamplesForDelay() const { return m_nMaxSamplesForDelay; }
        unsigned int GetCurrentSamplesForDelay() const { return m_nSamplesForDelay; }

        ~Recorder();
    private:

        bool InitPortAudio();
        short GetDeviceId(const std::string& sName);
        std::string GetDeviceName(short nDeviceId);
        bool StartRecording();

        std::string m_sDeviceName;
        short m_nDeviceId;
        unsigned long m_nSampleRate;
        unsigned short m_nChannels;

        size_t m_nStartSamplesForDelay;
        size_t m_nSamplesForDelay;
        size_t m_nMaxSamplesForDelay;

        PaStream* m_pStream;                ///< pointer to the PaStream tha reads in the audio
        bool m_bLoop;

        bool m_bInputOk;

        unsigned int m_nSamplesToHash;

        size_t m_nTotalSamples;

        deinterlacedBuffer m_Buffer;

        bool m_bAdjustDelayWindow;

        std::mutex m_mutex;
        std::mutex m_mutexInternal;
        std::condition_variable m_cv;

        peak m_peak;

        std::atomic<long> m_nOffset;
        std::atomic<bool> m_bLocked;
        std::atomic<bool> m_bReady;

        static const unsigned short CHANNELS =2;
};


/** PaStreamCallback function - simply calls wmComparitor::Callback using userData to get the wmComparitor object
**/
int paCallback( const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData );
