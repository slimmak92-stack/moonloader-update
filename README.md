# OpenMoonLoader Turbo v2.0 (Fast-Load & Optimized C++20 Runtime)

A high-speed, zero-polling, modular C++20 replacement foundation for GTA San Andreas / SA-MP `MoonLoader.asi`.

## Performance & Architecture Upgrades

1. **Zero-Delay Startup (`StartupDelayMs=0`):** Removes artificial boot sleeps and measures exact startup latency in microseconds (`GetTelemetry()`).
2. **Parallel Bytecode Pre-Warming & FNV-1a `.luacache` (`FastBytecodeCache=true`):** Compiles `.lua` scripts across worker threads (`std::async`) and caches LuaJIT bytecode keyed by 64-bit FNV-1a content hashes in `moonloader/.cache/` so unchanged scripts skip parsing on subsequent launches.
3. **Zero-Polling Native File Watcher (`src/core/file_watcher.*`):** Replaces periodic directory scans with Windows `ReadDirectoryChangesW` overlapped I/O—consuming 0% CPU while idle and reloading only the exact modified script milliseconds after you hit save.
4. **Shared-Lock Zero-Allocation Event Bus (`src/core/event_bus.*`):** Uses `std::shared_mutex` so high-frequency `runtime.tick` dispatches take a non-blocking shared read lock without copying listener vectors.
5. **Aggressive MSVC Release Optimization (`CMakeLists.txt`):** Compiles with `/O2 /Ob2 /Oi /Ot /Oy /GT /Gy /Gw /GF /arch:SSE2 /fp:fast /GL /LTCG /OPT:REF /OPT:ICF=10`.

## Build (Win32 Release)

```bat
cmake -B build -A Win32
cmake --build build --config Release
```
