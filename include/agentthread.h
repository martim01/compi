#pragma once
#include <mutex>
#include <set>

namespace Agentpp
{
    class Mib;
    class RequestList;
    class Snmpx;
    class MibStaticTable;
};


static bool g_bRun= true;

class AgentThread
{
    public:
        AgentThread(int nPort, int nPortTrap, const std::string& sCommunity="public");
        ~AgentThread();

        void Init();
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
        Agentpp::MibStaticTable* m_pTable;

        std::mutex m_mutex;

        int m_nPortTrap;
	std::string m_sCommunity;

        std::set<std::string> m_setTrapDestination;

        static const std::string OID_BASE;
        static const std::string OID_BASE_TRAP;
        static const std::string OID_AUDIO;
        static const std::string OID_COMPARISON;
        static const std::string OID_DELAY;
};
