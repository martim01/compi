#include "agentthread.h"
#include <agent_pp/agent++.h>
#include <agent_pp/snmp_group.h>
#include <agent_pp/system_group.h>
#include <agent_pp/snmp_target_mib.h>
#include <agent_pp/snmp_notification_mib.h>
#include <agent_pp/notification_originator.h>
#include <agent_pp/mib_complex_entry.h>
#include <agent_pp/v3_mib.h>
#include <agent_pp/vacm.h>
#include <snmp_pp/oid_def.h>
#include <snmp_pp/mp_v3.h>
#include <snmp_pp/log.h>
#include <thread>
#include <sstream>
#include "log.h"
#include <iomanip>

using namespace Snmp_pp;
using namespace Agentpp;

static const char* loggerModuleName = "compi.static_table";

const std::string AgentThread::OID_BASE = "1.3.6.1.4.1.2333.3.2.741.1";
const std::string AgentThread::OID_BASE_TRAP = "1.3.6.1.4.1.2333.3.2.741.2";
const std::string AgentThread::OID_AUDIO = ".1";
const std::string AgentThread::OID_COMPARISON = ".2";
const std::string AgentThread::OID_DELAY = ".3";

AgentThread::AgentThread(int nPort, int nPortTrap, const std::string& sCommunity) :
    m_nPortTrap(nPortTrap),
    m_sCommunity(sCommunity)
{
    int nStatus;
    Snmp::socket_startup();  // Initialize socket subsystem
    m_pSnmp = new Snmpx(nStatus, nPort);

    DefaultLog::log()->set_filter(ERROR_LOG,0);
    DefaultLog::log()->set_filter(WARNING_LOG,0);
    DefaultLog::log()->set_filter(EVENT_LOG,0);
    DefaultLog::log()->set_filter(INFO_LOG,0);
    DefaultLog::log()->set_filter(DEBUG_LOG,0);

    if (nStatus == SNMP_CLASS_SUCCESS)
    {

        LOG_BEGIN(loggerModuleName, EVENT_LOG | 1);
        LOG("main: SNMP listen port");
        LOG(nPort);
        LOG_END;
    }
    else
    {
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 0);
        LOG("main: SNMP port init failed");
        LOG(nStatus);
        LOG_END;
        exit(1);
    }

    m_pMib = new Mib();
    m_pReqList = new RequestList();
    m_pReqList->set_read_community(m_sCommunity.c_str());
    m_pReqList->set_write_community(m_sCommunity.c_str());

  m_pReqList->set_address_validation(true);

    // register requestList for outgoing requests
    m_pMib->set_request_list(m_pReqList);


    // add supported objects
    Init();
    // load persitent objects from disk
    m_pMib->init();

    m_pReqList->set_snmp(m_pSnmp);

}

AgentThread::~AgentThread()
{
    delete m_pMib;
    delete m_pSnmp;
    Snmp::socket_cleanup();  // Shut down socket subsystem
}

void AgentThread::Init()
{
    m_pMib->add(new sysGroup("compi SNMP Agent",OID_BASE.c_str(), 10));
    m_pMib->add(new snmpGroup());
    m_pMib->add(new snmp_target_mib());
    m_pMib->add(new snmp_notification_mib());

    m_pTable = new MibStaticTable((OID_BASE+".1").c_str());
    //message.channel.side
    m_pTable->add(MibStaticEntry(OID_AUDIO.c_str(), SnmpInt32(-1)));  //audio present
    m_pTable->add(MibStaticEntry(OID_COMPARISON.c_str(), SnmpInt32(-1)));  // comparioson
    m_pTable->add(MibStaticEntry(OID_DELAY.c_str(), SnmpInt32(-1)));  // comparioson


    m_pMib->add(m_pTable);

}

void AgentThread::Run()
{
    InitTraps();
    std::thread th(&AgentThread::ThreadLoop, this);
    th.detach();
}

void AgentThread::InitTraps()
{
    //send cold start OID
    Vbx* pVbs = 0;
    coldStartOid coldOid;
    NotificationOriginator no;
    for(std::set<std::string>::const_iterator itDest = m_setTrapDestination.begin(); itDest != m_setTrapDestination.end(); ++itDest)
    {
        std::stringstream ssDest;
        ssDest << (*itDest) << "/" << m_nPortTrap;

        UdpAddress dest(ssDest.str().c_str());
        no.add_v2_trap_destination(dest, "start", "start", m_sCommunity.c_str());
    }
    no.generate(pVbs, 0, coldOid, "", "");
}

