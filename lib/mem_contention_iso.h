#ifndef MEM_CONTENTION_H
#define MEM_CONTENTION_H

#include <queue>
#include <deque>
#include "cstdio"
#include <iostream>
#include <fstream>
#include "bench.h"
#include "duty_cycle.h"
#include "utility.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include "init.h"
#include "rdt_manager.h"
#include "vm_status.h"

#define FIXLEN 15
#define SLIDING_WINDOW 6
#define NATIVE_ID "native"

#define NORMAL 0
#define ELASTIC 1
#define ELASTIC_L 2
#define ELASTIC_R 4

using namespace std;

typedef struct VMMemUsage
{
    double llc_occupancy;
    double mem_bandwidth;
    int nr_cpus;
} MemUsage;

typedef struct SocketMemUsage
{
    double llc_occupancy;
    double mem_bandwidth;
    double bc;
} SocketMemUsage;

typedef struct VMCtrl
{
    timestamp_t llc_relax_time;
    int mem_zone;
    set<int> cpuset;
    int llc_zone;
    FixedQueue<double, SLIDING_WINDOW> llc_usage;
    timestamp_t llc_thrtl_time;
    double llc_quota;
} VMCtrl;

typedef struct
{
    bool llc_on;
    double llc_thrtl_bc;
    double llc_thrtl_pct;
    double llc_thrtl_membw;
    double llc_thrtl_period;
    double llc_relax_guard;
    double llc_relax_period;

    bool mba_on;
    double mba_threshold;
    double mba_relax_guard;
    uint32_t mba_rate_min;

    timestamp_t interval;
} MemContentionConfig;

class MemoryContention
{
public:
    timestamp_t last_time;
    timestamp_t interval;
    // socket: vmid => VMMemUsage
    map<int, map<string, MemUsage>> vm_memusage;
    // socket: SocketMemUsage
    map<int, SocketMemUsage> socket_memusage;
    // socket: vmid => VMCtrl
    map<int, map<string, VMCtrl>> vm_ctrl;

    double llc_size;

    // config
    bool llc_on;
    double llc_thrtl_bc;
    double llc_thrtl_pct;
    double llc_thrtl_membw;
    double llc_thrtl_period;
    double llc_relax_guard;
    double llc_relax_period;

    bool mba_on;
    double mba_threshold;
    double mba_relax_guard;
    uint32_t mba_rate_min;

    map<int, FixedQueue<double, SLIDING_WINDOW>> bcs;
    map<int, FixedQueue<double, SLIDING_WINDOW>> membws;
    double factor[6] = {0.1, 0.1, 0.1, 0.2, 0.2, 0.3};

    void init_vm_ctrl(int socket, string vmid, set<int> cpuset)
    {
        VMCtrl vm_ctrl;

        vm_ctrl.cpuset = cpuset;
        vm_ctrl.llc_quota = -1;
        for (int i = 0; i < SLIDING_WINDOW; i++)
            vm_ctrl.llc_usage.push_back(0.0);
        vm_ctrl.llc_thrtl_time = 0;
        vm_ctrl.llc_relax_time = 0;
        vm_ctrl.llc_zone = NORMAL;
        vm_ctrl.mem_zone = NORMAL;

        this->vm_ctrl[socket][vmid] = vm_ctrl;
    }

    void reset_vm_ctrl(int socket, string vmid)
    {
        this->vm_ctrl[socket][vmid].llc_thrtl_time = 0;
        this->vm_ctrl[socket][vmid].llc_relax_time = 0;
        this->vm_ctrl[socket][vmid].llc_zone = NORMAL;
        this->vm_ctrl[socket][vmid].mem_zone = NORMAL;
    }

    double _calculate_llc_quota(int socket, string vmid, int nr_effective_cpu)
    {
        int nr_vm_cpu = this->vm_ctrl[socket][vmid].cpuset.size();
        return this->llc_size * nr_vm_cpu * 1.0 / nr_effective_cpu;
    }

    // Update llc quota of all vms, as well as native
    void update_all_llc_quota()
    {
        for (int socket = 0; socket < nr_sockets; socket++)
        {
            int nr_effective_cpu = 0; // the total number of cpu which is vcpu of vm
            for (auto iter = this->vm_ctrl[socket].begin(); iter != this->vm_ctrl[socket].end(); iter++)
                nr_effective_cpu += iter->second.cpuset.size();
            for (auto iter = this->vm_ctrl[socket].begin(); iter != this->vm_ctrl[socket].end(); iter++)
            {
                string vmid = iter->first;
                VMCtrl ctrl = iter->second;
                ctrl.llc_quota = _calculate_llc_quota(socket, vmid, nr_effective_cpu);
                vm_ctrl[socket][vmid] = ctrl;
            }
        }
    }

