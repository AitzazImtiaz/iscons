#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include "command_registry.h"

namespace fs = std::filesystem;

namespace {

std::string home_dir() {
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    return home ? std::string(home) : ".";
}

std::string validate_download(const std::string& path) {
    if (!fs::exists(path)) {
        return "file was not created";
    }

    auto size = fs::file_size(path);
    if (size == 0) {
        return "downloaded file is empty";
    }
    if (size < 500) {
        return "downloaded file is suspiciously small (" + std::to_string(size) + " bytes)";
    }

    std::ifstream file(path);
    std::string first_line;
    std::getline(file, first_line);

    if (first_line.find("<!DOCTYPE") != std::string::npos ||
        first_line.find("<html") != std::string::npos ||
        first_line.find("<HTML") != std::string::npos) {
        return "downloaded content looks like an HTML page, not the data table";
    }

    return "";
}

}

CommandRegistrar reg_pull("pull", []() {
    std::string cons_dir = home_dir() + "/oeis/cons";
    std::string dest = cons_dir + "/allascii.txt";
    std::string tmp = cons_dir + "/allascii.txt.tmp";

    fs::create_directories(cons_dir);

    std::cout << "Pulling NIST constants table...\n";
    int result = std::system(
        ("curl -f -o \"" + tmp + "\" https://physics.nist.gov/cuu/Constants/Table/allascii.txt").c_str()
    );

    if (result != 0) {
        std::cerr << "\033[31mError: download failed (server error or network issue).\033[0m\n";
        fs::remove(tmp);
        return true;
    }

    std::string problem = validate_download(tmp);
    if (!problem.empty()) {
        std::cerr << "\033[31mError: " << problem << ". Aborting, existing file left untouched.\033[0m\n";
        fs::remove(tmp);
        return true;
    }

    fs::rename(tmp, dest);
    std::cout << "Pull complete: " << dest << "\n";
    return true;
});
