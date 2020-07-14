#ifndef SPLIT_LOCK_ISO_H
#define SPLIT_LOCK_ISO_H

#include <bits/stdc++.h>
#include <queue>
#include <deque>
#include <iostream>
#include <fstream>
#include "bench.h"
#include "duty_cycle.h"
#include "utility.h"
#include <algorithm>
#include <numeric>
#include "init.h"
#define FIXLEN 15

using namespace std;

class SplitLockIso
{
public:
    int DIRTY_THRESHOLD = 125;
    DutyCycleScheme split_lock_scheme;
    string split_lock_dir = "/var/log/fisher/split_lock_10HZ";
    //logger = get_logger('split_lock')
    //state = State.get_state_obj("isolation.split_lock", "good")
    //sls_output_handler = SlsOutputHandler.get_handler("split_lock")
    int threshold_update_period = FIXLEN;

    vector<int> split_locks = vector<int>(nr_cpus, 0);
    vector<int> last_rate = vector<int>(nr_cpus, 0);

    // // cum_rate: A fixed length queue
    // // element : cpu_cnt_per_sec, a rate vector for all cpus in a sec
    FixedQueue<vector<double>, FIXLEN> cum_rate;
    // system-wide split lock count in recent 15 second
    FixedQueue<double, FIXLEN> last_rates;

    int dirty_cores = nr_cpus / 2;
    double period = 0.5;
    //split_lock_reader = FisherReader(split_lock_dir, 'SQ_MISC.SPLIT_LOCK')
    int min_start = 1;

    double last_loop_time = 0.0;
    int last_dump_time = 0;

    int quota;
    bool switchon;
    int interval;

    // self-preserved variables
    double HZ;
    int max_start;
    int start;

    vector<int> triggered = vector<int>(nr_cpus, 0);
    vector<double> cum_time = vector<double>(nr_cpus, 0.0);
    timestamp_t burst_update_time;

    //accumulated split lock for each cpu in second
    vector<double> cpu_cnt_per_sec = vector<double>(nr_cpus, 0.0);
    vector<double> freq_per_sec = vector<double>(nr_cpus, 0.0);
    uint64_t freq_samples = 0;
    int freq_samples_min = 0;
    int last_sec_time = 0;
    uint64_t cnt_per_sec = 0;
    bool thrtl_flag = false;

    int threshold;
    int burst;

    SplitLockIso(bool switchon, double interval, int quota)
    {
        this->switchon = switchon;
        this->interval = interval;
        this->quota = quota;
        this->burst_update_time = get_timestamp();

        this->HZ = int(1.0 / this->interval);
        this->max_start = max_duty / 2;
        this->start = this->min_start;

        this->threshold = int(0.8 * this->quota / (this->dirty_cores * this->HZ));
    }

    int calc_dirty_cores(vector<int> &cpu_rate)
    {
        int count = 0;
        for (int i = 0; i < nr_cpus; i++)
        {
            if (cpu_rate[i] > DIRTY_THRESHOLD * threshold_update_period)
                count += 1;
        }
        return count;
    }

    vector<int> get_split_locks()
    {
        system("sudo perf stat -e r10f4 -a -A --per-core -- sleep 0.1 2>&1 | sed '1,3d' | sed '48,51d' > splitlock.log");
        std::ifstream infile("splitlock.log");
        double time;
        string cpu;
        int splitlock;
        string event;
        string tmp;
        vector<int> record;

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

        // skip head
        infile.imbue(std::locale(""));
        string line;
        int nr_hts;
        while (getline(infile, line))
        {
            stringstream ss(line);
            ss >> cpu >> nr_hts >> splitlock >> event;
            //infile.ignore(numeric_limits<streamsize>::max(), '\n');
            // cout << time << ", " << cpu  << ", " << splitlock << ", " << event << endl;
            record.push_back(splitlock);
        }
        return record;
    }

    int analysis(FixedQueue<double, FIXLEN> rates, int quota)
    {
        sort(rates.begin(), rates.end(), greater<double>());
        // for (int i = 0; i < rates.size(); i++)
        //     cout << rates[i] << " ";
        // cout << endl;

        int size = rates.size() / 3;
        int accum = 0;
        for (int i = 0; i < size; i++)
        {
            accum += rates[i];
        }

        double avg = accum / size;
        if (avg > quota * 1.05)
            return 1;
        if (avg < quota * 0.9)
            return -1;

        return 0;
    }