    vector<double> get_cpu_cachemiss()
    {
        system("sudo perf stat -e cache-misses -a -A  -- sleep 0.1 2>&1 | sed '1,3d' | sed '97,99d' > cache_misses.log");
        std::ifstream infile("cache_misses.log");
        double time;
        string cpu;
        double cache_misses;
        string event;
        string tmp;
        vector<double> record;

        // if (infile.is_open())
        // {
        //     std::string line;
        //     while (std::getline(infile, line))
        //     {
        //         // using printf() in all tests for consistency
        //         printf("%s", line.c_str());
        //     }
        //     infile.close();
        // }

        infile.imbue(std::locale(""));
        string line;
        int nr_hts;
        while (getline(infile, line))
        {
            stringstream ss(line);
            ss >> cpu >> cache_misses >> event;
            //infile.ignore(numeric_limits<streamsize>::max(), '\n');
            // cout << time << ", " << cpu  << ", " << splitlock << ", " << event << endl;
            record.push_back(cache_misses);
        }
        return record;
    }

    MemoryContention(MemContentionConfig config)
    {

        this->llc_size = get_llcinfo();

        // vm topo
        for (int id = 0; id < nr_vms; id++)
        {
            string vmid = vm_name[id];
            set<int> cpuset = vm_cpu[id];

            this->add_vm(vmid, cpuset);
        }
        this->update_all_llc_quota();

        //config
        this->llc_on = config.llc_on;
        this->llc_thrtl_bc = config.llc_thrtl_bc;
        this->llc_thrtl_pct = config.llc_thrtl_pct;
        this->llc_thrtl_membw = config.llc_thrtl_membw;
        this->llc_thrtl_period = config.llc_thrtl_period;
        this->llc_relax_guard = config.llc_relax_guard;
        this->llc_relax_period = config.llc_relax_period;

        this->mba_on = config.mba_on;
        this->mba_threshold = config.mba_threshold;
        this->mba_relax_guard = config.mba_relax_guard;
        this->mba_rate_min = min(max(config.mba_rate_min, rdtmba.mba_rate_min), rdtmba.mba_rate_max);

        this->interval = config.interval;


        for (int socket = 0; socket < nr_sockets; socket++)
        {
            set<int> common_cpuset = socket_topo[socket];

            // if (common_cpuset.size())
            // {
            //     this->init_vm_ctrl(socket, NATIVE_ID, common_cpuset);
            //     rdtmgr.init_vm_rdtinfo(socket, NATIVE_ID, common_cpuset);
            // }
            FixedQueue<double, SLIDING_WINDOW> bc;
            for (int i = 0; i < SLIDING_WINDOW; i++)
                bc.push_back(1.0);
            this->bcs[socket] = bc;

            FixedQueue<double, SLIDING_WINDOW> bw;
            for (int i = 0; i < SLIDING_WINDOW; i++)
                bc.push_back(0.0);
            this->membws[socket] = bw;
        }

        this->last_time = get_timestamp();
    }

    // # report state
    // this->state.transit("good", "memory_contention is working well")

    void add_vm(string vmid, set<int> cpuset)
    {
        for (int socket = 0; socket < nr_sockets; socket++)
        {
            set<int> common_cpuset;
            set_intersection(cpuset.begin(), cpuset.end(), socket_topo[socket].begin(), socket_topo[socket].end(), inserter(common_cpuset, common_cpuset.begin()));
            if (common_cpuset.size())
            {
                this->init_vm_ctrl(socket, vmid, common_cpuset);
                rdtmgr.init_vm_rdtinfo(socket, vmid, common_cpuset);
                rdtmgr.monitor_vm(socket, vmid);
            }
        }
    }

    void reset_vm(int socket, string vmid)
    {
        rdtmgr.cosid_free(socket, vmid);
        rdtmgr.reset_vm_rdtinfo(socket, vmid);
        this->reset_vm_ctrl(socket, vmid);
    }

