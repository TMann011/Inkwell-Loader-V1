# Inkwell Loader Core — Technical Developer Reference & Workspace Architecture Notes

## 1. Executive Summary & Loader Architecture

The **Inkwell Loader Core** is a hardware-accelerated Direct3D 9 / ImGui desktop application written in C++20 for x64 and Win32 platforms. It provides process discovery, user-mode NTAPI PEB module unlinking, and kernel-mode two-stage BYOVD driver staging across target games and internal software modules.

```text
my loader/
├── DEV_NOTES.md                    # Main Architectural Reference Documentation
├── InkwellLoader.sln               # Visual Studio 2022/2026 Solution
├── InkwellLoader.vcxproj           # MSBuild Project Definition
├── inkwell_config.ini              # Loader Configuration & State Persistence File
    ├── main.h / main.cpp           # Win32 WinMain entry, WndProc & D3D9 device management
    ├── core/                       # Core Systems & Injection Primitives
    │   ├── config.h / config.cpp   # INI Manager for Auth & User Preferences
    │   ├── driver.h / driver.cpp   # Two-Stage BYOVD Kernel Driver Loader & Anti-Cheat Profiles
    │   ├── injector.h / injector.cpp # NTAPI Remote Allocation & PEB List Cloaking Engine
    │   └── logger.h / logger.cpp   # Multi-level Thread-Safe Ring-Buffer Diagnostics Logger
    ├── gui/                        # User Interface Subsystem
    │   ├── theme.h / theme.cpp     # Purple & Black Default Theme, Presets & Accessibility Customizer
    │   └── ui.h / ui.cpp           # Frameless Custom Draggable Window & Tab Rendering Engine
    └── imgui/                      # Self-Contained Dear ImGui Core & Backends
```

---

## 2. Low-Level Subsystem Mechanics

### A. User-Mode NTAPI Remote PEB Module Unlinking (`core/injector.cpp`)
Standard process module enumeration (e.g. `CreateToolhelp32Snapshot` / `Module32NextW`) parses doubly linked lists inside the Process Environment Block (`PEB->Ldr`). The Inkwell loader unlinks injected DLL modules directly from remote process memory:

1. Resolves `NtQueryInformationProcess` from `ntdll.dll` to acquire `PEB` base address.
2. Dereferences `PEB->Ldr` pointer (compensating for pointer width: `+0x18` on x64, `+0x0C` on x86).
3. Traverses `InLoadOrderModuleList` and `InMemoryOrderModuleList` to find target module matching base address `hModule`.
4. Atomically modifies remote node links:
   ```cpp
   Blink->Flink = Flink;
   Flink->Blink = Blink;
   ```
5. Result: The injected DLL operates normally in process memory but becomes invisible to Toolhelp32/PSAPI module scans.

### B. Two-Stage BYOVD Kernel Driver Subsystem (`core/driver.cpp`)
Based on captured kernel artifacts in `chs/driver prepared` and `chs/a notes for driver`:

1. **Stage 1 (BYOVD Staging Vector)**:
   - File: `iqvw64e.sys` (Intel Corporation Authenticode Signed).
   - Loaded via Service Control Manager (`CreateServiceW` / `StartServiceW`).
   - Exposes device handle `\Device\Nal` with physical memory control primitives (`MmMapIoSpace`, `MmGetPhysicalAddress`, `ExAllocatePoolWithTag`).

2. **Stage 2 (In-Memory Kernel Payload Mapping)**:
   - File: `stage2_payload_1.5MB.sys`.
   - Allocates non-paged kernel pool memory (`0xFFFF...`).
   - Maps raw payload sections, resolves `ntoskrnl.exe` imports, and executes `DriverEntry(0xFFFF...)`.

3. **Stage 1 Cleanup**:
   - Immediately stops and unloads `iqvw64e.sys` (`ControlService` / `DeleteService`).
   - Leaves Stage 2 payload filelessly executing in un-backed kernel memory without a corresponding entry in `PsLoadedModuleList`.

---

## 3. Targeted Anti-Cheat Category Profiles

