#include <iostream>
#include "command_registry.h"

CommandRegistrar reg_version("version", []() {
    std::cout << "iscons — development build\n";
    return true;
});