    map<string, MemUsage> get_vm_memuage(int socket, vector<double> cpu_cachemiss)
    {
        map<string, MemUsage> vm_memusage;
        for (auto iter = this->vm_ctrl[socket].begin(); iter != this->vm_ctrl[socket].end(); iter++)
        {
            string vmid = iter->first;
            VMCtrl ctrl = iter->second;
            set<int> cpuset = ctrl.cpuset;
            double llc_occupancy = rdtmgr.vm_llc_occupancy(socket, vmid);
            double mem_bandwidth = 0;
            for (auto set_iter = cpuset.begin(); set_iter != cpuset.end(); set_iter++)
            {
                mem_bandwidth += cpu_cachemiss[*set_iter];
            }
            vm_memusage[vmid].llc_occupancy = llc_occupancy;
            vm_memusage[vmid].mem_bandwidth = mem_bandwidth;
            vm_memusage[vmid].mem_bandwidth = cpuset.size();
        }
        return vm_memusage;
    }

    SocketMemUsage get_socket_memusage(int socket, map<string, MemUsage> vm_memusage)
    {
        SocketMemUsage socket_memusage;
        double socket_llc_occupancy = 0.0;
        double socket_mem_bandwidth = 0.0;
        for (auto iter = vm_memusage.begin(); iter != vm_memusage.end(); iter++)
        {
            string vmid = iter->first;
            MemUsage usage = iter->second;
            socket_llc_occupancy += usage.llc_occupancy;
            socket_mem_bandwidth += usage.mem_bandwidth;
        }

        // NOTE this may happen when there are no native or vm(s),
        // or rmid has just been allocated and associated on this socket
        // if(socket_llc_occupancy == 0.0)
        //     return None

        double bc = 0.0;
        for (auto iter = this->vm_ctrl[socket].begin(); iter != this->vm_ctrl[socket].end(); iter++)
        {
            string vmid = iter->first;
            VMCtrl ctrl = iter->second;
            bc += sqrt(ctrl.llc_quota * vm_memusage[vmid].llc_occupancy / (this->llc_size * socket_llc_occupancy));
        }
        DEBUG_LOG(">>> bc %lf", bc);
        socket_memusage.llc_occupancy = socket_llc_occupancy;
        socket_memusage.mem_bandwidth = socket_mem_bandwidth;
        socket_memusage.bc = bc;

        return socket_memusage;
    }

    void push_usage(int socket, map<string, MemUsage> vm_memusage, SocketMemUsage socket_memusage)
    {
        DEBUG_LOG(">>> socket %d, bc %lf, mem_bwd %lf", socket, socket_memusage.bc, socket_memusage.mem_bandwidth)
        this->bcs[socket].push_back(socket_memusage.bc);
        this->membws[socket].push_back(socket_memusage.mem_bandwidth);

        for (auto iter = this->vm_ctrl[socket].begin(); iter != this->vm_ctrl[socket].end(); iter++)
        {
            string vmid = iter->first;
            VMCtrl ctrl = iter->second;
            ctrl.llc_usage.push_back(vm_memusage[vmid].llc_occupancy);

            this->vm_ctrl[socket][vmid] = ctrl;
        }
    }
    bool detect()
    {
        timestamp_t now = get_timestamp();
        if (now - this->last_time < this->interval)
            return false;
        this->last_time = now;
        vector<double> cpu_cachemiss = this->get_cpu_cachemiss();

        for (int socket = 0; socket < nr_sockets; socket++)
        {
            map<string, MemUsage> vm_memusage = this->get_vm_memuage(socket, cpu_cachemiss);
            SocketMemUsage socket_memusage = this->get_socket_memusage(socket, this->vm_memusage[socket]);
            this->vm_memusage[socket] = vm_memusage;
            this->socket_memusage[socket] = socket_memusage;

            this->push_usage(socket, vm_memusage, socket_memusage);
        }

        DEBUG_LOG(">>> detected...");
        return true;
    }

    timestamp_t _thrtl_duration(int socket, string vmid)
    {
        return this->last_time - this->vm_ctrl[socket][vmid].llc_thrtl_time;
    }

