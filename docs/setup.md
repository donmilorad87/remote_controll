# RemoteControl — Full Setup Guide

## Prerequisites

- Docker & Docker Compose v2+
- Git
- A machine where the server will run

### For Desktop Development
- **Linux**: GCC, GTK3, libappindicator3, libXtst, PulseAudio dev headers, libcurl, libjansson
- **Windows**: Visual Studio 2022 (or MinGW), vcpkg for curl/jansson
- **macOS**: Xcode Command Line Tools, Homebrew for curl/jansson

### For Mobile Development
- Flutter SDK 3.2+
- Android Studio (for Android)
- Xcode (for iOS, macOS only)

---

## Step 1: Configure /etc/hosts

Add this line to your system's hosts file:

```bash
# Linux/macOS: /etc/hosts
# Windows: C:\Windows\System32\drivers\etc\hosts
127.0.0.1   local.remotecontrol.rs
```

On the mobile device, the `.env` file should point to the server machine's **LAN IP** instead:
```
RC_SERVER_HOST=192.168.1.100
```

---

## Step 2: Start Docker Services

```bash
cd /path/to/remote

# Build and start all services
docker-compose up -d --build

# Verify all services are running
docker-compose ps

# Check logs
docker-compose logs -f rc-server
```

### Expected running services:
| Service | Container | Port |
|---------|-----------|------|
| PostgreSQL | rc-postgresql | 7483 |
| PgBouncer | rc-pgbouncer | 7484 |
| Postfix | rc-postfix | 7486 |
| C Server | rc-server | 7481 (HTTP), 7482 (UDP) |
| Nginx | rc-nginx | 7480 |
| PgAdmin | rc-pgadmin | via /pgadmin/ |

### Verify server health:
```bash
curl http://local.remotecontrol.rs:7480/health
# Expected: {"status":"ok"}
```

### Access PgAdmin:
Open `http://local.remotecontrol.rs:7480/pgadmin/`
- Email: `admin@remotecontrol.rs`
- Password: `pgadmin_secret_change_me`

### Access Admin Panel:
Open `http://local.remotecontrol.rs:7480/admin/`

---

## Step 3: Build Desktop App

See [desktop-build.md](desktop-build.md) for platform-specific instructions.

---

## Step 4: Build Mobile App

See [mobile-build.md](mobile-build.md) for Flutter build instructions.

---

## Step 5: First Use

### Register
1. Open the desktop app OR mobile app
2. Enter email and password, click "Register"
3. Check your email for the activation link (check spam folder)
4. Click the activation link

### Login
1. Login on the desktop app (it connects and sits in system tray)
2. Login on the mobile app with the same account
3. They auto-pair through the server
4. Use the mobile controls to control the desktop!

---

## Troubleshooting

### Server won't start
```bash
docker-compose logs rc-server  # Check for errors
docker-compose logs rc-postgresql  # DB connection issues
```

### Can't connect from mobile
- Ensure mobile `.env` uses the server's LAN IP, not `local.remotecontrol.rs`
- Ensure port 7480 (HTTP) and 7482 (UDP) are not blocked by firewall
- On Linux: `sudo ufw allow 7480/tcp && sudo ufw allow 7482/udp`

### Activation email not received
- Check Postfix logs: `docker-compose logs rc-postfix`
- For local testing, check the mailbox directly or use the admin panel
- You can manually activate via PgAdmin: update the user's `is_activated` to `true`

### Desktop app can't find display (Linux)
- Ensure `$DISPLAY` is set (usually `:0`)
- Run from within an X11 session, not via SSH without X forwarding
