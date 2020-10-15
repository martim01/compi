#pragma once
#include <memory>
#include "inimanager.h"
#include "hash.h"


class AgentThread;
class Recorder;

class Compi
{
    public:
        Compi();
        int Run(const std::string& sPath);

    private:

        void SetupLogging();
        void SetupAgent();
        void SetupRecorder();
        void Loop();

        void HandleNoLock();
        void HandleLock(const hashresult& result);
        void UpdateSNMP(const hashresult& result);

        iniManager m_iniConfig;
        std::shared_ptr<AgentThread> m_pAgent;
        std::shared_ptr<Recorder> m_pRecorder;

        int m_nSampleRate;
        int m_nStartDelay;
        int m_nMaxDelay;

};
