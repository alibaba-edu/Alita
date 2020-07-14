#ifndef DUTY_CYCLE_H
#define DUTY_CYCLE_H

#include "utility.h"

// IA32_CLOCK_MODULATION
#define MSR 0x19a
#define enable_bit 4
uint32_t duty_bit;
uint32_t max_duty;

int read_duty_cycle(int cpu)
{
    unsigned long long val = read_msr(cpu, MSR);
    uint64_t enable = (val >> enable_bit);
    uint64_t target = (val & (~(enable << enable_bit)));
    //return enable, target
    return target;
}

void write_duty_cycle(int cpu, int target)
{
    int enable = 0;
    if (target > 0)
        enable = 1;
    else
        enable = 0;
    uint64_t val = (enable << enable_bit) | (target << duty_bit);
    write_msr(cpu, MSR, val);
}

int duty_account(int duty)
{
    if (duty == 0)
        return max_duty + 1;
    else
        return duty;
}

void init_cpu_duty_cycle()
{
    for (int cpu = 0; cpu < nr_cpus; cpu++)
    {
        write_duty_cycle(cpu, 0);
    }
}

class DutyCycleScheme
{
public:
    int min_duty;
    // cpu => duty
    map<int, int> duty;
    map<int, double> duty_map;
    vector<double> duty_rate = vector<double>(nr_cpus, 0);

    void set_min_duty(int min_duty)
    {
        this->min_duty = min_duty;
    }

    DutyCycleScheme()
    {
        this->min_duty = 1;
        for (auto it = core_vote_cpu.begin(); it != core_vote_cpu.end(); it++)
        {
            duty[it->second] = 0;
        }
        for (int duty = 0; duty < (int)max_duty + 1; duty++)
        {
            duty_map[duty] = duty_account(duty) * 100.0 / (max_duty + 1);
        }
    };

    void update_duty(int cpu, int value, bool relative)
    {
        //We simply ignore those higher HT

        if (!this->duty.count(cpu))
            return;

        if (relative)
            value += duty_account(this->duty[cpu]);
        else
            value = duty_account(value);

        if (value < this->min_duty)
            value = this->min_duty;
        if (value > (int)max_duty)
            value = 0;

        this->duty[cpu] = value;

        write_duty_cycle(cpu, value);
        this->duty_rate[cpu] = this->duty_map[value];
        DEBUG_LOG("update cpu %d, value %d", cpu, value);
        // cout << this->duty_rate[cpu] << endl;
    }

    //return duty rate for each cpu
    vector<double> get_freq_rate()
    {
        return this->duty_rate;
    }

    bool get_thrtl_flag()
    {
        for (auto it = socket_vote_core.begin(); it != socket_vote_core.end(); it++)
        {
            for (auto itset = it->second.begin(); itset != it->second.end(); ++itset)
            {
                if (this->duty[*itset] > 0)
                    return true;
            }
        }
        return false;
    }
};

DutyCycleScheme split_lock_scheme;

#endif
