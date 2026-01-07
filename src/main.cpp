#include <complex>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <unistd.h>

#include <vector>
#include <sys/wait.h>

void run_executable(const std::string& path, const std::string& command, const std::string& args_str) {
  std::vector<char*> c_args;

  // argv[0] is traditionally the command name itself
  c_args.push_back(const_cast<char*>(command.c_str()));

  // Split args_str by spaces to handle multiple flags (e.g., -l -a)
  std::stringstream ss(args_str);
  std::string arg;
  std::vector<std::string> arg_list; // Keep strings alive in memory
  while (ss >> arg) {
    arg_list.push_back(arg);
  }

  for (const auto& a : arg_list) {
    c_args.push_back(const_cast<char*>(a.c_str()));
  }
  c_args.push_back(nullptr); // Null terminator required for execv

  pid_t pid = fork();
  if (pid == 0) {
    // Child process
    execv(path.c_str(), c_args.data());
    // If execv returns, it failed
    perror("execv");
    exit(1);
  } else if (pid > 0) {
    // Parent process
    int status;
    waitpid(pid, &status, 0);
  } else {
    perror("fork");
  }
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string input;
  std::cout << "$ ";

  while (std::getline(std::cin,input)) {
    std::string delimiter = " ";
    auto pos = input.find(delimiter);

    std::string command = input.substr(0,pos);
    std::string args = input.substr(pos+1,input.length());

    if (command == "exit") {
      break;

    } else if (command == "echo") {
      std::cout << args << std::endl;

    } else if (command == "type") {
      std::unordered_set<std::string> builtins{"exit", "echo", "type"};

      if (builtins.contains(args)) {
        std::cout << args << " is a shell builtin" << std::endl;
      } else {
        bool found = false;
        const char* env_p = std::getenv("PATH");

        if (env_p) {
          std::stringstream ss(env_p);
          std::string path_dir;

          while (std::getline(ss, path_dir, ':')) {
            // Combine directory and command name
            std::filesystem::path full_path = std::filesystem::path(path_dir) / args;

            if (std::filesystem::exists(full_path)) {

              // check if execute permissions
              if (!  access(full_path.string().c_str(), X_OK)){
                std::cout << args << " is " << full_path.string() << std::endl;
                found = true;
                break;
            }
            }
          }
        }

        if (!found) {
          std::cout << args << ": not found" << std::endl;
        }
      }


    } else {

      // check for implementation in PATH
      bool found = false;
      const char* env_p = std::getenv("PATH");

      if (env_p) {
        std::stringstream ss(env_p);
        std::string path_dir;

        while (std::getline(ss, path_dir, ':')) {
          // Combine directory and command name
          std::filesystem::path full_path = std::filesystem::path(path_dir) / command;

          if (std::filesystem::exists(full_path)) {

            // check if execute permissions
            if (!  access(full_path.string().c_str(), X_OK)){
              run_executable(full_path.string(), command, args);
              found = true;
              break;
            }
          }
        }
      }


      if (!found) {
        std::cout << command << ": command not found" << std::endl;
      }
    }

    std::cout << "$ ";
  }

  return 0;
}
