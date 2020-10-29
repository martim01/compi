#pragma once
#include <mutex>
#include <set>
#include <thread>
#include <atomic>
#include <functional>


namespace Agentpp
{
    class Mib;
    class RequestList;
    class Snmpx;
    class MibWritableTable;
};

namespace Snmp_pp
{
    class SnmpSyntax;
};

extern bool g_bRun;

class AgentThread
{
    public:
        AgentThread(int nPort, int nPortTrap,const std::string& sBaseOid, const std::string& sCommunity="public");
        ~AgentThread();

        void Init(std::function<bool(Snmp_pp::SnmpSyntax*, int)> maskCallback, std::function<bool(Snmp_pp::SnmpSyntax*, int)> activateCallback, unsigned int nMaskLevel);
        void Run();

        void AddTrapDestination(const std::string& sIpAddress);
        void RemoveTrapDestination(const std::string& sIpAddress);

        void AudioChanged(int nState);
        void ComparisonChanged(bool bSame);
        void DelayChanged(std::chrono::milliseconds delay);



    private:
        void InitTraps();
        void ThreadLoop();

        void SendTrap(int nValue, const std::string& sOid);


        Agentpp::Snmpx* m_pSnmp;
        Agentpp::Mib* m_pMib;
        Agentpp::RequestList* m_pReqList;
        Agentpp::MibWritableTable* m_pTable;

        std::mutex m_mutex;

        int m_nPortTrap;

        std::string m_sBaseOid;
        std::string m_sCommunity;

        std::set<std::string> m_setTrapDestination;

        std::unique_ptr<std::thread> m_pThread;

        static const std::string OID_AUDIO;
        static const std::string OID_COMPARISON;
        static const std::string OID_DELAY;
        static const std::string OID_MASK;
        static const std::string OID_ACTIVATE;
};
