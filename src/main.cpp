#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <utility>
#include <readline/readline.h>
#include <readline/history.h>
#include "command_registry.h"

struct configstr {
    std::string name, tagline, author, description, hint, prompt;
    std::vector<std::pair<std::string, std::string>> commands;
};

std::string trimmer(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    size_t end = s.find_last_not_of(" \t");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

configstr load_config(const std::string& path) {
    configstr configuration;
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Warning: I can not open config file: " << path << "\n";
        return configuration;
    }
    bool is_command = false;
    std::string line;
    while (std::getline(file, line)) {
        line = trimmer(line);
        if (line.empty() || line[0] == '#') continue;
        if (line == "[commands]") { is_command = true; continue; }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trimmer(line.substr(0, eq));
        std::string value = trimmer(line.substr(eq + 1));
        if (is_command) {
            configuration.commands.emplace_back(key, value);
        } else if (key == "name") configuration.name = value;
        else if (key == "tagline") configuration.tagline = value;
        else if (key == "author") configuration.author = value;
        else if (key == "description") configuration.description = value;
        else if (key == "hint") configuration.hint = value;
        else if (key == "prompt") configuration.prompt = value;
    }
    return configuration;
}

void banner(const configstr& configuration) {
    std::cout << configuration.name << " — " << configuration.tagline << "\n";
    std::cout << "by " << configuration.author << "\n";
    std::cout << configuration.description << "\n";
    std::cout << configuration.hint << "\n";
}

std::vector<std::pair<std::string, std::string>> load_commands(const std::string& path) {
    std::vector<std::pair<std::string, std::string>> cmds;
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Warning: I can not open commands file: " << path << "\n";
        return cmds;
    }
    std::string line;
    while (std::getline(file, line)) {
        line = trimmer(line);
        if (line.empty() || line[0] == '#') continue;
        size_t sep = line.find(" - ");
        if (sep == std::string::npos) continue;
        std::string name = trimmer(line.substr(0, sep));
        std::string desc = trimmer(line.substr(sep + 3));
        cmds.emplace_back(name, desc);
    }
    return cmds;
}

void print_help(const std::vector<std::pair<std::string, std::string>>& cmds) {
    std::cout << "List of commands:\n";
    for (const auto& cmd : cmds) {
        std::cout << "  " << cmd.first << " - " << cmd.second << "\n";
    }
}

std::vector<std::pair<std::string, std::string>>& loaded_commands() {
    static std::vector<std::pair<std::string, std::string>> cmds;
    return cmds;
}

CommandRegistrar reg_help("help", []() {
    print_help(loaded_commands());
    return true;
});
CommandRegistrar reg_quit("quit", []() { return false; });
CommandRegistrar reg_exit("exit", []() { return false; });

int main() {
    configstr configuration = load_config(std::string(APP_ROOT) + "/src/config/iscons.conf");
    banner(configuration);

    auto commands = load_commands(std::string(APP_ROOT) + "/src/cmds.txt");
    loaded_commands() = commands;

    bool running = true;

    while (running) {
        char* line = readline(configuration.prompt.c_str());
        if (!line) {
            std::cout << "\n";
            break;
        }
        std::string input = trimmer(line);
        add_history(line);
        free(line);

        if (input.empty()) continue;

        bool unknown = true;
        for (const auto& cmd : commands) {
            if (cmd.first == input) { unknown = false; break; }
        }

        if (unknown) {
            std::cerr << "\033[31mError: I cannot find command \"" << input << "\". Type help for a list!\033[0m\n";
            continue;
        }

        auto it = registry().find(input);
        if (it != registry().end()) {
            running = it->second();
        } else {
            std::cout << input << ": not yet implemented\n";
        }
    }

    return 0;
}
