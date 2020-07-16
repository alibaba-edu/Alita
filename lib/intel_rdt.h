#ifndef INTEL_RDT_H
#define INTEL_RDT_H

#include "cpuid.h"
#include "utility.h"
#include <assert.h>
#include "cpuid.h"
#include <cmath>

// DEFAULT RMID and COSID
#define RMID0 0
#define COSID0 0

// INTEL RDT MSR
#define RDT_MSR_ASSOC 0xC8F
#define RDT_MSR_ASSOC_RMID_MASK (1 << 10) - 1
#define RDT_MSR_ASSOC_COS_SHIFT 32
#define RDT_MSR_ASSOC_COS_MASK 0xffffffff00000000

#define RDT_MSR_MON_EVTSEL 0xC8D
#define RDT_MSR_MON_EVTSEL_RMID_SHIFT 32
#define RDT_MSR_MON_EVTSEL_RMID_MASK (1 << 10) - 1
#define RDT_MSR_MON_EVTSEL_EVTID_MASK (1 << 8) - 1
#define RDT_MON_EVENT_L3_OCCUP 1
#define RDT_MON_EVENT_TMEM_BW 2
#define RDT_MON_EVENT_LMEM_BW 3
#define RDT_MSR_MON_QMC 0xC8E
#define RDT_MSR_MON_QMC_ERROR 1ULL << 63
#define RDT_MSR_MON_QMC_UNAVAILABLE 1ULL << 62
#define RDT_MSR_MON_QMC_DATA_MASK (1ULL << 62) - 1

//CONSIDER ONLY L3CA in CAT
#define RDT_MSR_CAT_CBM_START 0xC90
#define RDT_MSR_CAT_CDP_SHIFT 2
#define RDT_MSR_CAT_QOS_CFG 0xC81
#define RDT_MSR_MBA_THROTTLE_START 0xD50

class RdtMonitoring
{
public:
    uint32_t nr_rmid;
    uint32_t mon_scale_factor;
    uint32_t mbm_max_count;

    RdtMonitoring()
    {
        this->nr_rmid = CPUID::cpuid_count(0xf, 0x1, 2) + 1;
        this->mon_scale_factor = CPUID::cpuid_count(0xf, 0x1, 1);
        // initial counter size is 24 bits for mbm, see 17-48 Vol.3B
        this->mbm_max_count = 1 << 24;
    }

    static bool discover_pqm_cap()
    {
        // platform QoS monitoring capability
        if (CPUID::cpuid_count(0x7, 0x0, 1) & (1 << 12) == 0)
            return false;
        // llc monitoring capability
        if (CPUID::cpuid_count(0xf, 0x0, 3) & (1 << 1) == 0)
            return false;
        // llc occupancy, total/local memory bandwidth capability
        CPUID::cpuid_count(0xf, 0x1, 3);
        uint32_t cap_cmt = CPUID::cpuid_count(0xf, 0x1, 3) & (1 << 0);
        uint32_t cap_tmbm = CPUID::cpuid_count(0xf, 0x1, 3) & (1 << 1);
        uint32_t cap_lmbm = CPUID::cpuid_count(0xf, 0x1, 3) & (1 << 2);
        if (cap_cmt == 0 or cap_tmbm == 0 or cap_lmbm == 0)
            return true;
        return false;
    }

    uint64_t get_rmid(int cpu)
    {
        uint64_t val = read_msr(cpu, RDT_MSR_ASSOC);
        val &= RDT_MSR_ASSOC_RMID_MASK;
        return val;
    }

    void set_rmid(int cpu, int rmid)
    {
        uint64_t val = read_msr(cpu, RDT_MSR_ASSOC);
        val &= RDT_MSR_ASSOC_COS_MASK;
        val |= (rmid & RDT_MSR_ASSOC_RMID_MASK);
        write_msr(cpu, RDT_MSR_ASSOC, val);
    }

    void reset_rmid(int cpu)
    {
        set_rmid(cpu, RMID0);
    }

