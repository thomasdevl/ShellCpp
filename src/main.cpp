#include <complex>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_set>
#include <unistd.h>
#include <fcntl.h>

#include <vector>
#include <sys/wait.h>


class Shell {
public:
  Shell() : curDir(std::filesystem::current_path()), running(true) {
    // built ins
    commands["exit"] = [this](auto args){running = false;};
    commands["pwd"] = [this](auto args) {std::cout << curDir.string() << std::endl;};
    commands["echo"] = [this](auto args) { handle_echo(args); };
    commands["cd"] = [this](auto args) { handle_cd(args); };
    commands["type"] = [this](auto args) { handle_type(args); };
  }

  void run() {
    std::cout << std::unitbuf;
    std::string input;

    while (running) {
      std::cout << "$ ";
      if (!std::getline(std::cin, input)) break;

      // parsing
      std::vector<std::string> tokens = parse_arguments(input);
      if (tokens.empty()) continue;

      // redirecting
      std::string output_file;
      int original_stdout = -1;
      int original_stderr = -1;

      for (auto it = tokens.begin(); it != tokens.end(); ) {
        bool is_stdout = (*it == ">" || *it == "1>");
        bool is_stdout_append = (*it == ">>" || *it == "1>>");
        bool is_stderr = (*it == "2>");
        bool is_stderr_append = (*it == "2>>");

        if (is_stdout || is_stderr || is_stdout_append || is_stderr_append) {
          if (std::next(it) == tokens.end()) {
            std::cerr << "shell: syntax error near unexpected token 'newline'" << std::endl;
          }

          output_file = *std::next(it);
          int flags = O_WRONLY | O_CREAT;
          flags |= (is_stdout_append || is_stderr_append) ? O_APPEND : O_TRUNC;

          int fd = open(output_file.c_str(), flags, 0644);
          if (fd < 0) {
            perror("open");
            goto next_iteration;
          }

          if (is_stdout || is_stdout_append) {
            if (original_stdout == -1) original_stdout = dup(STDOUT_FILENO);
            dup2(fd, STDOUT_FILENO);
          } else {
            if (original_stderr == -1) original_stderr = dup(STDERR_FILENO);
            dup2(fd, STDERR_FILENO);
          }

          close(fd);
          tokens.erase(it, it + 2);
          break;
        } else {
          ++it;
        }
      }

      if (tokens.empty()) goto cleanup;

      {
        // separate cmd and arg
        std::string cmd_name = tokens[0];
        std::vector args(tokens.begin() + 1, tokens.end());

        // dispatch
        if (commands.contains(cmd_name)) {
          commands[cmd_name](args);
        } else {
          handle_external_command(cmd_name, args);
        }
      }

      cleanup:
        // restore STDOUT
        if (original_stdout != -1) {
          dup2(original_stdout, STDOUT_FILENO);
          close(original_stdout);
        }

      // restore STDERR
      if (original_stderr != -1) {
        dup2(original_stderr, STDERR_FILENO);
        close(original_stderr);
      }

      next_iteration:;
    }
  }

private:
  std::filesystem::path curDir;
  bool running;
  std::map<std::string, std::function<void(std::vector<std::string>)>> commands;
  std::unordered_set<std::string> builtins{"exit", "echo", "type","pwd", "cd"};

  std::string find_in_path(const std::string& cmd) {
    const char* env_p = std::getenv("PATH");
    if (!env_p) return "";

    std::stringstream ss(env_p);
    std::string path_dir;
    while (std::getline(ss, path_dir, ':')) {
      std::filesystem::path full_path = std::filesystem::path(path_dir) / cmd;
      if (std::filesystem::exists(full_path) && !access(full_path.string().c_str(), X_OK)) {
        return full_path.string();
      }
    }
    return "";
  }

  void handle_echo(const std::vector<std::string>& arg_list) {
    for (const auto & i : arg_list) {
      std::cout << i << " ";
    }
    std::cout << std::endl;

  }

  void handle_type(const std::vector<std::string>& arg_list) {
    if (arg_list.empty()) return;
    const std::string& cmd = arg_list[0];

    if (builtins.contains(cmd)) {
      std::cout << cmd << " is a shell builtin" << std::endl;
    } else {
      std::string path = find_in_path(cmd);
      if (!path.empty()) {
        std::cout << cmd << " is " << path << std::endl;
      } else {
        std::cerr << cmd << ": not found" << std::endl;
      }
    }
  }

  void handle_cd(const std::vector<std::string>& arg_list) {
    if (arg_list.empty()) return;
    const std::string& path_str = arg_list[0];
    std::filesystem::path targetDir;

    if (path_str == "~") {
      const char* home = std::getenv("HOME");
      targetDir = home ? home : "/";
    } else if (path_str.starts_with("/")) {
      targetDir = path_str;
    } else {
      targetDir = curDir / path_str;
    }

    // clean up path
    targetDir = std::filesystem::weakly_canonical(targetDir);

    if (std::filesystem::is_directory(targetDir)) {
      curDir = targetDir;
      std::filesystem::current_path(curDir); // Sync actual process dir
    } else {
      std::cerr << "cd: " << path_str << ": No such file or directory" << std::endl;
    }
  }

  void handle_external_command(const std::string& command, const std::vector<std::string>& arg_list) {
    std::string full_path = find_in_path(command);
    if (full_path.empty()) {
      std::cerr << command << ": command not found" << std::endl;
      return;
    }

    std::vector<char*> c_args;
    c_args.push_back(const_cast<char*>(command.c_str()));
    for (const auto& a : arg_list) {
      c_args.push_back(const_cast<char*>(a.c_str()));
    }
    c_args.push_back(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
      execv(full_path.c_str(), c_args.data());
      perror("execv");
      exit(1);
    }
    
    if (pid > 0) {
      int status;
      waitpid(pid, &status, 0);
    } else {
      perror("fork");
    }
  }

  std::vector<std::string> parse_arguments(const std::string& args) {
    std::vector<std::string> arg_list;
    std::string current_arg;
    char quote_char = '\0';
    bool escape_next = false;

    for (const char c : args) {

      // Handle escaped character immediately
      if (escape_next) {
        if (quote_char == '\"') {
          if (c != '\"' && c != '\\') {
            current_arg += '\\';
          }
        }
        current_arg += c;
        escape_next = false;
        continue;
      }

      if (quote_char == '\'') {
        // INSIDE SINGLE QUOTES: Everything is literal until the next '
        if (c == '\'') quote_char = '\0';
        else current_arg += c;
      }
      else if (quote_char == '\"') {
        // INSIDE DOUBLE QUOTES: Watch for \ or closing "
        if (c == '\\') escape_next = true;
        else if (c == '\"') quote_char = '\0';
        else current_arg += c;
      }
      else {
        // OUTSIDE QUOTES
        if (c == '\\') {
          escape_next = true;
        } else if (c == '\'' || c == '\"') {
          quote_char = c;
        } else if (std::isspace(static_cast<unsigned char>(c))) {
          if (!current_arg.empty()) {
            arg_list.push_back(current_arg);
            current_arg.clear();
          }
        } else {
          current_arg += c;
        }
      }
    }

    // Capture the final argument
    if (!current_arg.empty()) {
      arg_list.push_back(current_arg);
    }

    return arg_list;
  }

};

int main() {
  //  -- supported --
  // exit : exit shell
  // echo : print out args
  // type : type of file/command/etc
  // pwd : prints working directory
  // cd : change directory
  // parsing single and double quotes + \
  // redirecting 1> > 2> 1>> >>  
  // + all commands specified in PATH

  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  Shell myShell{};
  myShell.run();
  return 0;
}
