#include "agentthread.h"
#include <agent_pp/agent++.h>
#include <agent_pp/snmp_group.h>
#include <agent_pp/system_group.h>
#include <agent_pp/snmp_target_mib.h>
#include <agent_pp/snmp_notification_mib.h>
#include <agent_pp/notification_originator.h>
#include <agent_pp/v3_mib.h>
#include <agent_pp/vacm.h>
#include <snmp_pp/oid_def.h>
#include <snmp_pp/mp_v3.h>
#include <snmp_pp/log.h>
#include <thread>
#include <sstream>
#include "log.h"
#include <iomanip>
#include "mibwritabletable.h"

using namespace Snmp_pp;
using namespace Agentpp;

static const char* loggerModuleName = "compi.static_table";

const std::string AgentThread::OID_AUDIO = ".1";
const std::string AgentThread::OID_COMPARISON = ".2";
const std::string AgentThread::OID_DELAY = ".3";
const std::string AgentThread::OID_MASK = ".4";
const std::string AgentThread::OID_ACTIVATE = ".5";
const std::string AgentThread::OID_OVERALL = ".6";
const std::string AgentThread::OID_SILENCE_A_LEG = ".7";
const std::string AgentThread::OID_SILENCE_B_LEG = ".8";

bool g_bRun = true;

AgentThread::AgentThread(int nPort, int nPortTrap, const std::string& sBaseOid, const std::string& sCommunity) :
    m_nPortTrap(nPortTrap),
    m_sBaseOid(sBaseOid),
    m_sCommunity(sCommunity),
    m_pThread(nullptr)
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
        pmlLog() << "SNMP Started";
    }
    else
    {
        pmlLog(pml::LOG_CRITICAL) << "SNMP Failed To Start!";
        exit(1);
    }

    m_pMib = new Mib();
    m_pReqList = new RequestList();
    m_pReqList->set_read_community(m_sCommunity.c_str());
    m_pReqList->set_write_community(m_sCommunity.c_str());

    m_pReqList->set_address_validation(true);

    // register requestList for outgoing requests
    m_pMib->set_request_list(m_pReqList);




}

AgentThread::~AgentThread()
{
    if(m_pThread)
    {
        m_pThread->join();
    }
    delete m_pMib;
    delete m_pSnmp;
    Snmp::socket_cleanup();  // Shut down socket subsystem
}

void AgentThread::Init(std::function<bool(Snmp_pp::SnmpSyntax*, int)> maskCallback, std::function<bool(Snmp_pp::SnmpSyntax*, int)> activateCallback, unsigned int nMaskLevel)
{
    m_pMib->add(new sysGroup("compi SNMP Agent",m_sBaseOid.c_str(), 10));
    m_pMib->add(new snmpGroup());
    m_pMib->add(new snmp_target_mib());
    m_pMib->add(new snmp_notification_mib());

    m_pTable = new MibWritableTable((m_sBaseOid+".1").c_str());
    //message.channel.sid
    m_pTable->add(MibWritableEntry(OID_AUDIO.c_str(), SnmpInt32(-1)));  //audio present
    m_pTable->add(MibWritableEntry(OID_COMPARISON.c_str(), SnmpInt32(-1)));  // comparioson
    m_pTable->add(MibWritableEntry(OID_DELAY.c_str(), SnmpInt32(-1)));  // comparioson
    m_pTable->add(MibWritableEntry(OID_OVERALL.c_str(), SnmpInt32((nMaskLevel == 2 ? 1 : 0))));  // comparioson

    m_pTable->add(MibWritableEntry(OID_MASK.c_str(), SnmpInt32(nMaskLevel), maskCallback));  // comparioson
    m_pTable->add(MibWritableEntry(OID_ACTIVATE.c_str(), SnmpInt32(0), activateCallback));  // comparioson


    m_pTable->add(MibWritableEntry(OID_SILENCE_A_LEG.c_str(), SnmpInt32(-1)));  // silence
    m_pTable->add(MibWritableEntry(OID_SILENCE_B_LEG.c_str(), SnmpInt32(-1)));  // silence

    m_pMib->add(m_pTable);

    // load persitent objects from disk
    m_pMib->init();

    m_pReqList->set_snmp(m_pSnmp);

}

void AgentThread::Run()
{
    InitTraps();
    m_pThread = std::make_unique<std::thread>(&AgentThread::ThreadLoop, this);

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
            m_pMib->process_request(req);
        }
        else
        {
            m_pMib->cleanup();
        }
    }
    pmlLog(pml::LOG_INFO) << "AgentThread\tExiting";

}

void AgentThread::AudioChanged(int nState)
{
    std::lock_guard<std::mutex> lg(m_mutex);

    MibWritableEntry* pEntry = m_pTable->get(OID_AUDIO.c_str(), true);
    if(pEntry)
    {
        int nCurrent(-1);
        pEntry->get_value(nCurrent);
        if(nCurrent != nState)
        {
  	        pmlLog(pml::LOG_DEBUG) << "AgentThread\tAudioChanged: " << nState;
            pEntry->set_value(SnmpInt32(nState));
            SendTrap(nState, OID_AUDIO);
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN)  << "AgentThread\tAudioChanged:  OID Not Found!";
    }
}