    //INFO CONTROL
    void throttle_llc(int socket)
    {
        string abuse_vm;
        double abuse_matric = 0;
        for (auto iter = this->vm_ctrl[socket].begin(); iter != this->vm_ctrl[socket].end(); iter++)
        {
            string vmid = iter->first;
            VMCtrl ctrl = iter->second;

            double llc_usage = 0;
            for (int slide = 0; slide < SLIDING_WINDOW; slide++)
            {
                llc_usage += ctrl.llc_usage[slide] * this->factor[slide];
            }
            if (llc_usage - ctrl.llc_quota > abuse_matric)
            {
                abuse_matric = llc_usage - ctrl.llc_quota;
                abuse_vm = vmid;
            }
        }

        //  bypass native
        if (abuse_vm == NATIVE_ID)
            return;

        DEBUG_LOG("abuse_vm %s", abuse_vm.c_str());

        // left vm isolation zone 
        string lvm, rvm;
        for (auto iter = this->vm_ctrl[socket].begin(); iter != this->vm_ctrl[socket].end(); iter++)
        {
            string vmid = iter->first;
            VMCtrl ctrl = iter->second;

            if (ctrl.llc_zone & ELASTIC_L)
                lvm = vmid;
            else if (ctrl.llc_zone & ELASTIC_R)
                rvm = vmid;
        }

        // whether in slot
        int zone = this->vm_ctrl[socket][abuse_vm].llc_zone;

        // find free slot
        if (zone == NORMAL)
        {
            if (lvm.empty())
                zone = ELASTIC | ELASTIC_L; //? ELASTIC 1
            else if (rvm.empty())
                zone = ELASTIC | ELASTIC_R;
        }

        DEBUG_LOG("zone %d", zone);

        // find victim slot
        if (zone == NORMAL)
        {
            string victim;
            // victim is the vmid which has longer thrtl_duration
            if (_thrtl_duration(socket, lvm) > _thrtl_duration(socket, rvm))
                victim = lvm;
            else
                victim = rvm;

            if (_thrtl_duration(socket, victim) >= this->llc_thrtl_period)
            {
                zone = this->vm_ctrl[socket][victim].llc_zone;
                DEBUG_LOG(">>> isolate_vm_llc: socket %d, vitctim %s ", socket, victim.c_str() );
                rdtmgr.isolate_vm_llc(socket, victim, rdtcat.cat_nr_way);

                this->vm_ctrl[socket][victim].llc_zone = NORMAL;
                this->vm_ctrl[socket][victim].llc_relax_time = this->last_time;
                //this->isodebug(socket, victim, "reset_llc", header="ctrl")
                this->reclaim_cos(socket, victim);
            }
        }

        // found no slot
        if (zone == NORMAL)
            return;

        int socket_nr_cpus = socket_topo[socket].size();
        int abuse_vm_nr_cpus = this->vm_ctrl[socket][abuse_vm].cpuset.size();
        int cat_way = (rdtcat.cat_nr_way * abuse_vm_nr_cpus + socket_nr_cpus - 1) / socket_nr_cpus + 1;
        int cat_cbm = (1 << cat_way) - 1;

        if (zone & ELASTIC_L)
            cat_cbm <<= rdtcat.cat_nr_way - cat_way;

        DEBUG_LOG("cat_way %d, cat_cbm %x ", cat_way, cat_cbm);

        rdtmgr.isolate_vm_llc(socket, abuse_vm, cat_way, cat_cbm); //?

        this->vm_ctrl[socket][abuse_vm].llc_zone = zone;
        this->vm_ctrl[socket][abuse_vm].llc_thrtl_time = this->last_time;
        this->vm_ctrl[socket][abuse_vm].llc_relax_time = this->last_time;

        // this->isodebug(socket, abuse_vm, "throttle_llc", rdtmgr.vm_rdtinfo[socket][abuse_vm]["cosid"],
        //       format(cat_cbm, '0{}b'.format(rdtcat.cat_nr_way)), header="ctrl")
    }

    void throttle_membw(int socket, map<string, MemUsage> vm_memusage, double socket_mem_bandwidth)
    {
        double socket_avg_bw = socket_mem_bandwidth / socket_topo[socket].size();

        for (auto iter = this->vm_ctrl[socket].begin(); iter != this->vm_ctrl[socket].end(); iter++)
        {
            string vmid = iter->first;
            VMCtrl ctrl = iter->second;

            if (vm_memusage[vmid].mem_bandwidth < socket_avg_bw * vm_memusage[vmid].nr_cpus)
                continue;

            uint32_t mbrate = rdtmgr.vm_rdtinfo[socket][vmid].mba_rate;
            if (mbrate <= this->mba_rate_min)
                continue;
            mbrate = max(mbrate / 2, this->mba_rate_min);
            if (!rdtmgr.isolate_vm_mem(socket, vmid, mbrate))
                continue;
            ctrl.mem_zone = ELASTIC;

            this->vm_ctrl[socket][vmid] = ctrl;
        }
    }

