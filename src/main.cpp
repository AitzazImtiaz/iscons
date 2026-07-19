#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <utility>
#include <readline/readline.h>
#include <readline/history.h>

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

int main() {
    configstr configuration = load_config(std::string(APP_ROOT) + "/src/config/iscons.conf");
    banner(configuration);

    while (true) {
        char* line = readline(configuration.prompt.c_str());

        if (!line) {
            std::cout << "\n";
            break;
        }

        std::string input = trimmer(line);
        add_history(line);

        if (input == "quit" || input == "exit") {
            free(line);
            break;
        }

        std::cout << ">> " << input << "\n";
        free(line);
    }

    return 0;
}
