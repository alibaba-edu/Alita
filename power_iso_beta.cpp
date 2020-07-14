#include "duty_cycle.h"
#include "vm_status.h"
#include <bits/stdc++.h>
#include <unistd.h>
using namespace std;
// # topo_cpu => { cpu : {socket, core} }
// # socket_topo => { socket: set([cpu]) }
// # vm_socket_cpu => { vm : socket : set[cpu] }
// # socket_vm => { socket : set[vm] }

// map<int, vector<int>> topo_cpu;
// map<int, set<int>> socket_topo;
// map<int, map<int, set<int>>> vm_socket_cpu;
// map<int, set<int>> socket_vm;

// vector<string> vm_name; // record vm id[0~n-1] with names(string)
class PowerStrategy
{
public:
    const double quota = 150;
    const double guard = 10;
    const double cpu_util_threshold = 50; // ignore %
    int interval = 12;
    string file_des;
    //read from turbostat/turbo once time
    map<int, double> socket_power; //nr_sockets
    map<int, double> cpu_util;     //nr_cpus
    map<int, double> cpu_temp;     //nr_cpus

    //calculate
    map<int, double> socket_avg_temp;          //nr_sockets,calculate
    map<int, map<int, double>> vm_socket_temp; //{ vm : socket : temperature }
    set<vector<int>> noise_vm_on_socket;       // set({vm, socket}), cal in noise()
    map<int, set<int>> safe_cpu_need_increase;

    map<int, set<int>> socket_noise_cpus; //target cpus
    // map<int, set<int>> socket_noise_cpus_history;

    PowerStrategy() {}

    void update_from_file(string file_des)
    {
        file_des = file_des;

        get_socket_power(file_des);
        get_cpu_util(file_des);
        get_cpu_temp(file_des);
        calculate_socket_avg_temp();
        calculate_vm_socket_temp();
        safe();
        noise();
    }

    bool half_or_more_cpu_util(int vm, int socket)
    {
        int total_num = (int)vm_socket_cpu[vm][socket].size();
        int over_cpu_util_threshold_num = 0;
        for (auto cpu : vm_socket_cpu[vm][socket])
        {
            if (cpu_util[cpu] > cpu_util_threshold)
                over_cpu_util_threshold_num++;
        }
        return 2 * over_cpu_util_threshold_num >= total_num;
    }

    void safe()
    {
        for (int socket = 0; socket < nr_sockets; socket++)
        {
            if (socket_power[socket] > quota - guard)
                continue;
            //this socket is safe!
            for (auto cpu : socket_topo[socket])
            {
                int cur_duty = read_duty_cycle(cpu);
                if (cur_duty == 0)
                    continue;
                safe_cpu_need_increase[socket].insert(cpu);
            }
        }
    }

    void noise()
    {
        for (int socket = 0; socket < nr_sockets; socket++)
        {
            if (socket_power[socket] < quota)
                continue;
            //this socket is over quota !
            for (auto vm : socket_vm[socket])
            {
                if (vm_socket_temp[vm][socket] < socket_avg_temp[socket])
                    continue;
                if (half_or_more_cpu_util(vm, socket) == true)
                    //need to be clear each time
                    noise_vm_on_socket.insert({vm, socket});
            }
        }

        for (auto &v_vm_socket : noise_vm_on_socket)
        {
            for (auto cpu : vm_socket_cpu[v_vm_socket[0]][v_vm_socket[1]])
                //need to be clear each time
                socket_noise_cpus[v_vm_socket[1]].insert(cpu);
        }
    }

    void get_socket_power(string read_des)
    {
        int socket = 0;
        ifstream fin(read_des);
        string s;
        vector<string> a;
        while (getline(fin, s))
        {
            a = split(s);
            if (a.size() < 13 || isdigit(a[0][0]) == false)
                continue;
            socket_power[socket++] = stof(a[12]);
        }
    }

    void get_cpu_util(string read_des)
    {
        ifstream fin(read_des);
        string s;
        vector<string> a;
        while (getline(fin, s))
        {
            a = split(s);
            if (isdigit(a[0][0]) == false)
                continue;
            int cpu = stoi(a[0]);
            cpu_util[cpu] = stof(a[2]);
        }
    }

