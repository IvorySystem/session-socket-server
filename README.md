# Ivory Socket Session Server

`isss` is the session server used by the IvoryScript console and related tools. It provides interactive script execution with streamed output for a persistent session-style environment.  The server is intended to act as a proxy between client sockets and multiple worker processes running `iss`.  It is currently used by the online IvoryScript console to execute submitted scripts and stream output progressively back to the browser.

## Repository Layout

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
├── scripts/
├── systemd/
│   ├── isss.service
│   └── README.md
└── docs/
```

The repository contains both the `isss` application code and the small amount of shared support code required to build it.

## Build Assumptions

The existing build system expects the retained source hierarchy and Makefile structure. Only the source subtrees required by `isss` have been included.

## Status

This repository contains the standalone session server component currently used to support the online IvoryScript console.

## Related

* IvoryScript language and tools
* Online console at `ivoryscript.net/console`
* Application examples at `ivoryscript.net/documents/applications`

