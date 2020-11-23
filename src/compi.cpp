#include "compi.h"
#include "agentthread.h"
#include "recorder.h"
#include "log.h"
#include "inisection.h"
#include "logtofile.h"
#include "hash.h"
#include <thread>
#include <functional>
#include <snmp_pp/integer.h>
#include <cmath>


Compi::Compi() :
    m_pAgent(nullptr),
    m_pRecorder(nullptr),
    m_nSampleRate(48000),
    m_nStartDelay(100),
    m_nMaxDelay(8000),
    m_nFailures(3),
    m_nFailureCount(0),
    m_bSendOnActiveOnly(false),
    m_dSilenceThreshold(-70),
    m_nSilenceHoldoff(30),
    m_nSilent{-1,-1},
    m_nMask(FOLLOW_ACTIVE),
    m_bActive(false),
    m_bLocked(false)
{

}

void Compi::SetupLogging()
{
    if(m_iniConfig.GetIniInt("log","file",0) == 1)
    {
        size_t nIndex = pml::Log::Get(pml::Log::LOG_INFO).AddOutput(std::unique_ptr<pml::LogOutput>(new LogToFile(m_iniConfig.GetIniString("paths", "logs", "~/compilogs"))));
        pml::Log::Get().SetOutputLevel(nIndex, static_cast<pml::Log::enumLevel>(m_iniConfig.GetIniInt("loglevel", "file", 2)));
    }
    else if(m_iniConfig.GetIniInt("log","console",1) == 1)
    {
        size_t nIndex = pml::Log::Get().AddOutput(std::unique_ptr<pml::LogOutput>(new pml::LogOutput()));
        pml::Log::Get().SetOutputLevel(nIndex, static_cast<pml::Log::enumLevel>(m_iniConfig.GetIniInt("loglevel", "console", 2)));
    }
}

void Compi::SetupAgent()
{
    m_pAgent = std::make_shared<AgentThread>(m_iniConfig.GetIniInt("snmp", "port_snmp", 161),
                                             m_iniConfig.GetIniInt("snmp", "port_trap", 162),
                                             m_iniConfig.GetIniString("snmp","base_oid",""),
                                             m_iniConfig.GetIniString("snmp","community","public"));

    m_bSendOnActiveOnly = m_iniConfig.GetIniInt("snmp", "active_only", 0);
    m_nMask = m_iniConfig.GetIniInt("snmp", "mask", 1);


    iniSection* pSection = m_iniConfig.GetSection("trap_destinations");
    if(pSection)
    {
        for(std::map<std::string, std::string>::const_iterator itKey = pSection->GetDataBegin(); itKey != pSection->GetDataEnd(); ++itKey)
        {
            m_pAgent->AddTrapDestination(itKey->second);
        }
    }

    m_pAgent->Init(std::bind(&Compi::MaskCallback,this,std::placeholders::_1, std::placeholders::_2),
                   std::bind(&Compi::ActivateCallback,this,std::placeholders::_1, std::placeholders::_2), m_nMask);

    m_pAgent->Run();

}

void Compi::SetupRecorder()
{
    int nDevice = m_iniConfig.GetIniInt("recorder", "deviceid", -1);
    m_nSampleRate=m_iniConfig.GetIniInt("recorder", "samplerate", 48000);
    m_nStartDelay = m_iniConfig.GetIniInt("delay", "start", 100);
    m_nMaxDelay = m_iniConfig.GetIniInt("delay", "max", 8000);
    m_nFailures = m_iniConfig.GetIniInt("delay", "failures", 3);
    m_dSilenceThreshold  = std::pow(10, (m_iniConfig.GetIniDouble("silence", "threshold", -70)/20));
    m_nSilenceHoldoff  = m_iniConfig.GetIniInt("silence", "holdoff", 30);


    if(nDevice != -1)
    {
        m_pRecorder = std::make_shared<Recorder>(nDevice,m_nSampleRate, std::chrono::milliseconds(m_nStartDelay),
            std::chrono::milliseconds(m_nMaxDelay), std::chrono::milliseconds(m_iniConfig.GetIniInt("comparison","window",250)));

    }
    else
    {
        m_pRecorder = std::make_shared<Recorder>(m_iniConfig.GetIniString("recorder", "device", "default"),m_nSampleRate, std::chrono::milliseconds(m_nStartDelay),
        std::chrono::milliseconds(m_nMaxDelay), std::chrono::milliseconds(m_iniConfig.GetIniInt("comparison","window",250)));

    }
    m_pRecorder->Init();

}

