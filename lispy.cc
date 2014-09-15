#include <stdio.h>
#include <iostream>
#include <readline/readline.h>
#include <readline/history.h>
#include <cstdlib>

#include "lispy.h"

int main(int argc, char *argv[]) {
    const std::string prompt(">> ");
    lispy::environment env;
    lispy::bind_std(env);

    while (true) {
        char *cmd = readline(prompt.c_str());
        if (!cmd)
            break;

        std::string command(cmd);

        if (command == "exit")
            break;

        add_history(command.c_str());

        try {
            lispy::atom result = lispy::exec(env, command);
            std::cout << result.repr() << std::endl;
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    std::cout << std::endl;

    return EXIT_SUCCESS;
}
