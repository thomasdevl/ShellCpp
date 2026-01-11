# ShellCpp
Implementation of a POSIX compliant shell in C++

### Supported Shell Features

| Feature | Category | Implementation Details |
| :--- | :--- | :--- |
| **Interactive Prompt** | UX | Displays a `$ ` prompt; uses `std::unitbuf` for real-time flushing. |
| **Built-in: `exit`** | Commands | Cleanly breaks the execution loop to close the shell. |
| **Built-in: `echo`** | Commands | Prints the provided arguments to standard output. |
| **Built-in: `type`** | Commands | Identifies if a command is a **builtin** or an **executable** in the `PATH`. |
| **Built-in: `pwd`** | Commands | Tracks and prints the current working directory using `std::filesystem::path`. |
| **Built-in: `cd`** | Commands | Supports absolute (`/`), home (`~`), parent (`../`), and relative path navigation. |
| **PATH Resolution** | File System | Iterates through `PATH`, filtering for regular files with execution permissions. |
| **Trie Data Structure** | Performance | Efficiently stores all system commands and built-ins for $O(L)$ lookup and completion. |
| **Tab Autocompletion** | UX | Supports **Double-Tab** logic: rings bell on 1st tab, lists alphabetical matches on 2nd. |
| **LCP Completion** | UX | Automatically completes the **Longest Common Prefix** if multiple commands share a stem. |
| **Raw Mode Handling** | Terminal | Uses `termios.h` to disable `ICANON` and `ECHO` for custom keystroke processing. |
| **Input Parsing** | Parsing | Robust state-machine parsing for single/double quotes and backslash escaping. |
| **Redirection** | I/O | Supports stdout/stderr redirection and appending (`>`, `>>`, `1>`, `1>>`, `2>`, `2>>`). |
| **External Execution** | Process Mgmt | Uses `fork()`, `execv()`, and `waitpid()` to execute external programs as child processes. |
