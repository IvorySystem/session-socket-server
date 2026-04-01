# isss Protocol

## Overview

`isss` provides a simple line-oriented TCP socket protocol for creating, reconnecting to and interacting with persistent Ivory System and IvoryScript sessions.

The protocol is intended to support long-lived interactive sessions rather than simple one-off command execution. In particular, it allows one client connection to submit input to a session while another connection remains responsible for receiving and relaying output.

This is the pattern used naturally by a web front end in which:

* a short-lived HTTP request submits script text;
* a separate SSE connection relays session output progressively to the browser.

All protocol commands and responses are plain UTF-8 text terminated by `\n`.

## Session Model

Each session corresponds to a worker process created by the server. A session is identified by a numeric session id.

A client connection may:

* create a new session;
* reconnect to an existing session;
* send script text or console input;
* receive output from the interpreter;
* check whether the session is ready for further input;
* terminate the session.

A session may outlive any individual client socket connection. This makes it possible for multiple client connections to interact with the same session over time.

## Typical Web Usage

A typical browser-facing arrangement uses two different connections for the same session:

1. an SSE relay connection, attached to the session and used to receive output;
2. a short-lived request connection, attached to the same session and used only to submit input.

The sending connection typically performs a sequence such as:

```text
SESSION: 12345678
RECEIVE: OFF
SEND: <script text>
```

The SSE relay remains attached separately and forwards received output to the browser.

This split allows input submission and output streaming to be handled independently while preserving a single persistent interpreter session.

## Session Lifecycle

A typical sequence is:

1. Connect to the server.
2. Send `START SESSION`.
3. Receive `STARTED: nnnnnnnn`.
4. Establish or retain an output-reading connection for that session.
5. Submit script text using another connection if required.
6. Receive output messages.
7. Receive `READY` when the interpreter prompt is reached.
8. Send additional input or reconnect later using the session id.
9. Send `END` to terminate the session.

## Commands

### START SESSION

Creates a new session and launches a new interpreter process.

Request:

```text
START SESSION
```

Response:

```text
STARTED: 12345678
```

The returned session identifier is an eight digit decimal value.

### SESSION

Associates the current client connection with an existing session.

Request:

```text
SESSION: 12345678
```

If the session exists, the connection becomes associated with that session.

If the session id is unknown:

```text
ERROR: UNKNOWN SESSION ID
```

### SEND

Introduces script text or console input to be sent to the session.

Request:

```text
SEND: <command text>
```

or:

```text
SEND:
<command text>
```

Example:

```text
SEND:
2 + 2
```

When all pending input for that send operation has been forwarded to the interpreter process, the server responds:

```text
SENT
```

The command text may span multiple lines.

### RECEIVE

Controls whether the current client connection is an active receiver for session output.

Request:

```text
RECEIVE
```

Some client code may use an explicit form such as:

```text
RECEIVE: OFF
```

to indicate that the current connection should not receive output.

This does not terminate the session. It is useful when one connection is being used only to submit input while another connection is responsible for relaying output.

### STATUS

Checks whether the session is ready for further input.

Request:

```text
STATUS
```

Response:

```text
READY
```

`READY` is returned when the underlying interpreter prompt has been reached.

### END

Terminates the session.

Request:

```text
END
```

Response:

```text
ENDED
```

If the session is terminated automatically because of inactivity:

```text
TIMEOUT
```

## Output Messages

Session output is returned incrementally.

Standard output from the interpreter process is returned in fragments of the form:

```text
RECEIVED <n>: <data>
```

where `<n>` is the number of bytes included in that fragment.

Example:

```text
RECEIVED 5: hello
```

The server also monitors the interpreter prompt. When a prompt beginning with `> ` is detected, the server responds:

```text
READY
```

In a web deployment, these output fragments are typically consumed by the SSE relay connection rather than by the short-lived sending request.

## Errors

General protocol errors are returned in the form:

```text
ERROR: <message>
```

Examples:

```text
ERROR: UNKNOWN SESSION ID
ERROR: FAILED
```

`ERROR: FAILED` indicates that the underlying interpreter process exited unexpectedly.

## Notes

* Commands are case-sensitive.
* All commands are newline-terminated.
* The maximum command length is 8192 bytes.
* Sessions are persistent across TCP connections until explicitly ended or timed out.
* Session inactivity is monitored by the server.
* Multiple clients may attach to the same session id over time.
* One connection may be used primarily for sending while another acts as the active output receiver.
* Output may arrive in fragments and should be assembled by the client if required.

## Example Exchange

```text
Client A: START SESSION
Server:   STARTED: 12345678

Client B: SESSION: 12345678
Client B: RECEIVE
... Client B remains connected to relay output ...

Client A: SESSION: 12345678
Client A: RECEIVE: OFF
Client A: SEND: 2 + 2
Server:   SENT

Server to Client B: RECEIVED 2: 4
Server to Client B: READY

Client A: END
Server:   ENDED
```
