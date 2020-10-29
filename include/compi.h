#pragma once
#include <memory>
#include "inimanager.h"
#include "hash.h"
#include <atomic>

namespace Snmp_pp
{
    class SnmpSyntax;
};

class AgentThread;
class Recorder;

class Compi
{
    public:
        Compi();
        int Run(const std::string& sPath);

        bool MaskCallback(Snmp_pp::SnmpSyntax* pValue, int nSyntax);
        bool ActivateCallback(Snmp_pp::SnmpSyntax* pValue, int nSyntax);

    private:

        void SetupLogging();
        void SetupAgent();
        void SetupRecorder();
        void Loop();

        void HandleNoLock();
        void HandleLock(const hashresult& result);
        void UpdateSNMP(const hashresult& result);
        void ClearSNMP();

        iniManager m_iniConfig;
        std::shared_ptr<AgentThread> m_pAgent;
        std::shared_ptr<Recorder> m_pRecorder;

        int m_nSampleRate;
        int m_nStartDelay;
        int m_nMaxDelay;
        int m_nFailures;
        int m_nFailureCount;
        bool m_bSendOnActiveOnly;

        std::atomic<unsigned int> m_nMask;
        std::atomic<bool> m_bActive;

        enum { FORCE_OFF, FOLLOW_ACTIVE,FORCE_ON};
};
