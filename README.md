# ShellCpp
Implementation of a POSIX compliant shell in C++

### Supported Shell Features

| Feature | Category | Implementation Details |
| :--- | :--- | :--- |
| **Interactive Prompt** | UX | Displays a `$ ` prompt and waits for user input. |
| **Built-in: `exit`** | Commands | Cleanly breaks the execution loop to close the shell. |
| **Built-in: `echo`** | Commands | Prints the provided arguments to standard output. |
| **Built-in: `type`** | Commands | Identifies if a command is a **builtin** or an **executable** in the `PATH`. |
| **PATH Resolution** | File System | Iterates through directories in the `PATH` environment variable using `std::filesystem`. |
| **Execute Permissions** | Security | Uses `access(path, X_OK)` to verify a file is runnable before attempting execution. |
| **External Execution** | Process Mgmt | Uses `fork()` and `execv()` to run programs found in the `PATH`. |
| **Output Buffering** | I/O | Uses `std::unitbuf` to disable buffering for immediate `std::cout` feedback. |
