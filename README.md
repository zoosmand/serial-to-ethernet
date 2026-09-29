# STM32F407 Bidirectional Serial-Ethernet Controller

Firmware for a network-managed serial gateway based on the STM32F407VET6.
The controller exposes an authenticated HTTPS API, transmits API commands on
a selected RS232 or RS485 interface, and persists received serial data in the
onboard W25Q64 NOR Flash until an API client consumes it.

The project reuses the proven platform layer from
[stmf4-health-check](https://github.com/zoosmand/stmf4-health-check): CMSIS
peripheral setup, FreeRTOS, lwIP/Ethernet, Mbed TLS 3.6, authentication,
runtime server credentials, and the W25Q64 driver. Health-check, callback,
temperature, trust-store, and buzzer application services are not part of this
firmware.

## Initial functional contract

- Serve the management API over HTTPS/TLS 1.3 on TCP port 443.
- Require bearer authentication for every serial and buffer operation.
- Support independently configured RS232 and RS485 ports.
- Send arbitrary binary payloads to one explicitly selected port.
- Receive serial data continuously, independently of API activity.
- Append received bytes to a power-loss-tolerant ring buffer in W25Q64 Flash.
- Preserve byte ordering and record the source port for every stored chunk.
- Consume buffered records through an explicit API operation.
- Report overflow, framing, parity, overrun, Flash, and truncation errors.
- Never silently overwrite unread data; reject new data and expose an overflow
  counter when the persistent buffer is full.

## Proposed API v1

Binary serial data is represented as Base64 in JSON. This avoids treating
arbitrary device protocols as UTF-8 and keeps request parsing unambiguous.

| Method | Path | Purpose |
|---|---|---|
| `POST` | `/api/v1/auth/token` | Exchange credentials for tokens. |
| `POST` | `/api/v1/auth/refresh` | Rotate access and refresh tokens. |
| `POST` | `/api/v1/auth/revoke` | Revoke the current session. |
| `GET` | `/api/v1/serial/ports` | Read port configuration and status. |
| `PUT` | `/api/v1/serial/ports/{port}` | Configure baud rate, data bits, parity, and stop bits. |
| `POST` | `/api/v1/serial/ports/{port}/tx` | Transmit a Base64-encoded payload. |
| `GET` | `/api/v1/serial/buffer/status` | Read buffer usage and error counters. |
| `POST` | `/api/v1/serial/buffer/consume` | Return and remove the oldest bounded batch. |
| `DELETE` | `/api/v1/serial/buffer` | Administratively erase all unread data. |
| `PUT` | `/api/v1/tls/certificate` | Replace the HTTPS server certificate. |
| `PUT` | `/api/v1/tls/private-key` | Replace the HTTPS server private key. |

Example transmission request:

```json
{"data":"AQIDBA=="}
```

Example consume response:

```json
{
  "records": [
    {"sequence": 42, "port": "rs485", "timestamp_ms": 123456, "data": "AQIDBA=="}
  ],
  "remaining_records": 7,
  "remaining_bytes": 381
}
```

`POST /api/v1/serial/buffer/consume` is used instead of a destructive `GET` so
that HTTP caches, link checkers, and automatic retries cannot accidentally
empty the buffer. A batch is retired only after its complete HTTPS response
has been written successfully. This provides at-least-once delivery across a
disconnect: a client must tolerate the last batch being repeated.

## Persistent-buffer design

The W25Q64 buffer uses erase-sector-aligned metadata and data regions. Two
generation-numbered metadata copies hold the read cursor, write cursor, next
sequence number, and counters. Records contain a magic value, format version,
source port, payload length, sequence, timestamp, and CRC32. A record becomes
visible only after its commit marker is programmed. Startup scans forward from
the newest valid metadata copy to recover a record committed immediately
before power loss.

Flash writes are owned by one service task. UART receive paths feed bounded
RAM queues and never erase Flash in an interrupt. Backpressure is observable:
when RAM or Flash has no capacity, input is counted as dropped rather than
overwriting unread records.

## Hardware assumptions requiring confirmation

The source board already maps RS485 to USART2 (`PD5` TX, `PD6` RX, `PD7`
driver direction) and has an 8 MiB W25Q64 on SPI2. The RS232 USART, GPIO pins,
transceiver enable signals, and whether RS232 and RS485 are separate physical
ports still need to be defined before peripheral code can be finalized.

---

&copy; 2017-2026 Askug Ltd., Dmitry Slobodchikov
