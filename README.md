# C++ Project Setup Guide

This guide explains how to set up, build, and run a basic C++ project on macOS, Windows, and Linux.

---

## Prerequisites

Ensure you have a C++ compiler installed:

* **macOS**: `clang` (via Xcode Command Line Tools)
* **Linux**: `g++` or `clang++`
* **Windows**: MinGW (GCC) or Microsoft Visual Studio (MSVC)

---

## 1. Install a C++ Compiler

### macOS

Install Xcode Command Line Tools:

```bash
xcode-select --install
```

Verify installation:

```bash
clang++ --version
```

---

### Linux (Ubuntu/Debian)

Update packages and install build tools:

```bash
sudo apt update
sudo apt install build-essential
```

Verify installation:

```bash
g++ --version
```

---

### Windows

#### Option 1: MinGW (GCC)

1. Download and install MinGW
2. Add `bin` directory to your system `PATH`
3. Verify installation:

```bash
g++ --version
```

#### Option 2: Visual Studio (MSVC)

1. Install Visual Studio
2. Select **Desktop development with C++** workload
3. Use the **Developer Command Prompt** to compile code

---

## 2. Project Structure

A simple C++ project may look like this:

```
project-root/
│
├── src/
│   └── main.cpp
├── include/
├── build/        (optional)
└── README.md
```

---

## 3. Example C++ Program

Create a file at `src/main.cpp`:

```cpp
#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
```

---

## 4. Build the Project

Navigate to the project root:

```bash
cd /path/to/project-root
```

### macOS / Linux

```bash
g++ -std=c++11 -o main src/main.cpp
```

---

### Windows (MinGW)

```bash
g++ -std=c++11 -o main.exe src/main.cpp
```

---

### Windows (MSVC)

```bash
cl src\main.cpp
```

---

## 5. Run the Program

### macOS / Linux

```bash
./main
```

---

### Windows

```bash
main.exe
```

---

## 6. Cleaning Build Files

Remove compiled binaries:

```bash
rm -f main main.exe
```

---

## 7. Notes

* Use `-std=c++11` or higher (e.g., `c++17`, `c++20`) depending on your needs
* Keep source files in `src/` and headers in `include/` for organization
* For larger projects, consider using a build system like **CMake**

---

## 8. Running Tests

```c++
g++ tests/unit/math_test.cpp -o test && ./test
```

## 9. License

This project is licensed under the MIT License. See the `LICENSE` file for details.
