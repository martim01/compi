#include "mibwritabletable.h"

#include "log.h"
using namespace Agentpp;


MibWritableTable::MibWritableTable(const Oidx& o): MibComplexEntry(o, READWRITE)
{
}

MibWritableTable::MibWritableTable(MibWritableTable& other):
    MibComplexEntry(other)
{
    OidListCursor<MibWritableEntry> cur;
    for (cur.init(&other.contents); cur.get(); cur.next())
    {
        contents.add(new MibWritableEntry(*cur.get()));
    }
}

MibWritableTable::~MibWritableTable()
{
}

void MibWritableTable::add(const MibWritableEntry& entry)
{
    start_synch();
    Oidx tmpoid(entry.get_oid());
    MibWritableEntry* newEntry = new MibWritableEntry(entry);
    // TODO: added here for backward compatibility, should be removed
    // later
    if (oid.is_root_of(tmpoid))
    {
        tmpoid = tmpoid.cut_left(oid.len());
        newEntry->set_oid(tmpoid);
    }
    MibWritableEntry* ptr = contents.find(&tmpoid);
    if (ptr)
    {
        contents.remove(&tmpoid);
    }
    contents.add(newEntry);
    end_synch();
}

void MibWritableTable::remove(const Oidx& o)
{
    start_synch();
    Oidx tmpoid(o);
    if (oid.is_root_of(tmpoid))
    {
        tmpoid = tmpoid.cut_left(oid.len());
        contents.remove(&tmpoid);
    }
    end_synch();
}

MibWritableEntry* MibWritableTable::get(const Oidx& o, bool suffixOnly)
{
    Oidx tmpoid(o);
    if (!suffixOnly)
    {
        if (!oid.is_root_of(tmpoid))
            return 0;
        tmpoid = tmpoid.cut_left(oid.len());
    }
    return contents.find(&tmpoid);
}

Oidx MibWritableTable::find_succ(const Oidx& o, Request*)
{
    start_synch();
    Oidx tmpoid(o);
    Oidx retval;
    if (tmpoid <= oid)
    {
        tmpoid = Oidx();
    }
    else if (tmpoid.len() >= oid.len())
    {
        tmpoid = tmpoid.cut_left(oid.len());
    }
    else
    {
        end_synch();
        return retval;
    }
    MibWritableEntry* ptr = contents.find_upper(&tmpoid);
    if ((ptr) && (*ptr->key() == tmpoid))
    {
        ptr = contents.find_next(&tmpoid);
    }
    if (ptr)
    {
        retval = oid;
        retval += ptr->get_oid();
    }
    end_synch();
    return retval;
}

MibWritableEntry* MibWritableTable::GetEntry(Request* req, int ind)
{
    Oidx tmpoid(req->get_oid(ind));
    if (oid.is_root_of(tmpoid))
    {
        tmpoid = tmpoid.cut_left(oid.len());
    }
    else
    {
        Vbx vb(req->get_oid(ind));
        vb.set_syntax(sNMP_SYNTAX_NOSUCHOBJECT);
        // error status (v1) will be set by RequestList
        req->finish(ind, vb);
        return nullptr;
    }

    MibWritableEntry* entry = contents.find(&tmpoid);
    if (!entry)
    {
        Vbx vb(req->get_oid(ind));
        if (tmpoid.len() == 0)
        {
            vb.set_syntax(sNMP_SYNTAX_NOSUCHOBJECT);
        }
        else
        {
            Oidx columnID;
            columnID = tmpoid[0];
            entry = contents.find_upper(&columnID);
            if (entry)
                vb.set_syntax(sNMP_SYNTAX_NOSUCHINSTANCE);
            else
                vb.set_syntax(sNMP_SYNTAX_NOSUCHOBJECT);
        }
        // error status (v1) will be set by RequestList
        req->finish(ind, vb);
    }
    return entry;
}

void MibWritableTable::get_request(Request* req, int ind)
{
    MibWritableEntry* entry = GetEntry(req, ind);
    if(entry)
    {
        Oidx id(oid);
        id += *entry->key();
        Vbx vb(*entry);
        vb.set_oid(id);
        req->finish(ind, vb);
    }
}

void MibWritableTable::get_next_request(Request* req, int ind)
{
    Oidx tmpoid(req->get_oid(ind));
    if (oid.is_root_of(tmpoid))
    {
        tmpoid = tmpoid.cut_left(oid.len());
    }
    else
    {
        Vbx vb(req->get_oid(ind));
        vb.set_syntax(sNMP_SYNTAX_NOSUCHOBJECT);
        // error status (v1) will be set by RequestList
        req->finish(ind, vb);
    }
    MibWritableEntry* entry = contents.find_upper(&tmpoid);
    if (!entry)
    {
        Vbx vb(req->get_oid(ind));
        // TODO: This error status is just a guess, we cannot
        // determine exactly whether it is a noSuchInstance or
        // noSuchObject. May be a subclass could do a better
        // job by knowing more details from the MIB structure?
        vb.set_syntax(sNMP_SYNTAX_NOSUCHINSTANCE);
        // error status (v1) will be set by RequestList
        req->finish(ind, vb);
    }
    else
    {
        Oidx id(oid);
        id += *entry->key();
        Vbx vb(*entry);
        vb.set_oid(id);
        req->finish(ind, vb);
    }
}


int MibWritableTable::commit_set_request(Request* req, int ind)
{
    pml::Log::Get(pml::Log::LOG_TRACE) << "commit_set_request" << std::endl;
    MibWritableEntry* pEntry = GetEntry(req, ind);
    if(pEntry)
    {
        Vbx vb = req->get_value(ind);
        SnmpSyntax* pValue = vb.clone_value();

        if(pEntry->GetCallback()(pValue, pEntry->get_syntax()) == false)
        {
            delete pValue;
            return SNMP_ERROR_WRONG_VALUE;
        }

        pEntry->set_value(*pValue);

        delete pValue;
        return SNMP_ERROR_SUCCESS;
    }
    return SNMP_ERROR_NO_SUCH_NAME;


}

int	MibWritableTable::prepare_set_request(Request* req, int& ind)
{
    pml::Log::Get(pml::Log::LOG_TRACE) << "prepare_set_request" << std::endl;
    MibWritableEntry* pEntry = GetEntry(req, ind);
    if(pEntry)
    {
        pml::Log::Get(pml::Log::LOG_TRACE) << " entry found: " << pEntry->get_oid().get_printable() << std::endl;

        if(pEntry->GetCallback() == nullptr)
        {
            pml::Log::Get(pml::Log::LOG_TRACE) << " entry not writeable " << std::endl;
            return SNMP_ERROR_NOT_WRITEABLE;
        }

        pml::Log::Get(pml::Log::LOG_TRACE) << " syntax of entry: " << pEntry->get_syntax() << " of request: " << req->get_syntax(ind) << std::endl;

        if(pEntry->get_syntax() != req->get_syntax(ind))
        {
            return SNMP_ERROR_WRONG_TYPE;
        }
        else
        {
            return SNMP_ERROR_SUCCESS;
        }
    }
    else
    {
        return SNMP_ERROR_NO_SUCH_NAME;
    }
}

int	MibWritableTable::undo_set_request(Request* req, int& ind)
{
    pml::Log::Get(pml::Log::LOG_TRACE) << "undo_set_request" << std::endl;
    return SNMP_ERROR_SUCCESS;
}

void MibWritableTable::cleanup_set_request(Request* req, int& ind)
{
    pml::Log::Get(pml::Log::LOG_TRACE) << "cleanup_set_request" << std::endl;

}
