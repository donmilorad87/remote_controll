# RemoteControl — Architecture Documentation

## System Overview

RemoteControl is a cross-platform system that allows a mobile phone to remotely control a desktop computer. All communication flows through a central C server.

```
┌─────────────────┐                              ┌──────────────────┐
│   Flutter App    │                              │  Desktop App     │
│   (Android/iOS)  │                              │  (Win/Lin/Mac)   │
│                  │                              │                  │
│  ┌─────────────┐ │     UDP/DTLS (port 7482)     │ ┌──────────────┐ │
│  │ Socket Svc  │◄├─────────────────────────────►├►│ Socket Client│ │
│  └─────────────┘ │                              │ └──────────────┘ │
│                  │                              │                  │
│  ┌─────────────┐ │    HTTP REST (port 7480)     │ ┌──────────────┐ │
│  │  API Svc    │◄├─────────────────────────────►├►│ HTTP Client  │ │
│  └─────────────┘ │                              │ └──────────────┘ │
└─────────────────┘                              └──────────────────┘
                          │
                   ┌──────┴──────┐
                   │    Nginx    │ :7480
                   │  (reverse   │
                   │   proxy)    │
                   └──────┬──────┘
                          │
                   ┌──────┴──────┐
                   │  C Server   │ :7481 HTTP, :7482 UDP
                   │  (monolith) │
                   └──────┬──────┘
                          │
                   ┌──────┴──────┐
                   │  PgBouncer  │ :7484
                   └──────┬──────┘
                          │
                   ┌──────┴──────┐
                   │ PostgreSQL  │ :7483
                   └─────────────┘
```

## Communication Flow

### Authentication (HTTP)
1. Client sends `POST /api/register` or `POST /api/login`
2. Nginx proxies to C server
3. C server validates, queries PostgreSQL via PgBouncer
4. Returns JWT token to client

### Event Routing (UDP)
1. Mobile sends UDP packet to server (port 7482)
2. Server deserializes the binary event
3. Server looks up the paired desktop for this user
4. Server forwards the packet to the desktop's UDP address
5. Desktop receives, deserializes, and executes the OS command

### Session Management
- One desktop per account (new login kicks existing)
- Auto-pairing: mobile + desktop logged into same account = paired
- Heartbeat every 8 seconds, timeout after 30 seconds
- Sessions tracked in PostgreSQL for audit/admin

## Binary Protocol

All events use a compact binary format for minimal latency:

```
Byte 0: Event type (1 byte)
Bytes 1+: Payload (variable, type-specific)
```

| Event | Type | Payload | Total |
|-------|------|---------|-------|
| Cursor Move | 0x01 | direction(1) + speed(1) | 3 bytes |
| Mouse Click | 0x02 | button(1) | 2 bytes |
| Mouse Scroll | 0x03 | direction(1) | 2 bytes |
| Key Press | 0x04 | keycode(2) + modifiers(1) | 4 bytes |
| Key Release | 0x05 | keycode(2) + modifiers(1) | 4 bytes |
| Volume | 0x06 | action(1) | 2 bytes |
| Auth | 0xFE | token_len(2) + token(N) | 3+N bytes |
| Heartbeat | 0xFF | (none) | 1 byte |

## Database Schema

All SQL is in stored procedures. The application never sends raw SQL.

### Tables
- **users**: email, password_hash, is_activated
- **activation_tokens**: token, user_id, expires_at
- **sessions**: JWT tracking, device_type, active/revoked
- **device_connections**: live socket connections, platform info

### Key Procedures
- `sp_register_user` → creates user + activation token
- `sp_activate_user` → validates token, activates user
- `sp_get_user_by_email` → lookup for login
- `sp_create_session` → creates session, kicks existing desktop
- `sp_validate_session` → checks JWT validity
- `sp_connect_device` → registers socket connection
- `sp_get_paired_desktop` → finds the desktop for a mobile user

## Security

- Passwords: PBKDF2 with SHA-256, 100,000 iterations, random salt
- JWT: HMAC-SHA256 signed, includes expiration
- Socket auth: JWT sent as first packet, validated against DB
- DTLS: planned for production (currently plain UDP for development)
- One-desktop enforcement: prevents session hijacking

## Platform-Specific Input Handling

| Feature | Linux | Windows | macOS |
|---------|-------|---------|-------|
| Cursor | XTest (XTestFakeRelativeMotionEvent) | SendInput (MOUSEEVENTF_MOVE) | CGEventCreateMouseEvent |
| Mouse Click | XTest (XTestFakeButtonEvent) | SendInput (MOUSEEVENTF_*) | CGEventCreateMouseEvent |
| Scroll | XTest (buttons 4/5) | SendInput (MOUSEEVENTF_WHEEL) | CGEventCreateScrollWheelEvent |
| Keyboard | XTest (XTestFakeKeyEvent) | SendInput (INPUT_KEYBOARD) | CGEventCreateKeyboardEvent |
| Volume | PulseAudio (pa_context_*) | IAudioEndpointVolume (COM) | CoreAudio (AudioObject*) |
| System Tray | AppIndicator3 | Shell_NotifyIcon | NSStatusItem |
| Login UI | GTK3 Dialog | Win32 CreateWindow | NSWindow (Cocoa) |
