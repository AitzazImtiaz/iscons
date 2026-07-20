#include "oeis_paths.h"
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <array>

namespace fs = std::filesystem;

namespace {

std::string home_dir() {
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");  // Windows fallback
    return home ? std::string(home) : ".";
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

}  // namespace

std::string oeisdata_dir() {
    return home_dir() + "/oeis/oeisdata";
}

bool oeisdata_exists() {
    return fs::exists(oeisdata_dir());
}

bool sync_oeisdata() {
    std::string oeis_root = home_dir() + "/oeis";
    std::string data_dir = oeisdata_dir();

    if (!oeisdata_exists()) {
        std::cout << "oeisdata not found locally. Cloning into " << data_dir << " ...\n";
        fs::create_directories(oeis_root);
        int result = std::system(("git clone https://github.com/oeis/oeisdata.git \"" + data_dir + "\"").c_str());
        if (result != 0) {
            std::cerr << "\033[31mError: git clone failed.\033[0m\n";
            return false;
        }
        std::cout << "Clone complete.\n";
        return true;
    }

    std::string local_time = read_file(data_dir + "/time.txt");
    std::string remote_time = run_capture(
        "curl -s https://raw.githubusercontent.com/oeis/oeisdata/main/time.txt"
    );

    if (!remote_time.empty() && local_time == remote_time) {
        std::cout << "oeisdata is already up to date.\n";
        return true;
    }

    std::cout << "oeisdata may be out of date. Pulling latest changes...\n";
    int result = std::system(("git -C \"" + data_dir + "\" pull").c_str());
    if (result != 0) {
        std::cerr << "\033[31mError: git pull failed.\033[0m\n";
        return false;
    }
    std::cout << "Pull complete.\n";
    return true;
}
