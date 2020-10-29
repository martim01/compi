#pragma once
#include <functional>
#include <agent_pp/mib_complex_entry.h>

namespace Agentpp
{

class MibWritableEntry: public Vbx {
 public:
	MibWritableEntry(const Vbx& v, std::function<bool(SnmpSyntax*, int)> callback=nullptr): Vbx(v), m_callback(callback) { }
	MibWritableEntry(const Oidx& o, const NS_SNMP SnmpSyntax& v, std::function<bool(SnmpSyntax*, int)> callback=nullptr): Vbx(o),
	m_callback(callback)
	  { set_value(v); }
	MibWritableEntry(const MibWritableEntry& other): Vbx(other), m_callback(other.GetCallback()) { }

	OidxPtr		key() { return (Oidx*)&iv_vb_oid; }

	std::function<bool(SnmpSyntax*, int)> GetCallback() const { return m_callback;}

    private:
    std::function<bool(SnmpSyntax*, int)> m_callback;
};


class MibWritableTable  : public MibComplexEntry
{
    public:
        MibWritableTable(const Oidx&);
        MibWritableTable(MibWritableTable&);
        virtual ~MibWritableTable();

        virtual MibEntry*	clone()
        {
            return new MibWritableTable(*this);
        }

        virtual int	commit_set_request(Request*, int) override;
        virtual int	prepare_set_request(Request*, int&) override;
        virtual int	undo_set_request(Request*, int&) override;
        virtual void cleanup_set_request(Request*, int&) override;

        virtual void add(const MibWritableEntry&);
        virtual void remove(const Oidx&);
        virtual MibWritableEntry* get(const Oidx&, bool suffixOnly=FALSE);
        virtual Oidx find_succ(const Oidx&, Request* req = 0);
        virtual void get_request(Request*, int);
        virtual void get_next_request(Request*, int);

    protected:

        MibWritableEntry* GetEntry(Request*, int);

        OidList<MibWritableEntry>		contents;

};
}

