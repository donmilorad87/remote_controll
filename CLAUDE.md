# RemoteControl

Direct LAN remote control. A mobile phone controls a desktop PC over the local network — no server needed.

## Architecture

```
┌──────────────┐      UDP (direct LAN)      ┌──────────────────┐
│  Flutter App  │ ◄────────────────────────► │  Desktop App      │
│  (Android/iOS)│                             │  (Win/Lin/Mac)    │
│  Enter code   │    Same WiFi / LAN         │  Shows 6-digit    │
│  or scan QR   │                             │  code + QR        │
└──────────────┘                             └──────────────────┘
```

## Pairing Flow

1. Desktop starts UDP listener on port `7482` + discovery listener on `7483`
2. Desktop generates random 6-digit code and detects its LAN IP
3. Desktop shows a window with the **code** and a **QR code** (`rc://IP:PORT?code=XXXXXX`)
4. Mobile either **scans QR** or **enters code manually**
5. If code entered manually: mobile broadcasts UDP discovery on port `7483`, desktop replies
6. Mobile sends pairing request with code to desktop, desktop verifies and accepts
7. Mobile sends control events directly to desktop over UDP
8. Desktop executes them (cursor, mouse, keyboard, volume)

## Project Structure

```
remote/
├── desktop/
│   ├── shared/           # Cross-platform headers + QR code generator
│   │   ├── rc_protocol.h # Binary event protocol
│   │   ├── rc_desktop.h  # Desktop server types
│   │   ├── qrcodegen.c/h # QR code generation (nayuki)
│   ├── linux/            # Linux: X11/XTest + PulseAudio + GTK
│   ├── windows/          # Windows: Win32 + WASAPI
│   └── mac/              # macOS: CGEvent + CoreAudio + Cocoa
├── mobile/               # Flutter app (Android + iOS + Linux)
└── docs/                 # Build instructions
```

## Ports

| Port | Purpose |
|------|---------|
| 7482 | UDP event communication (desktop listens) |
| 7483 | UDP broadcast discovery (desktop listens) |

## Binary Protocol

```
[1 byte: event_type] [variable: payload]
```

| Event | Type | Payload |
|-------|------|---------|
| Cursor Move | 0x01 | direction(1) + speed(1) |
| Mouse Click | 0x02 | button(1) |
| Mouse Scroll | 0x03 | direction(1) |
| Key Press | 0x04 | keycode(2) + modifiers(1) |
| Key Release | 0x05 | keycode(2) + modifiers(1) |
| Volume | 0x06 | action(1) |
| Pair Request | 0xFD | code(6) |
| Pair Accept | 0xFC | (none) |
| Pair Reject | 0xFB | (none) |
| Discover | 0xFA | code(6) |
| Discover Reply | 0xF9 | port(2) + code(6) |
| Heartbeat | 0xFF | (none) |

## Coding Standards

- C code follows NASA "Power of 10" rules
- Functions ≤ 60 lines, ≥ 2 assertions per function
- No recursion, no goto, bounded loops
- Compile with `-Wall -Wextra -Werror`
