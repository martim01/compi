#pragma once
#include <memory>
#include "inimanager.h"
#include "hash.h"
#include <atomic>
#include <chrono>


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
        bool HandleLock(const hashresult& result);
        void UpdateSNMP(const hashresult& result, bool bJustLocked);
        void ClearSNMP();

        enum enumLeg{A_LEG=0, B_LEG=1};
        bool CheckSilence(float dPeak, enumLeg eLeg);

        iniManager m_iniConfig;
        std::shared_ptr<AgentThread> m_pAgent;
        std::shared_ptr<Recorder> m_pRecorder;

        int m_nSampleRate;
        int m_nStartDelay;
        int m_nMaxDelay;
        int m_nFailures;
        int m_nFailureCount;
        bool m_bSendOnActiveOnly;

        float m_dSilenceThreshold;
        int m_nSilenceHoldoff;
        int m_nSilent[2];
        std::chrono::time_point<std::chrono::system_clock> m_tpSilence[2];

        std::atomic<int> m_nMask;
        std::atomic<bool> m_bActive;

        bool m_bLocked;

        enum { FORCE_OFF, FOLLOW_ACTIVE,FORCE_ON};
};
