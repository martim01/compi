#include "compi.h"
#include "agentthread.h"
#include "recorder.h"
#include "log.h"
#include "inisection.h"
#include "logtofile.h"
#include "hash.h"

Compi::Compi() : m_pAgent(nullptr), m_pRecorder(nullptr)
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
    m_pAgent = std::make_shared<AgentThread>(m_iniConfig.GetIniInt("ports", "snmp", 161), m_iniConfig.GetIniInt("ports", "trap", 162), m_iniConfig.GetIniString("snmp","community","public"));
    iniSection* pSection = m_iniConfig.GetSection("trap_destinations");
    if(pSection)
    {
        for(std::map<std::string, std::string>::const_iterator itKey = pSection->GetDataBegin(); itKey != pSection->GetDataEnd(); ++itKey)
        {
            m_pAgent->AddTrapDestination(itKey->second);
        }
    }
    m_pAgent->Run();

}

void Compi::SetupRecorder()
{
    int nDevice = m_iniConfig.GetIniInt("recorder", "deviceid", -1);
    m_nSampleRate=m_iniConfig.GetIniInt("recorder", "samplerate", 48000);
    m_nStartDelay = m_iniConfig.GetIniInt("delay", "start", 100);
    m_nMaxDelay = m_iniConfig.GetIniInt("delay", "max", 8000);

    if(nDevice != -1)
    {
        m_pRecorder = std::make_shared<Recorder>(nDevice,m_nSampleRate, std::chrono::milliseconds(m_nStartDelay));

    }
    else
    {
        m_pRecorder = std::make_shared<Recorder>(m_iniConfig.GetIniString("recorder", "device", "default"),m_nSampleRate, std::chrono::milliseconds(m_nStartDelay));

    }
    m_pRecorder->Init();

}

void Compi::HandleNoLock()
{
    if(m_pRecorder->GetMaxDelay() < std::chrono::milliseconds(m_nMaxDelay))
    {
        m_pRecorder->SetMaxDelay(m_pRecorder->GetMaxDelay()*2);
        pml::Log::Get() << "Increase Max Delay: " << m_pRecorder->GetMaxDelay().count() << std::endl;
    }
    else
    {
        m_pRecorder->SetMaxDelay(std::chrono::milliseconds(m_nStartDelay));
        pml::Log::Get() << "Reset Max Delay: " << m_pRecorder->GetMaxDelay().count() << std::endl;
    }
}

void Compi::HandleLock(const hashresult& result)
{
    auto delay = std::chrono::milliseconds((abs(result.first*1000)/m_nSampleRate)+100);
    if(delay != m_pRecorder->GetMaxDelay())
    {
        m_pRecorder->SetMaxDelay(delay);
        pml::Log::Get() << "Lock: Set Max Delay: " << m_pRecorder->GetMaxDelay().count() << std::endl;
    }
}

void Compi::Loop()
{
    while(true)
    {
        std::unique_lock<std::mutex> lck(m_pRecorder->GetMutex());
        m_pRecorder->GetConditionVariable().wait(lck, [this]{return m_pRecorder->BufferFull(); });

        hashresult result = CalculateHash(m_pRecorder->GetBuffer().first,m_pRecorder->GetBuffer().second, m_pRecorder->GetNumberOfSamplesToHash());

        pml::Log::Get() << "Calculation\tDelay=" <<  (result.first*1000/m_nSampleRate) << "ms\tConfidence=" << result.second << std::endl;
        if(result.second < 0.5) //could not get lock
        {
            HandleNoLock();
        }
        else
        {
            HandleLock(result);
        }

        UpdateSNMP(result);
        //get the recorder to start filling up again
        m_pRecorder->EmptyBuffer();
    }
}

void Compi::UpdateSNMP(const hashresult& result)
{
    //send the traps
    m_pAgent->AudioChanged(1);  //audio must be okay as we are here
    m_pAgent->ComparisonChanged(result.second > 0.6);
    m_pAgent->DelayChanged(std::chrono::milliseconds(result.first*1000/m_nSampleRate));
}

int Compi::Run(const std::string& sPath)
{
    if(m_iniConfig.ReadIniFile(sPath+"/compi.ini"))
    {
        SetupLogging();

        pml::Log::Get(pml::Log::LOG_INFO) << "compi started" << std::endl;

        SetupAgent();
        SetupRecorder();

        Loop();

        return 0;

    }
    return -1;
}
