#ifndef RDT_MANAGER_H
#define RDT_MANAGER_H

#include "intel_rdt.h"

typedef struct VMRdtInfo
{
    set<int> cpuset;
    int vote_cpu;
    uint32_t rmid;
    uint32_t cosid;
    uint32_t cat_way;
    uint32_t mba_rate;
} VMRdtInfo;

class RdtManager
{
public:
    uint32_t nr_rmid;
    vector<int> rmid_bitmap = vector<int>(nr_sockets, 1);
    vector<int> cosid_bitmap = vector<int>(nr_sockets, 1);
    uint32_t cat_nr_way;
    uint32_t nr_cosid;
    uint32_t mba_rate_max;

    map<int, map<string, VMRdtInfo>> vm_rdtinfo;

    RdtManager()
    {
        for (int socket = 0; socket < nr_sockets; socket++)
        {
            rdtcat.disable_cat_cdp(socket_vote_cpu[socket]);
        }
        this->nr_rmid = rdtmon.nr_rmid;

        // cat & mba share the same cosid
        this->nr_cosid = min(rdtcat.cat_nr_cosid, rdtmba.mba_nr_cosid);
        // key parameters
        this->cat_nr_way = rdtcat.cat_nr_way;
        this->mba_rate_max = rdtmba.mba_rate_max;
        //this->vm_rdtinfo = [dict() for _ in range(nr_sockets)]
        // this->logger = get_logger("memory_contention")
    }
    // INFO MANAGE VM_RDTINFO
    void init_vm_rdtinfo(int socket, string vmid, set<int> cpuset)
    {
        VMRdtInfo vm_rdtinfo;

        vm_rdtinfo.cpuset = cpuset;
        vm_rdtinfo.vote_cpu = *cpuset.begin(); //
        vm_rdtinfo.rmid = RMID0;
        vm_rdtinfo.cosid = COSID0;
        vm_rdtinfo.cat_way = this->cat_nr_way;
        vm_rdtinfo.mba_rate = this->mba_rate_max;

        this->vm_rdtinfo[socket][vmid] = vm_rdtinfo;
    }

    void reset_vm_rdtinfo(int socket, string vmid, bool reset_rmid = false)
    {
        this->vm_rdtinfo[socket][vmid].cosid = COSID0;
        this->vm_rdtinfo[socket][vmid].cat_way = this->cat_nr_way;
        this->vm_rdtinfo[socket][vmid].mba_rate = this->mba_rate_max;
        if (reset_rmid)
            this->vm_rdtinfo[socket][vmid].rmid = RMID0;
    }

    // INFO PERFORMANCE COUNTERS
    double vm_llc_occupancy(int socket, string vmid)
    {
        int vote_cpu = this->vm_rdtinfo[socket][vmid].vote_cpu;
        return rdtmon.get_llc_occupancy(vote_cpu) * 1.0 / pow(1024.0, 2);
    }

    uint32_t rmid_alloc(int socket)
    {
        uint32_t rmid = RMID0;

        for (uint32_t id = 1; id < nr_rmid; id++)
        {
            if ((this->rmid_bitmap[socket] & (1 << id)) == 0)
            {
                rmid = id;
                this->rmid_bitmap[socket] |= (1 << id);
                break;
            }
        }
        return rmid;
    }

    uint32_t cosid_alloc(int socket)
    {
        uint32_t cosid = COSID0;
        for (uint32_t id = 1; id < nr_cosid; id++)
        {
            if ((cosid_bitmap[socket] & (1 << id)) == 0)
            {
                cosid = id;
                this->cosid_bitmap[socket] |= (1 << id);
                break;
            }
        }
        return cosid;
    }

    bool rmid_free(int socket, string vmid)
    {
        uint32_t rmid = this->vm_rdtinfo[socket][vmid].rmid;
        if (rmid == RMID0)
            return false;
        for (auto iter = vm_rdtinfo[socket][vmid].cpuset.begin(); iter != vm_rdtinfo[socket][vmid].cpuset.end(); ++iter)
            rdtmon.reset_rmid(*iter);

        this->rmid_bitmap[socket] &= ~(1 << rmid);
        //this->rdtdebug("rmid free", socket, vmid, rmid, header = "rdtmon")
        return true;
    }

