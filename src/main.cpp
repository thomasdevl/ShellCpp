#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_set>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>

#include <vector>
#include <sys/wait.h>
#include <termios.h>

#include "Trie.hpp"


class Shell {
public:
  Shell() : curDir(std::filesystem::current_path()), running(true) {
    // built ins
    commands["exit"] = [this](auto args){handle_exit();};
    commands["pwd"] = [this](auto args) {std::cout << curDir.string() << std::endl;};
    commands["echo"] = [this](auto args) { handle_echo(args); };
    commands["cd"] = [this](auto args) { handle_cd(args); };
    commands["type"] = [this](auto args) { handle_type(args); };
    commands["history"] = [this](auto args) {handle_history(args); };

    // add the commands to the Trie
    add_command_to_Trie(command_trie);

    const char* env_hist = std::getenv("HISTFILE");
    if (env_hist) {
      history = get_history_from_file(std::getenv("HISTFILE"));
    }
  }

  void run() {
    std::cout << std::unitbuf;
    int tab_counter = 0; // for consecutive tab presses
    int up_counter = 0;

    while (running) {
      std::cout << "$ " << std::flush;

      std::string input;
      setRawMode(true); // all individual keystrokes

      while (true) {
        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) break;

        if (c == '\r' || c == '\n') { // ENTER
          std::cout << "\n";
          tab_counter = 0;
          up_counter = 0;
          break;
        }

        if (c == '\t') { // TAB
          std::vector<std::string> matches = get_matches(input);

          if (matches.empty()) {
            // bell if no match
            std::cout << '\a' << std::flush;
            tab_counter = 0;
          }
          else if (matches.size() == 1) {
            // perfect autocomplete
            std::string completion = matches[0].substr(input.length());
            input += completion + " ";
            std::cout << completion << " " << std::flush;
            tab_counter = 0;
          }
          else {
            std::string lcp = command_trie.getLongestCommonPrefix(input);

            if (lcp.length() > input.length()) {
              // add the lcp
              std::string extra = lcp.substr(input.length());
              input = lcp;
              std::cout << extra << std::flush;
            } else {
              tab_counter++;

              if (tab_counter == 1) {
                std::cout << '\a' << std::flush; // bell
              } else if (tab_counter >= 2) {
                // multiple matches -> list them all.
                std::cout << "\n";
                for (size_t i = 0; i < matches.size(); ++i) {
                  std::cout << matches[i] << (i == matches.size() - 1 ? "" : "  ");
                }
                // Move to a new line and reprint the prompt + current typed text
                std::cout << "\n$ " << input << std::flush;
                tab_counter = 0; // Reset after showing
              }
            }
          }
        }
        else if (c == 127) { // BACKSPACE (ASCII 127)
          if (!input.empty()) {
            input.pop_back();
            std::cout << "\b \b" << std::flush; // move cursor back
          }
          tab_counter = 0;
        } else if (c== 27) {
          //potential escape
          char seq[3];
          // check for more char in the buffer
          if (read(STDIN_FILENO, &seq[0],1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
            if (seq[0] == '[') {
              switch (seq[1]) {
                case 'A': // UP ARROW
                  if (!history.empty() && up_counter < static_cast<int>(history.size())) {
                    up_counter++;
                    input = history[history.size() - up_counter];

                    // ^[2K clears line and \r moves to start of line
                    std::cout << "\33[2K\r$" << " " << input << std::flush;
                  }
                  break;
                case 'B': // DOWN ARROW
                  if (up_counter > 0) {
                    up_counter--;
                    if (up_counter == 0) {
                      input = "";
                    } else {
                      input = history[history.size() - up_counter];
                    }
                    std::cout << "\33[2K\r$" << " "  << input << std::flush;
                  }
                  break;
                case 'C': // RIGHT ARROW
                  break; // TODO
                case 'D': // LEFT ARROW
                  break; // TODO
              }
            }
          }
        } else { // normal char
          input += c;
          std::cout << c << std::flush;
          tab_counter = 0;
        }
      }


      setRawMode(false);

      if (input.empty()) continue;

      history.push_back(input);

      // parsing
      std::vector<std::string> tokens = parse_arguments(input);
      if (tokens.empty()) continue;

      std::vector<std::vector<std::string>> pipeline;
      std::vector<std::string>  current_cmd;

      for (const auto& token : tokens) {
        if (token == "|") {
          pipeline.push_back(current_cmd);
          current_cmd.clear();
        } else {
          current_cmd.push_back(token);
        }
      }
      pipeline.push_back(current_cmd);

      if (pipeline.size() > 1) {
        handle_pipeline(pipeline);

      } else {

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
          }
          ++it;
        }

        if (tokens.empty()) goto cleanup;

        {
          // separate cmd and arg
          std::string cmd_name = tokens[0];
          std::vector args(tokens.begin() + 1, tokens.end());

          // dispatch
          execute_command(tokens,false);
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
  }

private:
  std::filesystem::path curDir;
  bool running;
  std::map<std::string, std::function<void(std::vector<std::string>)>> commands;
  std::unordered_set<std::string> builtins{"exit", "echo", "type","pwd", "cd","history"};
  Trie command_trie;
  std::vector<std::string> history = {};
  int appending_until = 0;

  void handle_exit() {

    const char* env_hist = std::getenv("HISTFILE");
    if (env_hist) {
      write_history_to_file(std::getenv("HISTFILE"));
    }

    running = false;
  }

  void add_command_to_Trie(Trie& command_trie) {

    // add built-ins
    for (const auto& command: builtins) {
      command_trie.insert(command);
    }

    // add all the executables from PATH
    const char* path_env = std::getenv("PATH");
    if (!path_env) return;

    std::stringstream ss(path_env);
    std::string dir_path;

    // splits path by :
    while (std::getline(ss, dir_path, ':')) {
      if (dir_path.empty() || !std::filesystem::exists(dir_path)) continue;

      try {
        for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
          // Ensure it's a file and we have permission to execute it
          if (entry.is_regular_file()) {
            auto permissions = entry.status().permissions();
            bool is_executable = (permissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none;

            if (is_executable) {
              command_trie.insert(entry.path().filename().string());
            }
          }
        }
      } catch (const std::filesystem::filesystem_error& e) {}
    }

  }

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
    for (size_t i = 0; i < arg_list.size(); ++i) {
      std::cout << arg_list[i];
      if (i < arg_list.size() -1) {
         std::cout << " ";
      }
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

  void setRawMode(bool enable) {
    static struct termios oldt;
    static bool firstCall = true;

    if (firstCall) {
      tcgetattr(STDIN_FILENO, &oldt);
      firstCall = false;
    }

    if (enable) {
      // current terminal settings
      static struct termios newt = oldt;

      // ICANON disables line buffering (Canonical mode)
      // ECHO disables printing the character back to the screen
      newt.c_lflag &= ~(ICANON | ECHO);

      // Apply new settings
      tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {

      // restore original
      tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
  }

  std::vector<std::string> get_matches(std::string & partial) {
    if (partial.empty()) return {};

    std::vector<std::string> matches = command_trie.get_completions(partial);

    // remove duplicates
    std::sort(matches.begin(), matches.end());
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());

    return matches;
  }

  void run_exec(const std::string& path, const std::vector<std::string>& tokens) {
    std::vector<char*> c_args;
    for (const auto& t : tokens) c_args.push_back(const_cast<char*>(t.c_str()));
    c_args.push_back(nullptr);
    execv(path.c_str(), c_args.data());
    perror("execv");
    exit(1);
  }

  void execute_command(std::vector<std::string> tokens, bool is_child) {
    if (tokens.empty()) return;
    std::string cmd_name = tokens[0];
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    if (commands.contains(cmd_name)) {
      commands[cmd_name](args);
      // If we are in a child process (like in a pipe), we must exit
      if (is_child) exit(0);
    } else {
      std::string full_path = find_in_path(cmd_name);
      if (full_path.empty()) {
        std::cerr << cmd_name << ": command not found" << std::endl;
        if (is_child) exit(1);
        return;
      }

      // External commands always need a fork if we aren't already in one
      if (!is_child) {
        pid_t pid = fork();
        if (pid == 0) {
          run_exec(full_path, tokens);
        } else {
          waitpid(pid, nullptr, 0);
        }
      } else {
        run_exec(full_path, tokens);
      }
    }
  }

  void handle_pipeline(std::vector<std::vector<std::string>>& pipeline) {
    int num_cmds = pipeline.size();
    int prev_pipe_read_end = -1;

    for (int i = 0; i < num_cmds; ++i) {
      int pipefds[2];

      // create pipe for all but last
      if (i < num_cmds - 1) {
        if (pipe(pipefds) == -1) {
          perror("pipe");
          return;
        }
      }

      pid_t pid = fork();
      if (pid == 0) { // child

        // get input from previous pipe
        if (prev_pipe_read_end != -1) {
          dup2(prev_pipe_read_end, STDIN_FILENO);
          close(prev_pipe_read_end);
        }

        // send output to the current pipe
        if (i < num_cmds - 1) {
          close(pipefds[0]); // Close unused read end
          dup2(pipefds[1],STDOUT_FILENO);
          close(pipefds[1]);
        }

        execute_command(pipeline[i], true);
        exit(0);
      }

      if (pid < 0) {
        perror("fork");
        return;
      }

      if (prev_pipe_read_end != -1) close(prev_pipe_read_end);
      if (i < num_cmds - 1) {
        close(pipefds[1]); // parent doesn't write
        prev_pipe_read_end = pipefds[0]; // save read end for next child
      }
    }

    // wait for all children
    while (wait(nullptr) > 0);
  }

  std::vector<std::string> get_history_from_file(const std::filesystem::path& path_to_file) {
    std::ifstream f(path_to_file);

    if (!f.is_open()) {
      std::cerr << "Error opening file : " << path_to_file.string() << std::endl;
    }

    std::vector<std::string> content;
    std::string s;

    while (std::getline(f,s)) {
      content.push_back(s);
    }

    f.close();
    return content;
  }

  void write_history_to_file(const std::filesystem::path& path_to_file) {
    std::ofstream f(path_to_file);

    if (!f.is_open()) {
      std::cerr << "Error opening file : " << path_to_file.string() << std::endl;
    }

    for (const auto& line: history) {
      f << line << std::endl;
    }

    f.close();
  }

  void append_history_to_file(const std::filesystem::path& path_to_file) {
    std::ofstream f(path_to_file, std::ofstream::app | std::ofstream::out);


    if (!f.is_open()) {
      std::cerr << "Error opening file : " << path_to_file.string() << std::endl;
    }

    for (int i = appending_until; i < history.size(); ++i) {
      f << history[i] << std::endl;
    }

    f.close();
  }

  void handle_history(const std::vector<std::string>& arg_list) {


    for (int i = 0; i < arg_list.size(); ++i) {

      // -r command
      if (arg_list[i] == "-r") {
        // get filename
        if (i+1 >= arg_list.size()) {
          std::cout << "history : no filename given to -r" << std::endl;
          return;
        }

        std::string last_cmd = history[history.size()-1];
        history = get_history_from_file(arg_list[i+1]);
        history.insert(history.begin(), last_cmd);

        return;
      }

      // -w command
      if (arg_list[i] == "-w") {
        // get filename
        if (i+1 >= arg_list.size()) {
          std::cout << "history : no filename given to -w" << std::endl;
          return;
        }

        write_history_to_file(arg_list[i+1]);

        return;
      }

      // -a command
      if (arg_list[i] == "-a") {
        // get filename
        if (i+1 >= arg_list.size()) {
          std::cout << "history : no filename given to -w" << std::endl;
          return;
        }

        append_history_to_file(arg_list[i+1]);

        appending_until = history.size();

        return;
      }
    }

    int i = 0;
    int arg = 0;
    try {
      if (!arg_list.empty()) {
        arg = history.size() - stoi(arg_list[0]);
      }
    } catch (std::invalid_argument& e ) {
      arg = 0;
      std::cout << "std::invalid_argument::what(): " << e.what() << '\n';
    }

    for (i = i + arg; i < history.size(); ++i) {
        std::cout << "    " << i+1 << "  " << history[i] << std::endl;
    }
  }
};

int main() {
  //  -- supported --
  // exit : exit Shell
  // echo : print out args
  // type : type of file/command/etc
  // pwd : prints working directory
  // cd : change directory
  // history -r -w -a : show command history
  // parsing single and double quotes + \ + ~ (HOME)
  // redirecting 1> > 2> 1>> >>
  // autocompletion
  // pipe redirecting |
  // up + down arrow history navigation
  // history saving and reading from HISTFILE
  // + all commands specified in PATH

  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  Shell myShell{};
  myShell.run();
  return 0;
}