void AgentThread::ThreadLoop()
{

    Request* req;
    while (g_bRun)
    {
	    req = m_pReqList->receive(1);
	    if (req)
        {
//          std::lock_guard<std::mutex> lg(m_mutex);
            m_pMib->process_request(req);
        }
        else
        {
            m_pMib->cleanup();
        }
    }
    pml::Log::Get(pml::Log::LOG_INFO) << "AgentThread\tExiting" << std::endl;

}

void AgentThread::AudioChanged(int nState)
{
    std::lock_guard<std::mutex> lg(m_mutex);

    MibStaticEntry* pEntry = m_pTable->get(OID_AUDIO.c_str(), true);
    if(pEntry)
    {
        int nCurrent(-1);
        pEntry->get_value(nCurrent);
        if(nCurrent != nState)
        {
  	        pml::Log::Get(pml::Log::LOG_DEBUG) << "AgentThread\tAudioChanged: " << nState << std::endl;
            pEntry->set_value(SnmpInt32(nState));
            SendTrap(nState, OID_AUDIO);
        }
    }
    else
    {
        pml::Log::Get(pml::Log::LOG_WARN)  << "AgentThread\tAudioChanged:  OID Not Found!" << std::endl;
    }
}

void AgentThread::ComparisonChanged(bool bSame)
{
    std::lock_guard<std::mutex> lg(m_mutex);

    MibStaticEntry* pEntry = m_pTable->get(OID_COMPARISON.c_str(), true);
    if(pEntry)
    {
        int nCurrent(-1);
        pEntry->get_value(nCurrent);
        if(nCurrent != static_cast<int>(bSame))
        {
  	        pml::Log::Get(pml::Log::LOG_DEBUG) << "AgentThread\tComparisonChanged: " << bSame << std::endl;
            pEntry->set_value(SnmpInt32(bSame));
            SendTrap(static_cast<int>(bSame), OID_COMPARISON);
        }
    }
    else
    {
        pml::Log::Get(pml::Log::LOG_WARN)  << "AgentThread\tComparisonChanged:  OID Not Found!" << std::endl;
    }

}

void AgentThread::DelayChanged(std::chrono::milliseconds delay)
{
    std::lock_guard<std::mutex> lg(m_mutex);

    MibStaticEntry* pEntry = m_pTable->get(OID_DELAY.c_str(), true);
    if(pEntry)
    {
        int nCurrent(-1);
        pEntry->get_value(nCurrent);
        if(nCurrent != delay.count())
        {
  	        pml::Log::Get(pml::Log::LOG_DEBUG) << "AgentThread\tDelayChanged: " << delay.count() << std::endl;
            pEntry->set_value(SnmpInt32(delay.count()));
            SendTrap(delay.count(), OID_DELAY);
        }
    }
    else
    {
        pml::Log::Get(pml::Log::LOG_WARN)  << "AgentThread\tDelayChanged:  OID Not Found!" << std::endl;
    }

}

void AgentThread::AddTrapDestination(const std::string& sIpAddress)
{
    std::lock_guard<std::mutex> lg(m_mutex);
    m_setTrapDestination.insert(sIpAddress);
    pml::Log::Get(pml::Log::LOG_INFO)  << "AgentThread\tTrap destination " << sIpAddress << " added" << std::endl;
}

void AgentThread::RemoveTrapDestination(const std::string& sIpAddress)
{
    std::lock_guard<std::mutex> lg(m_mutex);
    m_setTrapDestination.erase(sIpAddress);
    pml::Log::Get(pml::Log::LOG_INFO)  << "AgentThread\tTrap destination " << sIpAddress << " removed" << std::endl;
}


void AgentThread::SendTrap(int nValue, const std::string& sOid)
{
    Vbx* pVbs = new Vbx[1];
    Oidx rdsOid((OID_BASE_TRAP+"."+sOid).c_str());

    pVbs[0].set_oid((OID_BASE_TRAP+"."+sOid).c_str());
    pVbs[0].set_value(SnmpInt32(nValue));

    NotificationOriginator no;


    for(std::set<std::string>::const_iterator itDest = m_setTrapDestination.begin(); itDest != m_setTrapDestination.end(); ++itDest)
    {
        std::stringstream ssDest;
        ssDest << (*itDest) << "/" << m_nPortTrap;

        UdpAddress dest(ssDest.str().c_str());
        no.add_v2_trap_destination(dest, "start", "start", m_sCommunity.c_str());

        pml::Log::Get(pml::Log::LOG_DEBUG)  << "AgentThread\tTrap " << sOid << " sent to " << ssDest.str() << std::endl;
    }
    no.generate(pVbs, 1, rdsOid, "", "");

    delete[] pVbs;
}
