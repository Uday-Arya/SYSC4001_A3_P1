#include "interrupts_101268848_101281787.hpp"
#include <unordered_map>

// Round Robin scheduler (quantum = 100 ms)
// Uses print_exec_* from interrupts.hpp which produces the 49-width table.

const int RR_QUANTUM = 100;

std::tuple<std::string> run_simulation(std::vector<PCB> list_process) {
    std::vector<PCB> ready_queue;
    std::vector<std::pair<PCB, unsigned int>> wait_queue; // pair<pcb, completion_time>
    std::vector<PCB> job_list; // mirror of processes that have been seen/assigned
    unsigned int current_time = 0;
    PCB running;
    idle_CPU(running);

    std::string execution_status = print_exec_header();

    const size_t total_processes = list_process.size();
    size_t terminated_count = 0;

    // Helper: try assign NOT_ASSIGNED processes whose arrival <= current_time
    auto try_assign_pending = [&](unsigned int now){
        for (auto &p : list_process) {
            if (p.arrival_time <= now && p.state == NOT_ASSIGNED) {
                if (assign_memory(p)) {
                    p.state = READY;
                    ready_queue.push_back(p);
                    execution_status += print_exec_status(now, p.PID, NEW, READY);
                    // add to job_list if not already present
                    bool found=false;
                    for(auto &j:job_list) if(j.PID==p.PID) { found=true; j = p; break; }
                    if(!found) job_list.push_back(p);
                }
            }
        }
    };

    // initialize processes with arrival_time == 0
    for (auto &p : list_process) {
        if (p.arrival_time == 0) {
            if (assign_memory(p)) {
                p.state = READY;
                ready_queue.push_back(p);
                job_list.push_back(p);
                execution_status += print_exec_status(0, p.PID, NEW, READY);
            } else {
                p.state = NOT_ASSIGNED;
                job_list.push_back(p);
            }
        } else {
            // not arrived yet: mark as NOT_ASSIGNED so we can attempt assign after arrival
            p.state = NOT_ASSIGNED;
        }
    }

    while (terminated_count < total_processes) {

        // --- arrivals at this tick ---
        for (auto &p : list_process) {
            if (p.arrival_time == current_time && p.arrival_time != 0) {
                // mark NOT_ASSIGNED first (if not already)
                if (p.state == NOT_ASSIGNED) {
                    if (assign_memory(p)) {
                        p.state = READY;
                        ready_queue.push_back(p);
                        execution_status += print_exec_status(current_time, p.PID, NEW, READY);
                    } else {
                        // remain NOT_ASSIGNED; will retry next ticks
                        p.state = NOT_ASSIGNED;
                    }
                    // ensure in job_list
                    bool found=false;
                    for(auto &j:job_list) if(j.PID==p.PID) { found=true; j=p; break; }
                    if(!found) job_list.push_back(p);
                }
            }
        }

        // Retry to assign memory for earlier arrivals that couldn't fit
        try_assign_pending(current_time);

        // --- handle I/O completions scheduled for this tick ---
        for (auto it = wait_queue.begin(); it != wait_queue.end();) {
            if (it->second == current_time) {
                PCB p = it->first;
                p.state = READY;
                execution_status += print_exec_status(current_time, p.PID, WAITING, READY);
                // update list_process & job_list entry
                for (auto &lp : list_process) if (lp.PID == p.PID) lp = p;
                for (auto &jp : job_list) if (jp.PID == p.PID) jp = p;
                ready_queue.push_back(p);
                it = wait_queue.erase(it);
            } else {
                ++it;
            }
        }

        // --- Scheduler dispatch if CPU idle ---
        if (running.PID == -1) {
            if (!ready_queue.empty()) {
                PCB next = ready_queue.front();
                ready_queue.erase(ready_queue.begin());
                next.state = RUNNING;
                next.start_time = current_time;
                // sync into job_list/list_process
                for (auto &lp : list_process) if (lp.PID == next.PID) lp = next;
                for (auto &jp : job_list) if (jp.PID == next.PID) jp = next;
                execution_status += print_exec_status(current_time, next.PID, READY, RUNNING);
                running = next;
            }
        }

        // --- Execute one ms of CPU if running ---
        if (running.PID != -1) {
            running.remaining_time--;
            // update in job_list/list_process
            for (auto &lp : list_process) if (lp.PID == running.PID) lp = running;
            for (auto &jp : job_list) if (jp.PID == running.PID) jp = running;

            unsigned int executed_time = running.processing_time - running.remaining_time;
            unsigned int transition_time = current_time + 1; // events from this ms are timestamped at current_time+1

            // I/O request after this ms?
            if (running.io_freq > 0 && executed_time > 0 && (executed_time % running.io_freq) == 0 && running.remaining_time > 0) {
                // RUNNING -> WAITING at transition_time
                execution_status += print_exec_status(transition_time, running.PID, RUNNING, WAITING);
                // schedule completion at transition_time + io_duration
                unsigned int completion_time = transition_time + running.io_duration;
                PCB p = running;
                p.state = WAITING;
                wait_queue.push_back({p, completion_time});
                // update job_list/list_process
                for (auto &lp : list_process) if (lp.PID == p.PID) lp = p;
                for (auto &jp : job_list) if (jp.PID == p.PID) jp = p;
                // CPU becomes idle
                idle_CPU(running);
            }
            // Process finished this ms?
            else if (running.remaining_time == 0) {
                execution_status += print_exec_status(transition_time, running.PID, RUNNING, TERMINATED);
                terminate_process(running, job_list);
                terminated_count++;
                idle_CPU(running);
            }
            // RR quantum expiry?
            else if ((current_time - running.start_time + 1) % RR_QUANTUM == 0) {
                // preempt at transition_time
                execution_status += print_exec_status(transition_time, running.PID, RUNNING, READY);
                running.state = READY;
                // push to back of ready queue
                ready_queue.push_back(running);
                // update job_list/list_process
                for (auto &lp : list_process) if (lp.PID == running.PID) lp = running;
                for (auto &jp : job_list) if (jp.PID == running.PID) jp = running;
                idle_CPU(running);
            }
        }

        current_time++;
    } // end while

    execution_status += print_exec_footer();
    return std::make_tuple(execution_status);
}

int main(int argc, char** argv) {
    if(argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrupts_RR <your_input_file.txt>" << std::endl;
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
        // mark NOT_ASSIGNED so memory attempt is made when arrives
        new_process.state = NOT_ASSIGNED;
        list_process.push_back(new_process);
    }
    input_file.close();

    auto [exec] = run_simulation(list_process);
    write_output(exec, "execution.txt");

    return 0;
}
