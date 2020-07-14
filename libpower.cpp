#include <bits/stdc++.h>
#include "bench.cpp"
#include "utility.h" //from utility import (core_vote_cpu, get_cpuinfo, nr_cpus, nr_sockets, readmsr,socket_vote_cpu, topo_cpu)
using namespace std;
using ll = long long;

class PowerCpuUtilUpdater{
    private:
        double period;
        double update_time;
        Libpower libpower;
    
    public:
        PowerCpuUtilUpdater(double period, const Libpower& libpower): period(period),libpower(libpower){
            update_time = get_timestamp();
        }

        PowerCpuUtilUpdater operator()(){
            double now = get_timestamp();
            if (now - update_time >= period){
                libpower.update_cpu_util_power();
                update_time = now;
            }
        }
};

class PowerCapChecker{
    private:
        double period = 0;
        double update_time = 0;
        Libpower libpower;
        bool sane = true;

    public:
        bool power_capping_check(){
            double unit = 1000;
            double tsc_mhz = libpower.tsc_mhz();
            for (int cpu = 0; cpu < nr_cpus.size(); cpu++){
                //这里暂时没写完
                string path = "/sys/devices/system/cpu/cpu" + to_string(cpu) + "/cpufreq";
                //读写文件
            }
            return true;
        }

        PowerCapChecker(const Libpower& libpower): libpower(libpower){}

        void set_period(double period){
            period = period;
        }

        PowerCapChecker operator()(){
            double now = get_timestamp();
            if (period > 0 && now - update_time >= period){
                sane = power_capping_check();
                update_time = now;
            }
            if (sane == false){
                //from exception import PowerCapException
                throw "Powercapping is configured";
            }
        }
};
class Libpower{
    private:
        double tsc_mhz = get_tsc_mhz();
        callbacks;              //不明类型

        const int MSR_PKG_ENERGY_STATUS = 0x611;
        const int MSR_DRAM_ENERGY_STATUS = 0x619;

        vector<double> callbacks = vector<double>(nr_sockets, 0);
        vector<int> tcc = vector<int>(nr_sockets, 0);
        vector<double> energy_unit = vector<double>(nr_sockets, 0);
        vector<double> dram_energy_unit = vector<double>(nr_sockets, 0);
        vector<double> power_unit = vector<double>(nr_sockets, 0);
        vector<double> TDP = vector<double>(nr_sockets, 0);
        vector<double> socket_power, dram_power, cpu_util;

        vector<vector<ll>> last_energy = vector<vector<ll>>(2, vector<ll>(nr_sockets, 0));
        map<int, vector<int>> last_cpu_stat;
        double energy_update_time;

    public:
        Libpower(){
            last_energy[0] = get_energy(MSR_PKG_ENERGY_STATUS);
            last_energy[1] = get_energy(MSR_DRAM_ENERGY_STATUS);
            last_cpu_stat = get_cpu_stat();

            energy_update_time = get_timestamp();

            for (auto socket : socket_vote_cpu){
                tcc[socket] = tcc_activation_temp(socket);
                get_energy_unit(socket, energy_unit[socket], power_unit[socket]);
                dram_energy_unit[socket] = get_dram_energy_unit(energy_unit[socket]);
                TDP[socket] = get_socket_tdp(socket);
            }
        }

        double get_tsc_mhz(){
            int MSR_PLATFORM_INFO = 0x000000ce;
            int base_ratio = (readmsr(0, MSR_PLATFORM_INFO) >> 8) & 0xFF;
            double bclk = 100.0;
            return base_ratio * bclk;
        }

        //返回map
        // def get_cpu_stat(self):
        //     cpu_stat = {}
        //     with open('/proc/stat') as f:
        //         for line in f.readlines()[1:nr_cpus+1]:
        //             fields = line.split()
        //             cpu = int(fields[0][3:])
        //             idle = int(fields[4])
        //             total = sum(map(int, fields[1:]))
        //             cpu_stat[cpu] = (idle, total)
        //     return cpu_stat

        void update_cpu_util_power(){
            vector<vector<double>> tmp = get_power();
            socket_power = tmp[0];
            dram_power = tmp[1];
            cpu_util = get_cpu_util();
            report();
        }

        vector<vector<double>> get_power(){
            vector<ll> old_socket_energy = last_energy[0];
            vector<ll> old_dram_energy = last_energy[1];
            vector<ll> socket_energy = get_energy(MSR_PKG_ENERGY_STATUS);
            vector<ll> dram_energy = get_energy(MSR_DRAM_ENERGY_STATUS);
            last_energy[0] = socket_energy;
            last_energy[1] = dram_energy;
            double now = get_timestamp();
            double interval = now - energy_update_time;
            energy_update_time = now;
            vector<double> socket_power(nr_sockets, 0);
            vector<double> dram_power(nr_sockets, 0);
            for (int socket = 0; socket < nr_sockets; socket++;){
                double energy_unit_val = energy_unit[socket];
                ll socket_delta_energy = socket_energy[socket] - old_socket_energy[socket];
                if (socket_delta_energy < 0){
                    socket_delta_energy &= 0xFFFFFFFF;
                }
                socket_power[socket] = double(socket_delta_energy) * energy_unit_val / interval;

                double dram_energy_unit_val = dram_energy_unit[socket];
                ll dram_delta_energy = dram_energy[socket] - old_dram_energy[socket];
                if (dram_delta_energy < 0){
                    dram_delta_energy &= 0xFFFFFFFF;
                }
                dram_power[socket] = double(dram_delta_energy) * energy_unit_val / interval;
            }
            return vector<vector<double>>({socket_power, dram_power});
        }

