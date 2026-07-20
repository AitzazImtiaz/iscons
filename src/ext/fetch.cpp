#include "command_registry.h"
#include "oeis_paths.h"

CommandRegistrar reg_fetch("fetch", []() {
    sync_oeisdata();
    return true;
});