    bool cosid_free(int socket, string vmid)
    {
        uint32_t cosid = this->vm_rdtinfo[socket][vmid].cosid;
        if (cosid == COSID0)
            return false;
        rdtcat.reset_cat_cbm(socket_vote_cpu[socket], cosid);
        rdtmba.reset_mba_available(socket_vote_cpu[socket], cosid);
        for (auto iter = vm_rdtinfo[socket][vmid].cpuset.begin(); iter != vm_rdtinfo[socket][vmid].cpuset.end(); ++iter)
            rdtmba.reset_cosid(*iter);

        this->cosid_bitmap[socket] &= ~(1 << cosid);
        //this->rdtdebug("cosid free", socket, vmid, cosid, header="rdtalloc")
        return true;
    }

    // INFO VM MONITORING & ISOLATION
    bool monitor_vm(int socket, string vmid)
    {
        if (this->vm_rdtinfo[socket][vmid].rmid != RMID0)
            return true;
        uint32_t rmid = this->rmid_alloc(socket);
        DEBUG_LOG("rmid %d", rmid);
        if (rmid == RMID0)
            //this->rdtdebug("run out of rmid", header="error")
            return false;
        for (auto iter = vm_rdtinfo[socket][vmid].cpuset.begin(); iter != vm_rdtinfo[socket][vmid].cpuset.end(); ++iter)
        {
            rdtmon.set_rmid(*iter, rmid);
            DEBUG_LOG("CPU %d, rmid %d", *iter, rmid);
        }

        this->vm_rdtinfo[socket][vmid].rmid = rmid;
        //this->rdtdebug(socket, vmid, rmid, this->vm_rdtinfo[socket][vmid]["cpuset"], header="rdtmon")
        return true;
    }

    bool isolate_vm_llc(int socket, string vmid, uint32_t cat_way, int cat_cbm = -1)
    {
        int vote_cpu = this->vm_rdtinfo[socket][vmid].vote_cpu;
        uint32_t cosid = this->vm_rdtinfo[socket][vmid].cosid;
        if (cosid == COSID0)
        {
            cosid = this->cosid_alloc(socket);
            DEBUG_LOG("cosid %d", cosid);
            if (cosid == COSID0)
                //this->rdtdebug("run out of cosid", header="error")
                return false;
            for (auto iter = vm_rdtinfo[socket][vmid].cpuset.begin(); iter != vm_rdtinfo[socket][vmid].cpuset.end(); ++iter)
                rdtcat.set_cosid(*iter, cosid);

            this->vm_rdtinfo[socket][vmid].cosid = cosid;
            //this->rdtdebug(socket, vmid, cosid, this->vm_rdtinfo[socket][vmid]["cpuset"], header="rdtalloc")
        }
        if (cat_cbm == -1 || !rdtcat.is_cbm_contiguous(cat_cbm))
            cat_cbm = (1 << cat_way) - 1;
        rdtcat.set_cat_cbm(vote_cpu, cosid, cat_cbm);
        this->vm_rdtinfo[socket][vmid].cat_way = cat_way;
        //this->rdtdebug(socket, vmid, cosid, cat_way, header="rdtcat")
        return true;
    }

    bool isolate_vm_mem(int socket, string vmid, int mba_rate)
    {
        int vote_cpu = this->vm_rdtinfo[socket][vmid].vote_cpu;
        uint32_t cosid = this->vm_rdtinfo[socket][vmid].cosid;
        if (cosid == COSID0)
        {
            cosid = this->cosid_alloc(socket);
            if (cosid == COSID0)
                //this->rdtdebug("run out of cosid", header="error")
                return false;
            for (auto iter = vm_rdtinfo[socket][vmid].cpuset.begin(); iter != vm_rdtinfo[socket][vmid].cpuset.end(); ++iter)
                rdtcat.set_cosid(*iter, cosid);
            this->vm_rdtinfo[socket][vmid].cosid = cosid;
            //this->rdtdebug(socket, vmid, cosid, this->vm_rdtinfo[socket][vmid]["cpuset"], header="rdtalloc")
        }
        rdtmba.set_mba_available(vote_cpu, cosid, mba_rate);
        // we are NOT absolutely sure about effective mba rate
        this->vm_rdtinfo[socket][vmid].mba_rate = rdtmba.get_mba_available(vote_cpu, cosid);
        //this->rdtdebug(socket, vmid, cosid, this->vm_rdtinfo[socket][vmid]["mba_rate"], header="rdtmba")
        return true;
    }

    void disable_mba()
    {
        rdtmba.clean_up();
        for (int socket = 0; socket < nr_sockets; socket++)
            for (auto iter = vm_rdtinfo[socket].begin(); iter != vm_rdtinfo[socket].end(); ++iter)
                this->vm_rdtinfo[socket][iter->first].mba_rate = this->mba_rate_max;
    }
};

RdtManager rdtmgr;

#endif
