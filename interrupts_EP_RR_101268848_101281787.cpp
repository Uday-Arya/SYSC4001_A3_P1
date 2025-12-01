#include "interrupts_101268848_101281787.hpp"
#include <unordered_map>

const int RR_ER_QUANTUM = 100;

// smaller PID -> higher priority
// Round-Robin among same-priority values (i.e., same PID ordering ties unlikely unless equal PID)

std::tuple<std::string> run_simulation(std::vector<PCB> list_process) {
    std::vector<PCB> ready_queue;
    std::vector<std::pair<PCB,unsigned int>> wait_queue;
    std::vector<PCB> job_list;
    unsigned int current_time = 0;
    PCB running;
    idle_CPU(running);

    std::string execution_status = print_exec_header();
    const size_t total_processes = list_process.size();
    size_t terminated_count = 0;

    for (auto &p : list_process) p.state = NOT_ASSIGNED;

    // arrivals at time 0
    for (auto &p : list_process) {
        if (p.arrival_time == 0) {
            if (assign_memory(p)) {
                p.state = READY;
                ready_queue.push_back(p);
                execution_status += print_exec_status(0, p.PID, NEW, READY);
            } else {
                p.state = NOT_ASSIGNED;
            }
            job_list.push_back(p);
        }
    }

    auto try_assign_pending = [&](unsigned int now){
        for (auto &p : list_process) {
            if (p.arrival_time <= now && p.state == NOT_ASSIGNED) {
                if (assign_memory(p)) {
                    p.state = READY;
                    ready_queue.push_back(p);
                    execution_status += print_exec_status(now, p.PID, NEW, READY);
                    bool found=false;
                    for(auto &j:job_list) if(j.PID==p.PID) { j=p; found=true; break; }
                    if(!found) job_list.push_back(p);
                }
            }
        }
    };

    auto sort_ready_by_priority = [&](){
        std::sort(ready_queue.begin(), ready_queue.end(), [](const PCB &a, const PCB &b){ return a.PID > b.PID; });
    };

    while (terminated_count < total_processes) {

        // arrivals at this tick
        for (auto &p : list_process) {
            if (p.arrival_time == current_time && p.arrival_time != 0) {
                if (assign_memory(p)) {
                    p.state = READY;
                    ready_queue.push_back(p);
                    execution_status += print_exec_status(current_time, p.PID, NEW, READY);
                } else {
                    p.state = NOT_ASSIGNED;
                }
                bool found=false;
                for(auto &j:job_list) if(j.PID==p.PID) { j=p; found=true; break; }
                if(!found) job_list.push_back(p);
            }
        }

        try_assign_pending(current_time);

        // handle io completions
        for (auto it = wait_queue.begin(); it != wait_queue.end();) {
            if (it->second == current_time) {
                PCB p = it->first;
                p.state = READY;
                execution_status += print_exec_status(current_time, p.PID, WAITING, READY);
                for (auto &lp : list_process) if (lp.PID == p.PID) lp = p;
                for (auto &jp : job_list) if (jp.PID == p.PID) jp = p;
                ready_queue.push_back(p);
                it = wait_queue.erase(it);
            } else ++it;
        }

        // sort ready by priority to see top
        sort_ready_by_priority();

        // Preemption: if a higher-priority process is ready (smaller PID) than running -> preempt immediately
        if (running.PID != -1 && !ready_queue.empty()) {
            PCB top = ready_queue.back();
            if (top.PID < running.PID) {
                // preempt now (at current_time)
                execution_status += print_exec_status(current_time, running.PID, RUNNING, READY);
                running.state = READY;
                ready_queue.push_back(running);
                // dispatch top immediately
                ready_queue.pop_back();
                PCB next = top;
                next.state = RUNNING;
                next.start_time = current_time;
                execution_status += print_exec_status(current_time, next.PID, READY, RUNNING);
                for (auto &lp : list_process) if (lp.PID == next.PID) lp = next;
                for (auto &jp : job_list) if (jp.PID == next.PID) jp = next;
                running = next;
            }
        }

        // If CPU idle, dispatch highest priority
        if (running.PID == -1) {
            if (!ready_queue.empty()) {
                sort_ready_by_priority();
                PCB next = ready_queue.back();
                ready_queue.pop_back();
                next.state = RUNNING;
                next.start_time = current_time;
                execution_status += print_exec_status(current_time, next.PID, READY, RUNNING);
                for (auto &lp : list_process) if (lp.PID == next.PID) lp = next;
                bool found=false;
                for(auto &jp:job_list) if(jp.PID==next.PID) { jp=next; found=true; break; }
                if(!found) job_list.push_back(next);
                running = next;
            }
        }

        // execute 1 ms if running
        if (running.PID != -1) {
            running.remaining_time--;
            for (auto &lp : list_process) if (lp.PID == running.PID) lp = running;
            for (auto &jp : job_list) if (jp.PID == running.PID) jp = running;

            unsigned int executed_time = running.processing_time - running.remaining_time;
            unsigned int transition_time = current_time + 1;

            if (running.io_freq > 0 && executed_time > 0 && (executed_time % running.io_freq) == 0 && running.remaining_time > 0) {
                // RUNNING -> WAITING
                execution_status += print_exec_status(transition_time, running.PID, RUNNING, WAITING);
                PCB p = running;
                p.state = WAITING;
                unsigned int completion_time = transition_time + p.io_duration;
                wait_queue.push_back({p, completion_time});
                for (auto &lp : list_process) if (lp.PID == p.PID) lp = p;
                for (auto &jp : job_list) if (jp.PID == p.PID) jp = p;
                idle_CPU(running);
            } else if (running.remaining_time == 0) {
                execution_status += print_exec_status(transition_time, running.PID, RUNNING, TERMINATED);
                terminate_process(running, job_list);
                terminated_count++;
                idle_CPU(running);
            } else if ((current_time - running.start_time + 1) % RR_ER_QUANTUM == 0) {
                // Round-robin among same-priority: preempt at transition_time
                execution_status += print_exec_status(transition_time, running.PID, RUNNING, READY);
                running.state = READY;
                ready_queue.push_back(running);
                for (auto &lp : list_process) if (lp.PID == running.PID) lp = running;
                for (auto &jp : job_list) if (jp.PID == running.PID) jp = running;
                idle_CPU(running);
            }
        }

        current_time++;
    }

    execution_status += print_exec_footer();
    return std::make_tuple(execution_status);
}

int main(int argc, char** argv) {
    if(argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrupts_RR_ER <your_input_file.txt>" << std::endl;
        return -1;
    }

    auto file_name = argv[1];
    std::ifstream input_file(file_name);

    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        return -1;
    }

    std::string line;
    std::vector<PCB> list_process;
    while(std::getline(input_file, line)) {
        if (line.size() == 0) continue;
        auto input_tokens = split_delim(line, ", ");
        auto new_process = add_process(input_tokens);
        new_process.state = NOT_ASSIGNED;
        list_process.push_back(new_process);
    }
    input_file.close();

    auto [exec] = run_simulation(list_process);
    write_output(exec, "execution.txt");

    return 0;
}
