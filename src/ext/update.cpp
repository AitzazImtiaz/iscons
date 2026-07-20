#include <cstdlib>
#include <iostream>
#include <string>
#include "command_registry.h"
#include "oeis_paths.h"

CommandRegistrar reg_update("update", []() {
    std::cout << "Pulling latest is-cons source...\n";
    int src_result = std::system(("git -C \"" + std::string(APP_ROOT) + "\" pull").c_str());
    if (src_result != 0) {
        std::cerr << "\033[31mError: I could not pull iscons source. Has the doomsday arrived?\033[0m\n";
    }

    std::cout << "Syncing oeisdata...\n";
    sync_oeisdata();

    std::string build_dir = std::string(APP_ROOT) + "/build";
    std::cout << "Rebuilding is-cons...\n";
    std::system(("cmake -S \"" + std::string(APP_ROOT) + "\" -B \"" + build_dir + "\"").c_str());
    int build_result = std::system(("cmake --build \"" + build_dir + "\"").c_str());

    if (build_result != 0) {
        std::cerr << "\033[31mError: I failed rebuild. Check output above.\033[0m\n";
    } else {
        std::cout << "Rebuild complete. Restart iscons to use the updated build.\n";
    }

    return true;
});
