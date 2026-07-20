#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <array>
#include "command_registry.h"

namespace {

bool path_exists(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0;
}

std::string run_capture(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return result;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

}

CommandRegistrar reg_fetch("fetch", []() {
    const char* home_env = std::getenv("HOME");
    if (!home_env) {
        std::cerr << "\033[31mError: I could not determine home directory.\033[0m\n";
        return true;
    }
    std::string home(home_env);
    std::string oeis_dir = home + "/oeis";
    std::string data_dir = oeis_dir + "/oeisdata";

    if (!path_exists(data_dir)) {
        std::cout << "oeisdata not found locally. Cloning into " << data_dir << " ...\n";
        std::system(("mkdir -p \"" + oeis_dir + "\"").c_str());
        int result = std::system(("git clone https://github.com/oeis/oeisdata.git \"" + data_dir + "\"").c_str());
        if (result != 0) {
            std::cerr << "\033[31mError: git clone failed.\033[0m\n";
        } else {
            std::cout << "Clone complete.\n";
        }
        return true;
    }

    std::string local_time = read_file(data_dir + "/time.txt");
    std::string remote_time = run_capture(
        "curl -s https://raw.githubusercontent.com/oeis/oeisdata/main/time.txt"
    );

    if (!remote_time.empty() && local_time == remote_time) {
        std::cout << "oeisdata is already up to date.\n";
    } else {
        std::cout << "oeisdata may be out of date. Pulling latest changes...\n";
        int result = std::system(("git -C \"" + data_dir + "\" pull").c_str());
        if (result != 0) {
            std::cerr << "\033[31mError: git pull failed.\033[0m\n";
        } else {
            std::cout << "Pull complete.\n";
        }
    }

    return true;
});
