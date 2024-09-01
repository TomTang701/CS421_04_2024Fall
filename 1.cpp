#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <process.h>
#include <algorithm>  

struct Job {
    HANDLE handle;
    DWORD pid;
    std::string command;
};

std::vector<Job> jobs;

void executeCommand(char* command[], bool isBackground) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::string commandStr;
    for (int i = 0; command[i] != NULL; ++i) {
        commandStr += command[i];
        commandStr += " ";
    }

    // Convert commandStr to LPSTR
    char* cmd = const_cast<char*>(commandStr.c_str());

    // Create a new process
    if (CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        if (isBackground) {
            jobs.push_back({ pi.hProcess, pi.dwProcessId, command[0] });
            std::cout << "tish: " << pi.dwProcessId << " started in background" << std::endl;
            CloseHandle(pi.hThread); // Close thread handle as we don't need it
        } else {
            // Wait for the process to finish
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    } else {
        std::cerr << "tish: command not found: " << command[0] << std::endl;
    }
}

void listJobs() {
    std::cout << "Background jobs:" << std::endl;
    for (const auto& job : jobs) {
        std::cout << job.pid << " " << job.command << std::endl;
    }
}

void killJob(DWORD pid) {
    auto it = std::remove_if(jobs.begin(), jobs.end(), [pid](const Job& job) {
        return job.pid == pid;
    });

    if (it != jobs.end()) {
        TerminateProcess(it->handle, 0);
        CloseHandle(it->handle);
        jobs.erase(it, jobs.end());
        std::cout << "tish: killed process " << pid << std::endl;
    } else {
        std::cerr << "tish: no such process " << pid << std::endl;
    }
}

void terminateAllJobs() {
    for (const auto& job : jobs) {
        TerminateProcess(job.handle, 0);
        CloseHandle(job.handle);
        std::cout << "tish: killed process " << job.pid << std::endl;
    }
    jobs.clear();
}

int main() {
    char input[256];

    while (true) {
        std::cout << "tish>> ";
        std::cin.getline(input, 256);

        // Split the input into command and arguments
        char* command[10];
        int i = 0;
        char* token = strtok(input, " ");
        bool isBackground = false;

        while (token != nullptr) {
            command[i++] = token;
            token = strtok(nullptr, " ");
        }
        command[i] = nullptr;

        if (i == 0) continue;  // Empty input, skip

        // Handle internal commands
        if (strcmp(command[0], "bye") == 0) {
            terminateAllJobs();
            break;
        } else if (strcmp(command[0], "jobs") == 0) {
            listJobs();
            continue;
        } else if (strcmp(command[0], "kill") == 0) {
            if (command[1] != nullptr) {
                killJob(atoi(command[1]));
            } else {
                std::cerr << "tish: kill: missing PID" << std::endl;
            }
            continue;
        }

        // Check if the last argument is "&", indicating background execution
        if (strcmp(command[i - 1], "&") == 0) {
            isBackground = true;
            command[i - 1] = nullptr;  // Remove "&" from command
        }

        // Execute the command
        executeCommand(command, isBackground);
    }

    return 0;
}

