# Inkwell Loader Core - Advanced Process & Kernel Bypass Suite

Hardware-accelerated Direct3D 9 / ImGui desktop loader written in C++20 for Windows x64 and Win32. Provides automated process discovery, user-mode NTAPI PEB module unlinking, and kernel-mode two-stage BYOVD driver orchestration.

---

## Architecture Overview

- **Core Engine (`core/`)**:
  - `driver.cpp / driver.h`: Two-stage BYOVD kernel driver staging engine and anti-cheat profiles (EAC, BE, Vanguard, VAC).
  - `injector.cpp / injector.h`: Low-level NTAPI remote virtual memory allocation, manual mapping, and PEB `InLoadOrderModuleList` unlinking / cloaking.
  - `logger.cpp / logger.h`: Thread-safe diagnostics ring-buffer logging.
  - `config.cpp / config.h`: Profile and preference serialization via INI.
  - `kdmapper/`: Intel vulnerable driver exploitation primitive for arbitrary kernel physical memory mapping.
- **GUI Subsystem (`gui/` & `imgui/`)**:
  - `ui.cpp / ui.h`: Interactive multi-tab ImGui interface (Target Selection, Kernel Driver Hub, Injector Console, Settings).
  - `theme.cpp / theme.h`: High-contrast tactical themes (Inkwell Amethyst, Electric Indigo, Cyber Emerald, Crimson Blood, Custom RGBA).
- **Application Core**:
  - `main.cpp / main.h`: Win32 application lifecycle, Direct3D 9 device creation, and message loop.

---

## Build Instructions

### Prerequisites
- Windows 10 / 11 (x64)
- Visual Studio 2022 / v18 with "Desktop development with C++" workload
- DirectX SDK / Direct3D 9 headers (included with modern Windows SDK)

### Compilation
Double-click `build.bat` or run:
```cmd
build.bat
```
The output binary will be generated at `bin\Release\InkwellLoader.exe`.