void Compi::HandleNoLock()
{
    if(m_bLocked)
    {

        m_nFailureCount++;
        pml::Log::Get() << "Compi\tNo match for " << m_nFailureCount  << " calculations since last increase of delay" << std::endl;
    }
    else
    {   //if we've already lost the lock then don't bother chekcing x times before moving on
        m_nFailureCount = m_nFailures;
    }

    if(m_nFailureCount >= m_nFailures)
    {
        size_t nDelay = m_pRecorder->Locked(false, 0);
        pml::Log::Get() << "Compi\tExceeded max lock failures. Set lock to false and increase window to " << nDelay << "ms" << std::endl;
        m_bLocked = false;

        m_nFailureCount = 0;
    }
}

bool Compi::HandleLock(const hashresult& result)
{
    m_nFailureCount = 0;

    if(!m_bLocked)
    {
        size_t nDelay = m_pRecorder->Locked(true, result.first);
        pml::Log::Get(pml::Log::LOG_INFO) << "Compi\tLocked. Window: " << nDelay << "ms" << std::endl;
        m_bLocked = true;

        return true;
    }
    return false;
}

void Compi::Loop()
{
    while(g_bRun)
    {
        std::unique_lock<std::mutex> lck(m_pRecorder->GetMutex());

        //work out how long to wait for before the buffer should be full
        bool bDone = m_pRecorder->GetConditionVariable().wait_for(lck, m_pRecorder->GetExpectedTimeToFillBuffer(), [this]{return m_pRecorder->BufferFull(); });


        hashresult result{0,0.0};
        if(bDone)
        {
            bool bJustLocked(false);

            bool bSilentA = CheckSilence(m_pRecorder->GetPeak().first, A_LEG);
            bool bSilentB = CheckSilence(m_pRecorder->GetPeak().second, B_LEG);
            if(!bSilentA || !bSilentB)
            {
                deinterlacedBuffer buffer(m_pRecorder->CreateBuffer());
                result = CalculateHash(buffer.first,buffer.second, m_pRecorder->GetNumberOfSamplesToHash(), m_bLocked);

                pml::Log::Get() << "Compi\tCalculation\tDelay=" <<  (result.first*1000/m_nSampleRate) << "ms\tConfidence=" << result.second << std::endl;
                if(result.second < 0.5) //could not get lock
                {
                    HandleNoLock();
                }
                else
                {
                    bJustLocked = HandleLock(result);
                }
            }
            else
            {
                result = {0,1.0};
                pml::Log::Get() << "Compi\tBoth channels silent" << std::endl;
            }
            UpdateSNMP(result, bJustLocked);
        }
        else
        {
            pml::Log::Get(pml::Log::LOG_ERROR) << "Compi\tBuffer taking longer to fill than expected!" << std::endl;
            m_pAgent->AudioChanged(0);
            m_pAgent->ComparisonChanged(-1);
            m_pAgent->DelayChanged(std::chrono::milliseconds(0));
        }


        m_pRecorder->CompiReady();
    }
    pml::Log::Get() << "Compi\tExiting...." << std::endl;
}

