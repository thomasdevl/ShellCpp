#include <algorithm>
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
#include <termios.h>


// Trie used for autocompletion
class TrieNode {
public:
  bool endofWord;
  std::unordered_map<char, TrieNode*> children;

  TrieNode() : endofWord(false){}

  ~TrieNode() {
    for (auto& pair : children) {
      delete pair.second;
    }
  }
};

class Trie {
  TrieNode* root;

  // Helper for recursive prefix searching
  void collectAllWords(TrieNode* node, std::string currentPrefix, std::vector<std::string>& results) {
    if (node->endofWord) {
      results.push_back(currentPrefix);
    }
    for (auto const& [ch, childNode] : node->children) {
      collectAllWords(childNode, currentPrefix + ch, results);
    }
  }

public:
  Trie() {root = new TrieNode();}
  ~Trie() { delete root; }

  void insert(std::string word) {
    TrieNode* node = root;
    for (char c : word) {
      if (node->children.find(c) == node->children.end()) {
        node->children[c] = new TrieNode();
      }
      node = node->children[c];
    }
    node->endofWord = true;
  }

  std::vector<std::string> get_completions(std::string prefix) {
    TrieNode* node = root;
    for (char c : prefix) {
      if (node->children.find(c) == node->children.end()) {
        return {}; // No matches
      }
      node = node->children[c];
    }
    std::vector<std::string> results;
    collectAllWords(node, prefix, results);
    return results;
  }

  std::string getLongestCommonPrefix(std::string prefix) {
    TrieNode* node = root;

    // go to the end of prefix
    for (char c : prefix) {
      if (node->children.find(c) == node->children.end()) {
        return prefix; // No matches at all
      }
      node = node->children[c];
    }

    // keep going if there is only one child + not end of word
    std::string lcp = prefix;
    while (node->children.size() == 1 && !node->endofWord) {
      auto it = node->children.begin();
      lcp += it->first;
      node = it->second;
    }

    return lcp;
  }
};


class Shell {
public:
  Shell() : curDir(std::filesystem::current_path()), running(true) {
    // built ins
    commands["exit"] = [this](auto args){running = false;};
    commands["pwd"] = [this](auto args) {std::cout << curDir.string() << std::endl;};
    commands["echo"] = [this](auto args) { handle_echo(args); };
    commands["cd"] = [this](auto args) { handle_cd(args); };
    commands["type"] = [this](auto args) { handle_type(args); };

    // add the commands to the Trie
    add_command_to_Trie(command_trie);
  }

  void run() {
    std::cout << std::unitbuf;
    int tab_counter = 0; // for consecutive tab presses

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
        } else { // normal char
          input += c;
          std::cout << c << std::flush;
          tab_counter = 0;
        }
      }

      setRawMode(false);

      if (input.empty()) continue;

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
  Trie command_trie;

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

};

int main() {
  //  -- supported --
  // exit : exit Shell
  // echo : print out args
  // type : type of file/command/etc
  // pwd : prints working directory
  // cd : change directory
  // parsing single and double quotes + \ + ~ (HOME)
  // redirecting 1> > 2> 1>> >>
  // autocompletion
  // + all commands specified in PATH

  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  Shell myShell{};
  myShell.run();
  return 0;
}