    // return real-time monitoring data, 0 if error
    int get_mon_data(int vote_cpu, int mon_event)
    {
        int retries = 3;
        int mon_data = 0;
        uint64_t rmid = get_rmid(vote_cpu);
        uint64_t query = (rmid & RDT_MSR_MON_EVTSEL_RMID_MASK) << RDT_MSR_MON_EVTSEL_RMID_SHIFT;
        query |= mon_event & RDT_MSR_MON_EVTSEL_EVTID_MASK;
        write_msr(vote_cpu, RDT_MSR_MON_EVTSEL, query);

        while (retries)
        {
            uint64_t val = read_msr(vote_cpu, RDT_MSR_MON_QMC);
            if (val & RDT_MSR_MON_QMC_ERROR)
            {
                break;
            }
            else if (val & RDT_MSR_MON_QMC_UNAVAILABLE)
            {
                retries -= 1;
                continue;
            }
            else
            {
                mon_data = (val & RDT_MSR_MON_QMC_DATA_MASK) * mon_scale_factor;
                break;
            }
        }
        return mon_data;
    }
    int get_llc_occupancy(int vote_cpu)
    {
        return get_mon_data(vote_cpu, RDT_MON_EVENT_L3_OCCUP);
    }

    int get_total_bandwidth(int vote_cpu)
    {
        return get_mon_data(vote_cpu, RDT_MON_EVENT_TMEM_BW);
    }

    int get_local_bandwidth(int vote_cpu)
    {
        return get_mon_data(vote_cpu, RDT_MON_EVENT_LMEM_BW);
    }

    int get_mbm_delta(int old_data, int new_data)
    {
        if (old_data > new_data)
            return mbm_max_count * mon_scale_factor - old_data + new_data;
        return new_data - old_data;
    }

    void clean_up()
    {
        for (int cpu = 0; cpu < nr_cpus; cpu++)
            reset_rmid(cpu);
    }
};

class RdtAllocation
{
public:
    uint32_t cat_nr_cosid;
    uint32_t cat_nr_way;
    uint32_t cat_way_contention;
    bool cap_cat_cdp;

    RdtAllocation()
    {
        this->cat_nr_cosid = CPUID::cpuid_count(0x10, 1, 3) + 1;
        this->cat_nr_way = CPUID::cpuid_count(0x10, 1, 0) + 1;
        this->cat_way_contention = CPUID::cpuid_count(0x10, 1, 1);
        if (CPUID::cpuid_count(0x10, 1, 2) & (1 << RDT_MSR_CAT_CDP_SHIFT) != 0)
            this->cap_cat_cdp = true;
        else
            this->cap_cat_cdp = false;
    }
    uint64_t get_cosid(int cpu)
    {
        uint64_t val = read_msr(cpu, RDT_MSR_ASSOC);
        return (val & RDT_MSR_ASSOC_COS_MASK) >> RDT_MSR_ASSOC_COS_SHIFT;
    }

    void set_cosid(int cpu, uint64_t cosid)
    {
        uint64_t val = read_msr(cpu, RDT_MSR_ASSOC);
        val &= (~RDT_MSR_ASSOC_COS_MASK);
        val |= cosid << RDT_MSR_ASSOC_COS_SHIFT;
        write_msr(cpu, RDT_MSR_ASSOC, val);
    }

    void reset_cosid(int cpu)
    {
        set_cosid(cpu, COSID0);
    }

    void reset_all_cosid()
    {
        for (int cpu = 0; cpu < nr_cpus; cpu++)
            reset_cosid(cpu);
    }

    bool is_cbm_contiguous(int mask)    //mask is cbm
    {
        if (!mask)
            return false;
        while (mask & 1 == 0)
            mask >>= 1;
        while (mask & 1 != 0)
            mask >>= 1;
        if (!mask)
            return true;
        else
            return false;
    }

    void disable_cat_cdp(int vote_cpu)
    {
        if (this->cap_cat_cdp)
        {
            uint64_t val = read_msr(vote_cpu, RDT_MSR_CAT_QOS_CFG);
            val &= ~1;
            write_msr(vote_cpu, RDT_MSR_CAT_QOS_CFG, val);
        }
    }
};

class RdtCAT : public RdtAllocation
{
public:
    uint32_t cat_nr_cosid;
    uint32_t cat_nr_way;
    uint32_t cat_way_contention;
    bool cap_cat_cdp;

    RdtCAT()
    {
        cat_nr_cosid = CPUID::cpuid_count(0x10, 1, 3) + 1;
        cat_nr_way = CPUID::cpuid_count(0x10, 1, 0) + 1;
        cat_way_contention = CPUID::cpuid_count(0x10, 1, 1);
        if (CPUID::cpuid_count(0x10, 1, 2) & (1 << RDT_MSR_CAT_CDP_SHIFT) != 0)
            cap_cat_cdp = true;
        else
            cap_cat_cdp = false;
    }

    static bool discover_cat_cap()
    {
        // platform QoS allocation capability
        if (CPUID::cpuid_count(0x7, 0x0, 1) & (1 << 15) == 0)
            return false;
        // llc allocation capability
        if (CPUID::cpuid_count(0x10, 0x0, 1) & (1 << 1) == 0)
            return false;
        return true;
    }