void Compi::UpdateSNMP(const hashresult& result, bool bJustLocked)
{
    if(m_nMask == FORCE_ON || (m_nMask == FOLLOW_ACTIVE && m_bActive) || !m_bSendOnActiveOnly)
    {
        //send the traps
        m_pAgent->AudioChanged(1);  //audio must be okay as we are here
        m_pAgent->ComparisonChanged(result.second > 0.6);
        if(bJustLocked)
        {
            m_pAgent->DelayChanged(std::chrono::milliseconds(result.first*1000/m_nSampleRate));
        }
    }
}

void Compi::ClearSNMP()
{
    if(m_bSendOnActiveOnly)
    {
        m_pAgent->AudioChanged(1);
        m_pAgent->ComparisonChanged(1);
    }
}

int Compi::Run(const std::string& sPath)
{
    if(m_iniConfig.ReadIniFile(sPath+"/compi.ini"))
    {
        SetupLogging();

        pml::Log::Get(pml::Log::LOG_INFO) << "Compi\tcompi started" << std::endl;

        SetupAgent();
        SetupRecorder();

        Loop();

        return 0;

    }
    return -1;
}


bool Compi::MaskCallback(Snmp_pp::SnmpSyntax* pValue, int nSyntax)
{
    Snmp_pp::SnmpInt32* pInt = dynamic_cast<Snmp_pp::SnmpInt32*>(pValue);
    if(!pInt)
    {
        pml::Log::Get(pml::Log::LOG_WARN) << "Compi\tCould not dynamic cast!" << std::endl;
    }

    if(pInt && *pInt < 3)
    {
        if(m_nMask != *pInt)
        {
            m_nMask = *pInt;
            m_iniConfig.SetSectionValue("snmp", "mask", m_nMask);
            m_iniConfig.WriteIniFile();

            m_pAgent->OverallChanged(m_nMask == FORCE_ON || (m_nMask == FOLLOW_ACTIVE && m_bActive));

            if(m_nMask == FORCE_OFF || (m_nMask == FOLLOW_ACTIVE && !m_bActive))
            {
                ClearSNMP();
            }

        }

        pml::Log::Get() << "Compi\tSNMP mask set to " << m_nMask << std::endl;
        return true;
    }
    else
    {
        return false;
    }
}

bool Compi::ActivateCallback(Snmp_pp::SnmpSyntax* pValue, int nSyntax)
{
    Snmp_pp::SnmpInt32* pInt = dynamic_cast<Snmp_pp::SnmpInt32*>(pValue);
    if(!pInt)
    {
        pml::Log::Get() << "Could not dynamic cast!" << std::endl;
    }

    if(pInt && *pInt < 2)
    {
        m_bActive = (*pInt != 0);

        m_pAgent->OverallChanged(m_nMask == FORCE_ON || (m_nMask == FOLLOW_ACTIVE && m_bActive));

        if(m_nMask == FORCE_OFF || (m_nMask == FOLLOW_ACTIVE && !m_bActive))
        {
            ClearSNMP();
        }

        pml::Log::Get(pml::Log::LOG_INFO) << "Active set to " << m_bActive << std::endl;
        return true;
    }
    else
    {
        return false;
    }
}


bool Compi::CheckSilence(float dPeak, enumLeg eLeg)
{
    if(dPeak < m_dSilenceThreshold)
    {
        if(m_nSilent[eLeg] != 1)
        {
            m_nSilent[eLeg] = 1;
            m_tpSilence[eLeg] = std::chrono::system_clock::now();
        }
        else
        {
            auto secondsSilent = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now()-m_tpSilence[eLeg]);
            if(secondsSilent.count() > m_nSilenceHoldoff)
            {
                m_pAgent->SilenceChanged(true, eLeg);
            }
        }
    }
    else if(m_nSilent[eLeg] != 0)
    {
        m_nSilent[eLeg] = 0;
        m_pAgent->SilenceChanged(false, eLeg);
    }
    return (m_nSilent[eLeg] == 1);
}