    void get_cpu_temp(string read_des)
    {
        double preline_cpu_temp = 0;
        ifstream fin(read_des);
        string s;
        vector<string> a;
        while (getline(fin, s))
        {
            a = split(s);
            if (isdigit(a[0][0]) == false)
                continue;
            int cpu = stoi(a[0]);
            if (a.size() < 8)
            {
                cpu_temp[cpu] = preline_cpu_temp;
            }
            else
            {
                cpu_temp[cpu] = stof(a[10]);
                preline_cpu_temp = cpu_temp[cpu];
            }
        }
    }

    void calculate_socket_avg_temp()
    {
        for (int socket = 0; socket < nr_sockets; socket++)
        {
            double temp_sum = 0;
            int socket_cpu_num = (int)socket_topo[socket].size();
            for (auto cpu : socket_topo[socket])
            {
                temp_sum += cpu_temp[cpu];
            }
            socket_avg_temp[socket] = temp_sum / socket_cpu_num;
        }
    }

    void calculate_vm_socket_temp()
    {
        for (int vm = 0; vm < nr_vms; vm++)
        {
            for (int socket = 0; socket < nr_sockets; socket++)
            {
                double temp_sum = 0;
                int vm_socket_cpu_num = (int)vm_socket_cpu[vm][socket].size();
                if (vm_socket_cpu_num == 0) // vm has no cpu on this socket
                    continue;
                for (auto cpu : vm_socket_cpu[vm][socket])
                {
                    temp_sum += cpu_temp[cpu];
                }
                vm_socket_temp[vm][socket] = temp_sum / vm_socket_cpu_num;
            }
        }
    }

    vector<string> split(string line)
    {
        vector<string> res;
        char str[500];
        for (int i = 0; i < (int)line.size(); i++)
            str[i] = line[i];
        const char *split = " ";
        char *p;
        p = strtok(str, split);
        while (p)
        {
            res.push_back(p);
            p = strtok(NULL, split);
        }
        return res;
    }
};

int main()
{
    init_socket_cpu_vm();
    init_cpu_duty_cycle();
    PowerStrategy ps;

    while (1) //main loop
    {

        string cmd = "timeout 1.2s turbo -i 1 > ./power.log" ;
        system(cmd.c_str());

        string des = "./power.log" ;
        ps.update_from_file(des);
        printf("\n================power data has updated!==============\n");

        for (int socket = 0; socket < nr_sockets; socket++)
        {
            printf("socket %d power is %f\n", socket, ps.socket_power[socket]);
            printf("socket %d avg temp is %f\n", socket, ps.socket_avg_temp[socket]);
        }

        for (int vm = 0; vm < nr_vms; vm++)
            for (int socket = 0; socket < nr_sockets; socket++)
                if (ps.vm_socket_temp[vm].count(socket))
                    printf("vm %d on socket %d power is %f\n", vm, socket, ps.vm_socket_temp[vm][socket]);
                else
                    printf("vm %d isn't on socket %d\n", vm, socket);

        for (int cpu = 0; cpu < nr_cpus; cpu++)
        {
            printf("cpu %d util is %f, temp is %f\n", cpu, ps.cpu_util[cpu], ps.cpu_temp[cpu]);
        }

        //safe cpus, increase duty_cycle, and clear
        for (int socket = 0; socket < nr_sockets; socket++)
        {
            if (ps.socket_power[socket] > ps.quota - ps.guard)
                continue;
            printf("safe cpus on socket %d are:\n", socket);
            for (auto cpu : ps.safe_cpu_need_increase[socket])
            {
                int cur_duty = read_duty_cycle(cpu);
                int new_duty = (cur_duty + 1) % 16;
                write_duty_cycle(cpu, new_duty);
                printf("cpu %d +1 level\n", cpu);
                if (new_duty == 0)
                    ps.socket_noise_cpus[socket].erase(cpu);
            }
            ps.safe_cpu_need_increase[socket].clear();
        }

        //noise cpus, decrease duty_cycle, and clear decrease
        for (int socket = 0; socket < nr_sockets; socket++)
        {
            if (ps.socket_power[socket] < ps.quota)
                continue;
            printf("noise cpus on socket %d are:\n", socket);
            for (auto cpu : ps.socket_noise_cpus[socket])
            {
                int cur_duty = read_duty_cycle(cpu);
                int new_duty = 15;
                if (cur_duty > 0)
                    new_duty = cur_duty - 1;
                if (cur_duty == 1)
                    new_duty = 1;
                write_duty_cycle(cpu, new_duty);
                printf("cpu %d -1 level\n", cpu);
            }
        }

        sleep(ps.interval);
    }

    return 0;
}
