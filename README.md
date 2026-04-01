# Ivory Session Socket Server

`isss` is the session socket server used by the IvoryScript console and related tools. It provides interactive script execution with streamed output for a persistent session-style environment.  The server is intended to act as a proxy between client sockets and multiple worker processes running `iss`.  It is currently used by the online IvoryScript console to execute submitted scripts and stream output progressively back to the browser.

## Repository layout

The originating source hierarchy has been retained to preserve compatibility with the existing build system.

```text
ivory-session-server/
├── README.md
├── LICENSE
├── Makefile
├── source/
│   ├── app/
│   │   └── ivory/
│   │       └── isss/
│   └── API/
│       └── gen/
├── systemd/
│   ├── isss.service
│   └── README.md
└── docs/
```

The repository contains both the `isss` application code and the small amount of shared support code required to build it.

---

## Build assumptions

The existing build system expects the retained source hierarchy and Makefile structure. Only the source subtrees required by `isss` have been included.

## Build instructions

The server is optimized for a 32-bit memory model to maintain a lean pointer footprint and maximize CPU cache performance. It utilizes Linux-specific features like `signalfd(2)` and `timerfd(2)` for robust, asynchronous event handling.

### Prerequisites
*   **OS**: Linux (Kernel 2.6.27+ required).
*   **Compiler**: `g++` with C++11 support.
*   **Libraries**: If building on a 64-bit system, ensure `g++-multilib` is installed to support the `-m32` flag.

### Compilation
The included `Makefile` handles dependency tracking and architecture-specific flags.

1.  **Standard 32-bit build**:
    ```bash
    make ARCH=32
    ```
    *This generates the binary in your configured executable directory (e.g., `build/gcc/x86_64/app/ivory/`).*

2.  **Debug build**:
    To enable verbose tracing of the **SIGCHLD** reaper and socket events:
    ```bash
    make DEBUG=1
    ```

3.  **Cross-compiling (ARM/Raspberry Pi)**:
    ```bash
    make ARM_ELF=1
    ```
---

## Running the server

`isss` uses a flexible key-value options parser. Parameters can be provided in any order using the `key=value` format. If an option is omitted, the server uses its hard-coded defaults.

### Command Line Options

| Option | Default | Description |
| :--- | :--- | :--- |
| `port` | `4196` | The TCP port the server listens on. |
| `exec` | `/usr/local/bin/iss` | Absolute path to the IvoryScript (`iss`) binary. |
| `maxClients` | `16` | Maximum number of concurrent network connections. |
| `maxWorkers` | `16` | Maximum number of simultaneous worker sessions. |
| `workerTimeout` | `900` | Inactivity timeout in seconds (default: 15 mins). |

### Examples

**Standard Start (using defaults):**
```bash
isss

---

## Status

This repository contains the standalone session server component currently used to support the online IvoryScript console.

## Related

* IvoryScript language and tools
* Online console at `ivoryscript.net/console`
* Application examples at `ivoryscript.net/documents/applications`