| Profile | Category | Targeted Services | Primary Strategy |
| :--- | :--- | :--- | :--- |
| **EAC** | Easy Anti-Cheat | `EasyAntiCheat.exe`, `EasyAntiCheat_EOS.sys` | Stage 1 BYOVD physical memory staging, EOS service handle cloaking, and dynamic PEB list unlinking. |
| **BE** | BattEye | `BEService.exe`, `BEDaisy.sys` | Pre-injection `BEService.exe` process termination, suspended process creation, PEB list unlinking. |
| **ACE** | Anti-Cheat Expert | `ACE-BASE.sys`, `AntiCheatExpert.exe` | Non-paged kernel pool payload mapping (`0xFFFF...`) and physical memory IOCTL address translation. |
| **OTHER** | Custom Protection | Generic Target Services | Fully configurable BYOVD kernel driver loading and user-mode remote thread injection. |

---

## 4. Ecosystem & Target Integration Analysis

Comprehensive survey of existing modules across the workspace:

### 1. `chs/bodycam`
- **Project**: `Inkwell_Bodycam` (Unreal Engine 5 Internal Hook).
- **Architecture**: DirectX 11/12 Kiero Hook with ImGui interface.
- **Key Modules**: Aimbot ballistics solver, 3D Skeletal ESP, Dynamic Material Chams (`PlasticWhite`, `Ironman`, `Glass Tumblers`), FOV circle, and memory telemetry.
- **Target Executable**: `Bodycam-Win64-Shipping.exe`.

### 2. `chs/dlls/Marvel Rivals`
- **Project**: `Inkwell Marvel` (Unreal Engine 5 Internal Hook).
- **Target Executable**: `Marvel-Win64-Shipping.exe`.
- **Outputs**: `inkwell Marvel.dll` / `inkwell.dll` (1,985,024 bytes).

### 3. `chs/4h`
- **Project**: Inkwell For Honor (AnvilNext Engine Script & Overlay).
- **Key Components**: DXGI Screen Capture, Zone Recognition, Millisecond Parry Trigger, Auto Guard Break Counter, and Timed Action Accelerator.
- **Target Executable**: `forhonor.exe`.

### 4. `chs/rematch`
- **Project**: Rematch Internal & Staging Vectors (`Rematch-Win64-Shipping.exe`).
- **Key Vectors**:
  - `vectors/manual_map/manual_map.cpp`: User-mode PE manual mapper handling relocations, TLS callbacks, and security cookies.
  - `vectors/proxy/version_hardened.cpp`: Export proxying for `version.dll` system wrapper.
  - `vectors/thread_hijack/thread_hijack.cpp`: Thread context hijacking (`GetThreadContext` / `SetThreadContext`) for shellcode execution.

### 5. `chs/anti`
- **Project**: Anti-Cheat Analysis & EAC Emulator (`EOSEmulator.sln`).
- **Key Components**: Synthetic Epic Online Services (EOS) SDK emulation layer for local offline testing.

---

## 5. UI Styling & Accessibility Design System

Standardized in `gui/theme.h` & `gui/theme.cpp`:

### Color Palette Tokens
- **Accent (Purple & Black Default)**: `#9447F2` (`ImVec4(0.58f, 0.28f, 0.95f, 1.00f)`)
- **Background Canvas**: Pitch Black (`ImVec4(0.04f, 0.03f, 0.06f, 0.96f)`)
- **Card Containers**: Dark Violet Glass (`ImVec4(0.08f, 0.06f, 0.12f, 0.85f)`)
- **Borders**: Muted Violet (`ImVec4(0.24f, 0.15f, 0.38f, 0.55f)`)

### Theme Presets & Accessibility
The loader features interactive theme switching in **`05 // SETTINGS`**:
1. `0 // Inkwell Purple & Black (Default)`
2. `1 // Electric Indigo (Classic)`
3. `2 // Cyber Emerald (High Contrast)`
4. `3 // Crimson Blood`
5. `4 // Custom RGBA Palette` (With individual color pickers for Accent, Background, Panel, and Text)
6. **High Contrast Mode**: High-visibility monochrome mode for accessibility.

---

## 6. Build & Deployment Commands

```powershell
# Build x64 Release Binary
MSBuild.exe InkwellLoader.sln /p:Configuration=Release /p:Platform=x64

# Build Win32 Release Binary
MSBuild.exe InkwellLoader.sln /p:Configuration=Release /p:Platform=Win32
```