    void relax_membw(int socket)
    {
        for (auto iter = this->vm_ctrl[socket].begin(); iter != this->vm_ctrl[socket].end(); iter++)
        {
            string vmid = iter->first;
            VMCtrl ctrl = iter->second;

            if (ctrl.mem_zone == NORMAL)
                continue;
            uint32_t mbrate = rdtmgr.vm_rdtinfo[socket][vmid].mba_rate;
            if (rdtmba.mba_linear)
                mbrate += rdtmba.mba_throttle_step;
            else
                mbrate = (rdtmba.mba_rate_max + mbrate) / 2;
            mbrate = min(mbrate, rdtmba.mba_rate_max);
            if (!rdtmgr.isolate_vm_mem(socket, vmid, mbrate))
                continue;
            if (mbrate >= rdtmba.mba_rate_max)
                ctrl.mem_zone = NORMAL;

            this->vm_ctrl[socket][vmid] = ctrl;
        }
    }

    void relax_llc(int socket)
    {
        for (auto iter = this->vm_ctrl[socket].begin(); iter != this->vm_ctrl[socket].end(); iter++)
        {
            string vmid = iter->first;
            VMCtrl ctrl = iter->second;

            if (ctrl.llc_zone == NORMAL ||
                this->last_time < ctrl.llc_relax_time + this->llc_relax_period)
                continue;
            uint32_t cat_way = min(rdtmgr.vm_rdtinfo[socket][vmid].cat_way + 1, rdtcat.cat_nr_way);
            uint32_t cat_cbm = (1 << cat_way) - 1;
            if (ctrl.llc_zone & ELASTIC_L)
                cat_cbm <<= rdtcat.cat_nr_way - cat_way;
            if (!rdtmgr.isolate_vm_llc(socket, vmid, cat_way, cat_cbm))
                continue;
            if (cat_way == rdtcat.cat_nr_way)
                this->vm_ctrl[socket][vmid].llc_zone = NORMAL;
            this->vm_ctrl[socket][vmid].llc_relax_time = this->last_time;
        }
    }

    void _do_reclaim_cos(int socket, string vmid)
    {
        if (rdtmgr.vm_rdtinfo[socket][vmid].cosid != COSID0 &&
            this->vm_ctrl[socket][vmid].llc_zone == NORMAL &&
            this->vm_ctrl[socket][vmid].mem_zone == NORMAL)
        {
            this->reset_vm(socket, vmid);
        }
    }
    void reclaim_cos(int socket)
    {
        for (auto iter = this->vm_ctrl[socket].begin(); iter != this->vm_ctrl[socket].end(); iter++)
        {
            string vmid = iter->first;
            VMCtrl ctrl = iter->second;
            _do_reclaim_cos(socket, vmid);
        }
    }

    void reclaim_cos(int socket, string vmid)
    {
        _do_reclaim_cos(socket, vmid);
    }

    void control()
    {
        for (int socket = 0; socket < nr_sockets; socket++)
        {
            // no vm in system
            if (this->vm_ctrl[socket].size() <= 1)
                continue;
            // if this->memusage[socket] is None:
            //     continue

            map<string, MemUsage> t_vm_memusage = this->vm_memusage[socket];
            SocketMemUsage t_socket_memusage = this->socket_memusage[socket];

            double bc_wavg = 0.0;
            double membw_wavg = 0.0;
            for (int slide = 0; slide < SLIDING_WINDOW; slide++)
            {
                bc_wavg += this->bcs[socket][slide] * this->factor[slide];
                membw_wavg += this->membws[socket][slide] * this->factor[slide];
            }
            DEBUG_LOG(">>> socket %d, bc_wavg %lf, llc_thrtl_membw %lf", socket, bc_wavg, membw_wavg);
            // weighted avg
            if (this->llc_on && bc_wavg <= this->llc_thrtl_bc && membw_wavg >= this->llc_thrtl_membw && t_socket_memusage.llc_occupancy >= this->llc_size * this->llc_thrtl_pct)
            {
                DEBUG_LOG(">>> throttle_llc");
                this->throttle_llc(socket);
                continue;
            }

            // immediate val
            if (this->mba_on && t_socket_memusage.mem_bandwidth >= this->mba_threshold)
            {
                this->throttle_membw(socket, t_vm_memusage, t_socket_memusage.mem_bandwidth);
                continue;
            }

            // relax
            if (this->mba_on && t_socket_memusage.mem_bandwidth <= this->mba_threshold - this->mba_relax_guard)
                this->relax_membw(socket);
            if (bc_wavg >= this->llc_thrtl_bc + this->llc_relax_guard)
                this->relax_llc(socket);
            this->reclaim_cos(socket);
        }
    }
};

#endif
