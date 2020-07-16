#ifndef INIT_H
#define INIT_H

#include "duty_cycle.h"
#include "utility.h"
#include "cpuid.h"
#include "intel_rdt.h"
#include "mem_contention_iso.h"
#include "vm_status.h"
#include "rdt_manager.h"

void init()
{
    init_topology();
    nr_sockets = socket_topo.size();
    nr_cpus = topo_cpu.size();

    get_vm_topo();
    nr_vms = vm_cpu.size();

    uint32_t extension = CPUID::cpuid(0x6, 0) & 0x20;
    if (extension)
    {
        duty_bit = 0x0;
        max_duty = 15;
    }
    else
    {
        duty_bit = 0x1;
        max_duty = 7;
    }

    rdtmon = RdtMonitoring();
    rdtcat = RdtCAT();
    rdtmba = RdtMBA();
    rdtmgr = RdtManager();
}
#endif