        vector<double> get_energy(int _register){
            vector<double> energy(nr_sockets, 0);
            for (int socket = 0; socket < nr_sockets; socket++;){
                int cpu = socket_vote_cpu[socket];
                energy[socket] = readmsr(cpu, _register) & 0xFFFFFFFF;
            }
            return energy;
        }

        vector<double> get_cpu_util(){
            map<int, vector<int>> cpu_stat = get_cpu_stat();
            map<int, vector<int>> old_cpu_stat = last_cpu_stat;
            vector<double> cpu_util(nr_cpus, 0);
            for (int cpu = 0; cpu < nr_cpus; cpu++){
                int old_idle = old_cpu_stat[cpu][0];
                int old_total = old_cpu_stat[cpu][1];
                int idle, total = cpu_stat[cpu][0];
                int delta_total = total - old_total;
                int delta_idle = idle - old_idle;
                if (delta_total == 0){
                    cpu_util[cpu] = 0;
                }
                else{
                    cpu_util[cpu] = 100.0 - 100.0 * delta_idle / delta_total;
                }
            }
            last_cpu_stat = cpu_stat;
            return cpu_util;
        }

        // def register_callback(self, cb, *args):
        //     self.callbacks.append((cb, args))

        // def report(self):
        //     for cb, args in self.callbacks:
        //         cb({'cpu_util': self.cpu_util, 'socket_power': self.socket_power, 'dram_power': self.dram_power}, *args)

        int tcc_activation_temp(int socket){
            int base_cpu = socket_vote_cpu[socket];
            int MSR_IA32_TEMPERATURE_TARGET = 0x1a2;
            int msr = readmsr(base_cpu, MSR_IA32_TEMPERATURE_TARGET);
            return (msr >> 16) & 0xFF;
        }

        int read_temperature(int cpu, int tcc){
            int MSR_IA32_THERM_STATUS = 0x19c;
            int msr = readmsr(cpu, MSR_IA32_THERM_STATUS);
            return tcc - ((msr >> 16) & 0x7F);
        }

        void get_energy_unit(int socket, double &a, double &b){
            int base_cpu = socket_vote_cpu[socket];
            int MSR_RAPL_POWER_UNIT = 0x606;
            int msr = readmsr(base_cpu, MSR_RAPL_POWER_UNIT);//rdmsr不明
            a = 1.0 / (1 << (msr >> 8 & 0x1F));
            b = 1.0 / (1 << (msr & 0xF));
        }

        double get_dram_energy_unit(double energy_unit){
            double dram_energy_unit = energy_unit;

            int INTEL_FAM6_HASWELL_X = 0x3F;
            int INTEL_FAM6_BROADWELL_X = 0x4F;
            int INTEL_FAM6_BROADWELL_XEON_D = 0x56;
            int INTEL_FAM6_XEON_PHI_KNL = 0x57;
            int INTEL_FAM6_XEON_PHI_KNM = 0x85;
            map<int, bool> mp;
            mp[INTEL_FAM6_HASWELL_X] = 1;
            mp[INTEL_FAM6_BROADWELL_X] = 1;
            mp[INTEL_FAM6_BROADWELL_XEON_D] = 1;
            mp[INTEL_FAM6_XEON_PHI_KNL] = 1;
            mp[INTEL_FAM6_XEON_PHI_KNM] = 1;

            int model = get_cpuinfo();//utility.h

            if (mp[model] == 1){
                dram_energy_unit = 15.3 / 1000000;
            }
            return dram_energy_unit;
        }

        double get_socket_tdp(int socket){
            int base_cpu = socket_vote_cpu[socket];
            int MSR_PKG_POWER_INFO = 0x614;
            int msr = readmsr(base_cpu, MSR_PKG_POWER_INFO);
            return (msr & 0x7FFF) * power_unit[socket];
        }

        // vector<double> get_temperature(){
        //     vector<double> temperatures(nr_cpus, 0);
        //     vote_cpus = core_vote_cpu.values()//utility.h
        //     for (int cpu = 0; cpu < nr_cpus.size(); cpu++){
        //         int socket, core = topo_cpu[cpu]
        //         if cpu in vote_cpus:
        //             temperatures[cpu] = self.read_temperature(cpu, self.tcc[socket])
        //         else:
        //             temperatures[cpu] = temperatures[core_vote_cpu[(socket, core)]]
        //     }
        //     return temperatures
        // }
};

// libpower = Libpower()
// powercap_checker = PowerCapChecker(libpower)
// power_cpu_util_updater = PowerCpuUtilUpdater(1, libpower)

// if __name__ == '__main__':
//     libpower = Libpower()

//     def test_cb(power):
//         temps = libpower.get_temperature()
//         cpu_util = power["cpu_util"]
//         for cpu, _ in enumerate(zip(cpu_util, temps)):
//             x, y = _
//             print '{}\t{:.2f}\t{:.2f}'.format(cpu, x, y)

//     libpower.register_callback(test_cb)
//     while True:
//         time.sleep(3)
//         libpower.update_cpu_util_power()