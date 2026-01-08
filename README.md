# ShellCpp
Implementation of a POSIX compliant shell in C++

### Supported Shell Features

| Feature | Category | Implementation Details |
| :--- | :--- | :--- |
| **Interactive Prompt** | UX | Displays a `$ ` prompt and waits for user input. |
| **Built-in: `exit`** | Commands | Cleanly breaks the execution loop to close the shell. |
| **Built-in: `echo`** | Commands | Prints the provided arguments to standard output. |
| **Built-in: `type`** | Commands | Identifies if a command is a **builtin** or an **executable** in the `PATH`. |
| **Built-in: `pwd`** | Commands | Tracks and prints the current working directory using `std::filesystem::path`. |
| **Built-in: `cd`** | Commands | Supports absolute (`/`), home (`~`), parent (`../`), and relative path navigation. |
| **PATH Resolution** | File System | Iterates through directories in the `PATH` environment variable using `std::filesystem`. |
| **External Execution** | Process Mgmt | Uses `fork()`, `execv()`, and `waitpid()` to execute external programs as child processes. |
