# Dominator-Hauken local IPC protocol

Dominator listens for one local Hauken connection using the Qt local-socket
server name `Dominator-Hauken-v1`. Hauken connects with `QLocalSocket`; no IP
address, port, or MQTT broker is needed.

Messages are UTF-8 JSON objects, one per line, terminated by `\n`. Receivers
must buffer incoming bytes until a complete line is available; a socket read
is not guaranteed to contain exactly one complete message. Messages larger
than 64 KiB are rejected.

## Hauken to Dominator

Send every signal reading as it arrives:

```json
{"type":"signal_level","level_dBuV":42.5,"timestamp":"2026-11-23T20:10:00.000Z"}
```

`timestamp` is optional and, when provided, must be ISO 8601. Dominator uses
the current UTC time if it is omitted.

Start or stop the scan configured in Dominator:

```json
{"type":"scan_command","command":"start","request_id":"unique-id"}
{"type":"scan_command","command":"stop","request_id":"unique-id"}
```

Dominator replies with a `command_result`, including the request ID, whether
the command was accepted, and the current scan state. The logical scan state
is also sent whenever it changes and immediately after Hauken connects:

```json
{"type":"command_result","request_id":"unique-id","command":"start","accepted":true,"state":"scanning"}
{"type":"scan_state","state":"scanning","timestamp":"2026-11-23T20:10:00.000Z"}
```

States are `stopped`, `scanning`, and `paused`. A lost Hauken connection
changes an active scan to `paused`; reconnecting does not resume it. Local
Dominator Start/Stop controls remain available while Hauken is disconnected.

## Dominator to Hauken

When a dish position has been supplied by the dish-control layer, Dominator
sends the latest position once per second while connected. The timestamp is
the time the position was last updated:

```json
{"type":"position","azimuth_deg":180.0,"elevation_deg":25.0,"reference":"true_north","timestamp":"2026-11-23T20:10:00.000Z"}
```

The current project does not yet have a dish protocol implementation, so it
does not produce positions or physically move the dish. `ScanController`
exposes start, stop, and pause signals for that future hardware adapter.
Command acceptance currently reflects the Dominator scan state, not a
hardware acknowledgement.
