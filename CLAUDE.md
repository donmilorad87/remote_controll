# RemoteControl

Cross-platform remote control system. A mobile phone (Android/iOS) controls a desktop PC (Windows/Linux/macOS) via a central C UDP/DTLS socket server.

## Architecture Overview

```
┌─────────────┐     UDP/DTLS      ┌──────────────┐     UDP/DTLS      ┌─────────────────┐
│  Flutter     │ ◄──────────────► │  C Socket    │ ◄──────────────► │  Desktop App     │
│  Mobile App  │                   │  Server      │                   │  (Win/Lin/Mac)   │
│  (iOS/Droid) │     HTTP/REST     │  (Monolith)  │                   │  Native C        │
│              │ ──────────────► │              │                   │                   │
└─────────────┘                   └──────┬───────┘                   └─────────────────┘
                                         │
                                   ┌─────┴─────┐
                                   │ PgBouncer  │
                                   └─────┬─────┘
                                   ┌─────┴─────┐
                                   │ PostgreSQL │
                                   └───────────┘
```

## Project Structure

```
remote/
├── backend/              # C server (HTTP API + UDP/DTLS socket + admin panel)
│   ├── src/              # C source files
│   ├── include/          # Header files
│   ├── migrations/       # SQL migration files (schema + stored procedures)
│   └── tests/            # C unit tests
├── mobile/               # Flutter app (Android + iOS)
├── desktop/
│   ├── windows/          # Win32 native C app
│   ├── mac/              # macOS CGEvent/CoreAudio native C app
│   └── linux/            # Linux X11/PulseAudio native C app
├── docker/               # Dockerfiles and configs per service
│   ├── c-server/
│   ├── postgresql/
│   ├── pgbouncer/
│   ├── nginx/
│   ├── pgadmin/
│   └── postfix/
├── docs/                 # Build and run documentation
├── docker-compose.yml
└── .env                  # Root environment config
```

## Technology Stack

| Component        | Technology                                        |
|------------------|---------------------------------------------------|
| Socket server    | C (vanilla), UDP/DTLS via OpenSSL                 |
| HTTP API         | C with libmicrohttpd                              |
| Database         | PostgreSQL 16 + stored procedures                 |
| Connection pool  | PgBouncer                                         |
| Reverse proxy    | Nginx                                             |
| Email            | Self-hosted Postfix                               |
| Desktop Windows  | C + Win32 API (SendInput, WASAPI, Shell_NotifyIcon)|
| Desktop macOS    | C + CGEvent API, CoreAudio, NSStatusItem          |
| Desktop Linux    | C + X11/XTest, PulseAudio, libappindicator        |
| Mobile           | Flutter + Provider                                |
| Admin panel      | Server-rendered HTML from C                       |

## Port Assignments (non-standard)

| Service          | Port  | Notes                            |
|------------------|-------|----------------------------------|
| Nginx HTTP       | 7480  | Main entry point                 |
| C Server HTTP    | 7481  | Auth API + admin panel           |
| C Server UDP     | 7482  | DTLS socket for device comms     |
| PostgreSQL       | 7483  | Database                         |
| PgBouncer        | 7484  | Connection pooler                |
| PgAdmin          | 7485  | Proxied at /pgadmin/             |
| Postfix SMTP     | 7486  | Internal email delivery          |

## Domain & Configuration

- Development domain: `local.remotecontrol.rs` (add to /etc/hosts)
- All apps use `.env` files for server address, ports, and config
- JWT secret shared between HTTP API and socket server (same monolith)

## Key Design Decisions

- **UDP + DTLS**: Chosen for lowest latency on input events. DTLS provides encryption without TCP overhead.
- **One desktop per account**: New desktop login kicks the existing one.
- **Auto-pairing**: Mobile and desktop auto-connect when logged into the same account.
- **Discrete joystick**: D-pad style (up/down/left/right), not analog.
- **Immediate key send**: Each mobile keystroke sent instantly to desktop.
- **Stored procedures**: All SQL lives in PostgreSQL procedures, called from C via libpq.
- **Server-rendered admin**: C server generates HTML directly, no JS framework.

## Coding Standards

- All C code follows NASA "Power of 10" rules (see global CLAUDE.md)
- Functions ≤ 60 lines, ≥ 2 assertions per function
- No recursion, no goto, bounded loops
- All return values checked, all inputs validated
- Compile with `-Wall -Wextra -Werror`
- No dynamic memory allocation after initialization (use pools/static buffers)

## Build & Run

```bash
# Start all Docker services
docker-compose up -d

# Desktop apps — see docs/desktop-build.md
# Mobile app — see docs/mobile-build.md
# Full setup — see docs/setup.md
```

## Event Protocol (UDP/DTLS)

Binary protocol over DTLS. Each packet:
```
[1 byte: event_type] [variable: payload]
```

Event types:
- `0x01` CURSOR_MOVE: `[int16 dx][int16 dy]`
- `0x02` MOUSE_CLICK: `[uint8 button]` (0=left, 1=middle, 2=right)
- `0x03` MOUSE_SCROLL: `[int8 direction]` (-1=down, 1=up)
- `0x04` KEY_PRESS: `[uint16 keycode][uint8 modifiers]`
- `0x05` KEY_RELEASE: `[uint16 keycode][uint8 modifiers]`
- `0x06` VOLUME: `[uint8 action]` (0=down, 1=up, 2=mute)
- `0xFE` AUTH: `[uint16 token_len][token_bytes]`
- `0xFF` HEARTBEAT: empty

## Authentication Flow

1. User registers via HTTP POST `/api/register` (email + password)
2. Server sends activation email via Postfix
3. User clicks activation link → GET `/api/activate/:token`
4. User logs in via POST `/api/login` → receives JWT
5. Desktop/mobile connect to UDP/DTLS server, send AUTH packet with JWT
6. Server validates JWT, pairs devices by account