    void disable_cat_cdp(int vote_cpu)
    {
        if (cap_cat_cdp)
        {
            uint64_t val = read_msr(vote_cpu, RDT_MSR_CAT_QOS_CFG);
            val &= ~1;
            write_msr(vote_cpu, RDT_MSR_CAT_QOS_CFG, val);
        }
    }

    bool is_cbm_contiguous(int mask)
    {
        if (!mask)
            return false;
        while (mask & 1 == 0)
            mask >>= 1;
        while (mask & 1 != 0)
            mask >>= 1;
        if (!mask)
            return true;
        else
            return false;
    }

    //set what?
    void set_cat_cbm(int vote_cpu, uint64_t cosid, uint64_t cbm)
    {
        uint64_t reg = RDT_MSR_CAT_CBM_START + cosid;
        uint64_t val = cbm;
        write_msr(vote_cpu, reg, val);
    }

    uint64_t get_cat_cbm(int vote_cpu, uint64_t cosid)
    {
        uint64_t reg = RDT_MSR_CAT_CBM_START + cosid;
        return read_msr(vote_cpu, reg);
    }

    void reset_cat_cbm(int vote_cpu, uint64_t cosid)
    {
        uint64_t cbm = (1 << cat_nr_way) - 1;
        set_cat_cbm(vote_cpu, cosid, cbm);
    }

    void clean_up()
    {
        for (int socket = 0; socket < nr_sockets; socket++)
        {
            int vote_cpu = socket_vote_cpu[socket];
            for (int cosid = 0; cosid < cat_nr_cosid; cosid++)
                reset_cat_cbm(vote_cpu, cosid);
        }
    }
};

class RdtMBA : public RdtAllocation
{
public:
    uint32_t mba_nr_cosid;
    uint32_t mba_throttle_max;
    uint32_t mba_rate_max;
    uint32_t mba_rate_min;
    bool mba_linear;
    uint32_t mba_throttle_step;

    RdtMBA()
    {
        mba_nr_cosid = (CPUID::cpuid_count(0x10, 3, 3) & 0xffff) + 1;
        mba_throttle_max = (CPUID::cpuid_count(0x10, 3, 0) & 0xfff) + 1;
        mba_rate_max = 100;
        mba_rate_min = mba_rate_max - mba_throttle_max;
        if (CPUID::cpuid_count(0x10, 3, 2) & (1 << 2))
            mba_linear = true;
        else
            mba_linear = false;
        if (mba_linear)
            mba_throttle_step = mba_rate_max - mba_throttle_max;
    }

    static bool discover_mba_cap()
    {
        // platform QoS allocation capability
        if (CPUID::cpuid_count(0x7, 0x0, 1) & (1 << 15) == 0)
            return false;
        // memory bandwidth allocation capability
        if (CPUID::cpuid_count(0x10, 0x0, 1) & (1 << 3) == 0)
            return false;
        return true;
    }

    void set_mba_available(int vote_cpu, uint64_t cosid, uint64_t mbrate)
    {

        uint64_t reg = RDT_MSR_MBA_THROTTLE_START + cosid;
        uint64_t val;
        if (mba_linear)
        {
            val = (mba_rate_max - ((mbrate + mba_throttle_step / 2) / mba_throttle_step) * mba_throttle_step);
        }
        else
        {
            val = mba_rate_max - mbrate;
            if (val > 0)
                //?
                //val = 1 << (val - 1).bit_length();
                val = 1 << (uint64_t)floor(log2((val - 1))) + 1;
            else
                val = 0;
        }

        if (val > mba_throttle_max)
            val = mba_throttle_max;
        write_msr(vote_cpu, reg, val);
    }

    uint64_t get_mba_available(int vote_cpu, uint64_t cosid)
    {
        uint64_t reg = RDT_MSR_MBA_THROTTLE_START + cosid;
        return mba_rate_max - read_msr(vote_cpu, reg);
    }

    void reset_mba_available(int vote_cpu, uint64_t cosid)
    {
        set_mba_available(vote_cpu, cosid, mba_rate_max);
    }

    void clean_up()
    {
        for (int socket = 0; socket < nr_sockets; socket++)
        {
            int vote_cpu = socket_vote_cpu[socket];
            for (int cosid = 0; cosid < mba_nr_cosid; cosid++)
                reset_mba_available(vote_cpu, cosid);
        }
    }
};

RdtCAT rdtcat;
RdtMonitoring rdtmon;
RdtMBA rdtmba;

#endif
