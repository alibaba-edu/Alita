#include "lib/intel_rdt.h"

int main()
{
    init_topology();
    nr_sockets = socket_topo.size();
    nr_cpus = topo_cpu.size();

    //if(RdtMonitoring::discover_pqm_cap()) 
    RdtAllocation rdtalloc = RdtAllocation();
    unsigned long long cosid;
    
    rdtalloc.set_cosid(0, 1);
    cosid = rdtalloc.get_cosid(0);
    assert(cosid == 1);
    
    rdtalloc.set_cosid(16, 6);
    cosid = rdtalloc.get_cosid(16);
    assert(cosid == 6);

    rdtalloc.reset_all_cosid();
    cosid = rdtalloc.get_cosid(0);
    assert(cosid == 0);
    cosid = rdtalloc.get_cosid(16);
    assert(cosid == 0);

    RdtCAT rdtcat = RdtCAT();
    unsigned long long cbm;
    cbm = rdtcat.get_cat_cbm(0, 1);
    cout << hex <<  cbm << endl;

    rdtcat.set_cat_cbm(24, 1, 0x3f);
    cbm = rdtcat.get_cat_cbm(24, 1);
    assert(cbm == 0x3f);

    cout.unsetf(ios::hex);
    RdtMBA rdtmba = RdtMBA();
    uint64_t mba_rate;
    mba_rate = rdtmba.get_mba_available(24, 1);
    cout << mba_rate << endl;

    rdtmba.set_mba_available(24, 1, 20);
    mba_rate = rdtmba.get_mba_available(24, 1);
    cout <<mba_rate << endl;
    assert(mba_rate == 20);
    
    rdtmon.set_rmid(24, 1);
    
    return 0;
}