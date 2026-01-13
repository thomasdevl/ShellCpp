# ShellCpp
Implementation of a POSIX compliant shell in C++

### Supported Shell Features

| Feature | Category | Implementation Details |
| :--- | :--- | :--- |
| **Interactive Prompt** | UX | Displays a `$ ` prompt; uses `std::unitbuf` for real-time flushing. |
| **Built-in: `exit`** | Commands | Cleanly breaks the execution loop to close the shell. |
| **Built-in: `echo`** | Commands | Prints arguments to stdout (refined logic to prevent trailing spaces). |
| **Built-in: `type`** | Commands | Identifies if a command is a **builtin** or an **executable** in the `PATH`. |
| **Built-in: `pwd`** | Commands | Tracks and prints the current working directory using `std::filesystem`. |
| **Built-in: `cd`** | Commands | Supports absolute (`/`), home (`~`), parent (`../`), and relative navigation. |
| **Built-in: `history`** | Commands | Logic to manage the history vector. Supports `-a` (append), `-r` (read), and `-w` (write) to custom files. |
| **History Persistence** | Lifecycle | **Startup:** Automatically loads `HISTFILE` into memory. <br> **Exit:** Automatically writes memory back to `HISTFILE`. |
| **PATH Resolution** | File System | Iterates through `PATH`, filtering for executables with `access(X_OK)`. |
| **Trie Data Structure** | Performance | Efficiently stores commands for $O(L)$ lookup and prefix completion. |
| **Tab Autocompletion** | UX | Supports **Double-Tab**: rings bell on 1st tab, lists matches on 2nd. |
| **LCP Completion** | UX | Automatically completes the **Longest Common Prefix** for shared stems. |
| **Raw Mode Handling** | Terminal | Uses `termios.h` to disable `ICANON` and `ECHO` for raw input. |
| **Backspace Handling** | UX | Intercepts ASCII `127` and uses `\b \b` to visually erase characters. |
| **ANSI Escape Handling** | UX | Intercepts 3-byte sequences (`\x1b[...`) for non-ASCII keyboard input. |
| **History Navigation** | UX | Uses **Up/Down Arrows** to scroll through the `history` vector. |
| **Line Clearing** | Terminal | Employs ANSI `\33[2K` and `\r` to clear the line before redrawing history. |
| **Input Parsing** | Parsing | Robust state-machine for single/double quotes and backslash escaping. |
| **Redirection** | I/O | Supports stdout/stderr redirection and appending (`>`, `>>`, `2>`, etc.). |
| **Pipelines ('\|')** | Process Mgmt | Connects commands via `pipe()`, `fork()`, and `dup2()` for concurrency. |
| **Subshell Execution** | Process Mgmt | Executes built-ins within forked children when part of a pipeline. |
| **External Execution** | Process Mgmt | Uses `fork()`, `execv()`, and `waitpid()` for external binary execution. |
