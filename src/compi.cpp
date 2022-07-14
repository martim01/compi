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
#include "utils.h"
#include "minuscompare.h"
#include "troughcompare.h"
#include "spectrumcompare.h"

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
    m_tpStart(std::chrono::system_clock::now()),
    m_nMask(FOLLOW_ACTIVE),
    m_bActive(false),
    m_eCheck(HASH),
    m_bLocked(false),
    m_dFFTChangeDown(0.05),
    m_dFFTChangeUp(0.1),
    m_nFFTBands(20),
    m_dFFTLimits(10.0)
{

}

Compi::~Compi()
{

}

void Compi::SetupLogging()
{
    if(m_iniConfig.GetIniInt("log","file",0) == 1)
    {
        size_t nIndex = pmlLog().AddOutput(std::unique_ptr<pml::LogOutput>(new LogToFile(m_iniConfig.GetIniString("paths", "logs", "."))));
        pmlLog().SetOutputLevel(nIndex, static_cast<pml::enumLevel>(m_iniConfig.GetIniInt("loglevel", "file", 2)));
    }
    if(m_iniConfig.GetIniInt("log","console",1) == 1)
    {
        size_t nIndex = pmlLog().AddOutput(std::unique_ptr<pml::LogOutput>(new pml::LogOutput()));
        pmlLog().SetOutputLevel(nIndex, static_cast<pml::enumLevel>(m_iniConfig.GetIniInt("loglevel", "console", 2)));
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
    if(m_iniConfig.GetIniString("method", "check", "hash") == "minus")
    {
        m_eCheck = MINUS;
    }
    else if(m_iniConfig.GetIniString("method", "check", "hash") == "spectrum")
    {
        m_eCheck = FFT_DIFF;
        SetupSpectrumComparitor();
    }


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

void Compi::SetupSpectrumComparitor()
{
    m_pSpectrum = std::make_unique<SpectrumCompare>(m_iniConfig.GetIniString("Spectrum", "Profile", "/usr/local/etc/profile"),
                                                    m_nSampleRate, m_iniConfig.GetIniInt("Spectrum", "FramesForGood", 5000), m_iniConfig.GetIniInt("Spectrum", "FramesForCurrent", 5000),
                                                    m_iniConfig.GetIniDouble("Spectrum", "MaxLevel", 3.0), m_iniConfig.GetIniInt("Spectrum", "MaxBands", 30));
}

void Compi::HandleNoLock()
{
    if(m_bLocked)
    {

        m_nFailureCount++;
        pmlLog() << "Compi\tNo match for " << m_nFailureCount  << " calculations since last increase of delay";
    }
    else
    {   //if we've already lost the lock then don't bother chekcing x times before moving on
        m_nFailureCount = m_nFailures;
    }

    if(m_nFailureCount >= m_nFailures)
    {

        if(m_pRecorder->GetCurrentSamplesForDelay() != m_pRecorder->GetMaxSamplesForDelay())
        {
            size_t nDelay = m_pRecorder->Locked(false, 0);
            pmlLog() << "Compi\tExceeded max lock failures. Set lock to false and increase window to " << nDelay << "ms";
        }
        else
        {
            m_pRecorder->Locked(false, 0);
        }
        m_bLocked = false;

    }
}

bool Compi::HandleLock(const hashresult& result)
{
    if(m_nFailureCount != 0)
    {
        pmlLog(pml::LOG_INFO) << "Compi\tRelocked.";
    }
    m_nFailureCount = 0;

    if(!m_bLocked)
    {
        size_t nDelay = m_pRecorder->Locked(true, result.first);

        pmlLog(pml::LOG_INFO) << "Compi\tLocked. Window: " << nDelay << "ms";
        m_bLocked = true;

        return true;
    }
    return false;
}

void Compi::Loop()
{
    bool bAES(false);
    while(g_bRun)
    {
        std::unique_lock<std::mutex> lck(m_pRecorder->GetMutex());

        //work out how long to wait for before the buffer should be full
        bool bDone = m_pRecorder->GetConditionVariable().wait_for(lck, m_pRecorder->GetExpectedTimeToFillBuffer(), [this]{return m_pRecorder->BufferFull(); });

        pmlLog(pml::LOG_TRACE) << "MEMORY\t" << GetMemoryUsage();
        hashresult result{0,0.0};
        if(bDone)
        {
            if(bAES)
            {
                pmlLog() << "AES detected";

                bAES = true;
            }

            LogHeartbeat();

            bool bJustLocked(false);

            bool bSilentA = CheckSilence(m_pRecorder->GetPeak().first, A_LEG);
            bool bSilentB = CheckSilence(m_pRecorder->GetPeak().second, B_LEG);
            if(!bSilentA || !bSilentB)
            {
                deinterlacedBuffer buffer(m_pRecorder->CreateBuffer());

                switch(m_eCheck)
                {
                    case MINUS:
                        result = CalculateMinus(buffer.first,buffer.second, m_pRecorder->GetPeak(), m_pRecorder->GetNumberOfSamplesToHash(), m_bLocked, result);
                        break;
                    case FFT_DIFF:
                        result = m_pSpectrum->AddAudio(buffer.first, buffer.second);
                        break;
                    default:
                        result = CalculateHash(buffer.first,buffer.second, m_pRecorder->GetNumberOfSamplesToHash(), m_bLocked);
                }

                pmlLog(pml::LOG_DEBUG) << "Compi\tCalculation\tDelay=" <<  (result.first*1000/m_nSampleRate) << "ms\tConfidence=" << result.second;
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
                m_pRecorder->CreateBuffer();    //have to create buffer so that we clear it out
                m_nFailureCount = 0;
                result = {0,1.0};
                pmlLog(pml::LOG_TRACE) << "Compi\tBoth channels silent";


            }
            UpdateSNMP(result, bJustLocked);
        }
        else
        {
            if(bAES)
            {
                bAES = false;
                pmlLog(pml::LOG_ERROR) << "Compi\tAES Lost!";;
            }
            m_pAgent->AudioChanged(0);
            m_pAgent->ComparisonChanged(-1);
            m_pAgent->DelayChanged(std::chrono::milliseconds(0));
            m_pAgent->SilenceChanged(true, A_LEG);  //no AES so it is silent
            m_pAgent->SilenceChanged(true, B_LEG); //no AES so it is silent
        }


        m_pRecorder->CompiReady();
    }
    pmlLog() << "Compi\tExiting....";
}

void Compi::UpdateSNMP(const hashresult& result, bool bJustLocked)
{
    if(m_nMask == FORCE_ON || (m_nMask == FOLLOW_ACTIVE && m_bActive) || !m_bSendOnActiveOnly)
    {
        //send the traps
        m_pAgent->AudioChanged(1);  //audio must be okay as we are here
        //we only say comparison has failed if it has failed for more than the max failures we are allowing
        if(m_nFailureCount >= m_nFailures)
        {
            m_pAgent->ComparisonChanged(0);
            m_nFailureCount = 0;
        }
        else
        {
            m_pAgent->ComparisonChanged(1);
        }
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

        pmlLog(pml::LOG_INFO) << "Compi\tcompi started";
        m_tpLogBeat = std::chrono::system_clock::now();

        SetupAgent();
        SetupRecorder();


        m_dFFTChangeDown = m_iniConfig.GetIniDouble("FFTDiff", "Down", 0.05);
        m_dFFTChangeUp = m_iniConfig.GetIniDouble("FFTDiff", "Up", 0.1);
        m_nFFTBands = m_iniConfig.GetIniInt("FFTDiff", "Bands", 20);
        m_dFFTLimits = m_iniConfig.GetIniInt("FFTDiff", "Limits", 10.0);

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
        pmlLog(pml::LOG_WARN) << "Compi\tCould not dynamic cast!";
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

        pmlLog() << "Compi\tSNMP mask set to " << m_nMask;
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
        pmlLog(pml::LOG_ERROR) << "Could not dynamic cast!";
    }

    if(pInt && *pInt < 2)
    {
        m_bActive = (*pInt != 0);

        m_pAgent->OverallChanged(m_nMask == FORCE_ON || (m_nMask == FOLLOW_ACTIVE && m_bActive));

        if(m_nMask == FORCE_OFF || (m_nMask == FOLLOW_ACTIVE && !m_bActive))
        {
            ClearSNMP();
        }

        pmlLog(pml::LOG_INFO) << "Active set to " << m_bActive;
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


void Compi::LogHeartbeat()
{
    auto minutesLog = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now()-m_tpLogBeat);
    if(minutesLog.count() > 59)
    {
        pmlLog(pml::LOG_CRITICAL) << "[LogBeat]" << ConvertTimeToIsoString(m_tpStart);
        m_tpLogBeat = std::chrono::system_clock::now();
    }
}
