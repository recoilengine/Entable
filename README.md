# Entable

A header-only C++20 compile-time Structure of Arrays (SoA) library with versioned entities and high-performance data structures.

## Overview

Entable is **not** a traditional ECS (Entity-Component-System) library. Instead, it provides:

- **Compile-time SoA (Structure of Arrays)**: Components are defined at compile-time; you cannot add/remove components at runtime
- **Versioned Entities**: Each entity has a version that increments when it's destroyed, allowing safe handling of stale references
- **Rows of Components**: Defined at compile-time with columns following specific entities
- **ChunkedArray**: A cache-friendly, chunked array data structure for efficient component storage

### Key Differences from ECS

| Feature | Traditional ECS | Entable |
|---------|-----------------|---------|
| Component addition | Runtime | Compile-time |
| Component removal | Runtime | Compile-time |
| Entity versioning | Optional | Built-in |
| Data layout | SoA or AoS | SoA (compile-time) |

## Features

- **Header-only**: No build required, just include and use
- **C++20**: Modern C++ with concepts, templates, and ranges
- **Zero-cost abstractions**: Designed for performance
- **Cache-friendly**: Chunked storage for better memory access patterns
- **Type-safe**: Full compile-time type checking
- **Versioned entities**: Safe handling of entity lifecycle

## Requirements

- C++20 compatible compiler (GCC, Clang, MSVC)
- CMake 3.14+
- Google Benchmark (fetched automatically via FetchContent)
- Catch2 for testing (fetched automatically via FetchContent)

## Building

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install cmake g++ clang

# Windows
# Install Visual Studio 2022 with C++ support
```

### Build

```bash
# Clone the repository
git clone https://github.com/RecoilEngine/Entable.git
cd Entable

# Create build directory
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build everything
cmake --build build

# Build only tests
cmake --build build --target chunked_array_tests entable_tests

# Build only benchmarks
cmake --build build --target chunked_array_benchmarks entable_benchmarks
```

### Running Tests

```bash
ctest --output-on-failure -C Release
```

### Running Benchmarks

```bash
# Run chunked array benchmarks
./build/chunked_array_benchmarks

# Run SoA vs AoS benchmarks
./build/entable_benchmarks
```

## Project Structure

```
Entable/
├── Entable.hpp           # Main SoA registry header
├── ChunkedArray.hpp      # Chunked array data structure
├── CMakeLists.txt        # CMake build configuration
├── benchmarks/
│   ├── vector_benchmarks.cpp      # ChunkedArray vs std::vector
│   └── soa_aos_benchmarks.cpp     # SoA vs AoS comparison
├── tests/
│   ├── ChunkedArray_tests.cpp     # ChunkedArray unit tests
│   └── Entable_tests.cpp          # Registry unit tests
└── .github/
    └── workflows/
        └── ci.yml         # GitHub Actions CI
```

## CI/CD

The project uses GitHub Actions for continuous integration:

- **Master branch**: Builds and tests with g++ on Ubuntu
- **Pull requests**: Builds and tests with g++, Clang, and MSVC
- All builds verify tests pass (benchmarks are built but not run as part of CI)

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Acknowledgments

This project is **mostly LLM coded, but with human supervision**. The core design and architecture were guided by human expertise, while the implementation was largely generated using large language models with iterative human review and refinement.

## Performance

Benchmarks compare:
- `ChunkedArray` vs `std::vector` for typical use cases
- SoA (Structure of Arrays) vs AoS (Array of Structures) data layouts

Run the benchmarks to see performance results on your hardware.