    bool detect()
    {
        timestamp_t now = get_timestamp();
        if (now - last_loop_time < interval)
            return false;
        last_loop_time = now;

        //splitlock for each cpu, (nr_cpus, split_lock)
        vector<int> split_locks = get_split_locks();
        thrtl_flag |= split_lock_scheme.get_thrtl_flag();

        //accumulate a system wide split lock for each second
        cnt_per_sec += accumulate(split_locks.begin(), split_locks.end(), 0);
        //accumulate split lock for each cpu in second
        for (int cpu = 0; cpu < nr_cpus; cpu++)
        {
            cpu_cnt_per_sec[cpu] += split_locks[cpu];
        }

        //accumulate frequence rate for each cpu in second
        vector<double> freq_rate = split_lock_scheme.get_freq_rate();
        for (int cpu = 0; cpu < nr_cpus; cpu++)
        {
            freq_per_sec[cpu] += freq_rate[cpu];
        }
        freq_samples += 1;

        if (now - last_sec_time >= 1)
        {
            //self.update_stat_info(self.cnt_per_sec, int(now))
            last_rates.push_back(cnt_per_sec);
            cnt_per_sec = 0;
            cum_rate.push_back(cpu_cnt_per_sec);
            fill(cpu_cnt_per_sec.begin(), cpu_cnt_per_sec.end(), 0);
            fill(freq_per_sec.begin(), freq_per_sec.end(), 0);
            freq_samples = 0;
            last_sec_time = now;
        }

        // if now - self.last_dump_time >= 60:
        //     self.dump_sls(int(now))
        //     self.clean_stat_info()
        //     self.last_dump_time = now

        for (auto it = core_vote_cpu.begin(); it != core_vote_cpu.end(); it++)
        {
            //std::cout << " (" << it->first[0] << "," << it->first[1] << "): " << it->second << std::endl;
            int cpu = it->second;
            double rate = split_locks[cpu];

            if (last_rate[cpu] > rate)
                rate = last_rate[cpu] * 0.75 + rate * 0.25;
            last_rate[cpu] = rate;
        }
        DEBUG_LOG(">>> detected...");
        return true;
    }

    void control()
    {
        timestamp_t now = get_timestamp();

        if (cum_rate.size() == threshold_update_period)
        {
            //cpu_rate: the sum rate across all record time slice for each cpu
            vector<int> cpu_rate = vector<int>(nr_cpus, 0);
            for (int cpu = 0; cpu < nr_cpus / 2; cpu++)
            {
                for (int slice = 0; slice < FIXLEN; slice++)
                {
                    cpu_rate[cpu] += cum_rate[slice][cpu];
                }
            }
            // pad cum_rate to nr_cpu
            //cum_rate = cum_rate.insert(0, nr_cpus/2);

            int dirty_cores = calc_dirty_cores(cpu_rate);
            //cum_rate.clear();

            if (dirty_cores)
                threshold = int(0.8 * quota / (dirty_cores * HZ));
            else
                threshold = quota;
            burst = threshold * 3;
        }

        for (auto iter = core_vote_cpu.begin(); iter != core_vote_cpu.end(); iter++)
        {
            vector<int> socket_core = iter->first;
            int cpu = iter->second;

            double rate = last_rate[cpu];

            if (rate > burst)
            {
                cum_time[cpu] = 0;
                triggered[cpu] = 1;
            }
            else
            {
                cum_time[cpu] += now - burst_update_time;
                if (triggered[cpu] && (cum_time[cpu] > period))
                    triggered[cpu] = 0;
            }

            // // Only throttle cores if their VM is on blacklist
            // if cpu not in core_blacklist:
            //     continue

            if (rate <= threshold && !triggered[cpu])
            {
                //try to relax duty cycle
                if (rate > 0.8 * threshold)
                    continue;
                split_lock_scheme.update_duty(cpu, 1, true);
            }
            else
            {
                //try to throttle duty cycle, relative = False
                split_lock_scheme.update_duty(cpu, start, false);
            }
        }
        // try to relax or throttle start
        if (last_rates.size() == threshold_update_period)
        {
            int res = analysis(last_rates, quota);
            //fill(last_rates.begin(), last_rates.end(), 0);
            //last_rates.clear();

            // throttle
            if (res == 1 && start > min_start)
            {
                start -= 1;
            }

            // relax
            if (res == -1 && start < max_start)
                start += 1;
        }
        burst_update_time = now;
    }
};

#endif