void AgentThread::OverallChanged(bool bActive)
{
    std::lock_guard<std::mutex> lg(m_mutex);

    MibWritableEntry* pEntry = m_pTable->get(OID_OVERALL.c_str(), true);
    if(pEntry)
    {
        int nCurrent(-1);
        pEntry->get_value(nCurrent);
        if(nCurrent != static_cast<int>(bActive))
        {
  	        pmlLog(pml::LOG_DEBUG) << "AgentThread\tOverallChanged: " << bActive;
            pEntry->set_value(SnmpInt32(bActive));
            SendTrap(static_cast<int>(bActive), OID_OVERALL);
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN)  << "AgentThread\tOverallChanged:  OID Not Found!";
    }
}


void AgentThread::ComparisonChanged(bool bSame)
{
    std::lock_guard<std::mutex> lg(m_mutex);

    MibWritableEntry* pEntry = m_pTable->get(OID_COMPARISON.c_str(), true);
    if(pEntry)
    {
        int nCurrent(-1);
        pEntry->get_value(nCurrent);
        if(nCurrent != static_cast<int>(bSame))
        {
  	        pmlLog(pml::LOG_DEBUG) << "AgentThread\tComparisonChanged: " << bSame;
            pEntry->set_value(SnmpInt32(bSame));
            SendTrap(static_cast<int>(bSame), OID_COMPARISON);
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN)  << "AgentThread\tComparisonChanged:  OID Not Found!";
    }
}

void AgentThread::SilenceChanged(bool bSilent, int nLeg)
{
    std::lock_guard<std::mutex> lg(m_mutex);

    std::string sOid = nLeg==0 ? OID_SILENCE_A_LEG : OID_SILENCE_B_LEG;

    MibWritableEntry* pEntry = m_pTable->get(sOid.c_str(), true);
    if(pEntry)
    {
        int nCurrent(-1);
        pEntry->get_value(nCurrent);
        if(nCurrent != static_cast<int>(bSilent))
        {
  	        pmlLog(pml::LOG_DEBUG) << "AgentThread\tSilenceChanged: " << nLeg << "=" << bSilent;
            pEntry->set_value(SnmpInt32(bSilent));
            SendTrap(static_cast<int>(bSilent), sOid);
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN)  << "AgentThread\tSilenceChanged:  OID Not Found!";
    }
}

void AgentThread::DelayChanged(std::chrono::milliseconds delay)
{
    std::lock_guard<std::mutex> lg(m_mutex);

    MibWritableEntry* pEntry = m_pTable->get(OID_DELAY.c_str(), true);
    if(pEntry)
    {
        int nCurrent(-1);
        pEntry->get_value(nCurrent);
        if(nCurrent != delay.count())
        {
  	        pmlLog(pml::LOG_DEBUG) << "AgentThread\tDelayChanged: " << delay.count();
            pEntry->set_value(SnmpInt32(delay.count()));
            SendTrap(delay.count(), OID_DELAY);
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN)  << "AgentThread\tDelayChanged:  OID Not Found!";
    }

}

void AgentThread::AddTrapDestination(const std::string& sIpAddress)
{
    std::lock_guard<std::mutex> lg(m_mutex);
    m_setTrapDestination.insert(sIpAddress);
    pmlLog(pml::LOG_INFO)  << "AgentThread\tTrap destination " << sIpAddress << " added";
}

void AgentThread::RemoveTrapDestination(const std::string& sIpAddress)
{
    std::lock_guard<std::mutex> lg(m_mutex);
    m_setTrapDestination.erase(sIpAddress);
    pmlLog(pml::LOG_INFO)  << "AgentThread\tTrap destination " << sIpAddress << " removed";
}


void AgentThread::SendTrap(int nValue, const std::string& sOid)
{
    Vbx* pVbs = new Vbx[1];
    Oidx rdsOid((m_sBaseOid+".2."+sOid).c_str());

    pVbs[0].set_oid((m_sBaseOid+".1."+sOid).c_str());
    pVbs[0].set_value(SnmpInt32(nValue));

    NotificationOriginator no;


    for(std::set<std::string>::const_iterator itDest = m_setTrapDestination.begin(); itDest != m_setTrapDestination.end(); ++itDest)
    {
        std::stringstream ssDest;
        ssDest << (*itDest) << "/" << m_nPortTrap;

        UdpAddress dest(ssDest.str().c_str());
        no.add_v2_trap_destination(dest, "start", "start", m_sCommunity.c_str());

        pmlLog(pml::LOG_DEBUG)  << "AgentThread\tTrap " << sOid << " sent to " << ssDest.str();
    }
    no.generate(pVbs, 1, rdsOid, "", "");

    delete[] pVbs;
}

