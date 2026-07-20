#ifndef COMMAND_REGISTRY_H
#define COMMAND_REGISTRY_H

#include <string>
#include <map>
#include <functional>

using CommandHandler = std::function<bool()>;

inline std::map<std::string, CommandHandler>& registry() {
    static std::map<std::string, CommandHandler> instance;
    return instance;
}

inline void register_command(const std::string& name, CommandHandler handler) {
    registry()[name] = handler;
}

struct CommandRegistrar {
    CommandRegistrar(const std::string& name, CommandHandler handler) {
        register_command(name, handler);
    }
};

#endif
