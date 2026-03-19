# Desktop App — Build Instructions

## Linux (X11 + PulseAudio)

### Install Dependencies (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  libgtk-3-dev \
  libappindicator3-dev \
  libx11-dev \
  libxtst-dev \
  libpulse-dev \
  libcurl4-openssl-dev \
  libjansson-dev \
  pkg-config
```

### Install Dependencies (Fedora)
```bash
sudo dnf install -y \
  gcc make \
  gtk3-devel \
  libappindicator-gtk3-devel \
  libX11-devel \
  libXtst-devel \
  pulseaudio-libs-devel \
  libcurl-devel \
  jansson-devel \
  pkgconf
```

### Install Dependencies (Arch)
```bash
sudo pacman -S \
  base-devel \
  gtk3 \
  libappindicator-gtk3 \
  libxtst \
  libpulse \
  curl \
  jansson \
  pkgconf
```

### Build
```bash
cd desktop/linux
make
```

### Run
```bash
# Make sure .env is configured
cd desktop/linux
./remotecontrol-desktop
```

The app will show a login window, then minimize to the system tray.

---

## Windows (Win32 API)

### Prerequisites
- **Visual Studio 2022** with "Desktop development with C++" workload
- **vcpkg** for dependency management

### Install Dependencies
```powershell
# Install vcpkg (if not already)
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat

# Install libraries
C:\vcpkg\vcpkg install curl:x64-windows jansson:x64-windows
```

### Build with CMake
```powershell
cd desktop\windows
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### Build with MSVC (manual)
```powershell
cl /W4 /WX /std:c11 /I..\shared ^
   main.c input_win32.c volume_win32.c client_net.c ^
   /link ws2_32.lib winmm.lib shell32.lib user32.lib ole32.lib comctl32.lib ^
   curl.lib jansson.lib ^
   /out:remotecontrol-desktop.exe
```

### Run
```powershell
cd desktop\windows
copy .env build\
cd build
.\remotecontrol-desktop.exe
```

The app shows a login dialog, then sits in the system tray (notification area).

**Note:** You need to copy `client_net.c` from `desktop/linux/` to `desktop/windows/` — it's cross-platform with `#ifdef _WIN32` detection (or adapt the socket calls for Winsock: `WSAStartup`, use `closesocket` instead of `close`, etc.).

---

## macOS (CGEvent + CoreAudio)

### Prerequisites
- **Xcode Command Line Tools**: `xcode-select --install`
- **Homebrew**: https://brew.sh

### Install Dependencies
```bash
brew install curl jansson
```

### Build
```bash
cd desktop/mac

# Copy client_net.c from Linux
cp ../linux/client_net.c .

make
```

### Run
```bash
cd desktop/mac
./remotecontrol-desktop
```

The app shows a login window, then appears as a menu bar icon (status item).

### Important: Accessibility Permissions
macOS requires **Accessibility** permissions for input simulation:

1. Open **System Settings → Privacy & Security → Accessibility**
2. Click the `+` button
3. Add `remotecontrol-desktop` (or your terminal if running from Terminal)

Without this permission, cursor/mouse/keyboard control will not work.

---

## Shared Code

The `desktop/shared/` directory contains platform-independent headers:
- `rc_protocol.h` — Binary protocol (event serialization/deserialization)
- `rc_client.h` — Client configuration and auth structures

The `client_net.c` file in `desktop/linux/` uses POSIX sockets and works on both Linux and macOS. For Windows, the socket calls need to be adapted for Winsock2 (`WSAStartup`, `closesocket`, `SOCKET` type, etc.).

---

## .env Configuration

Each desktop app directory has its own `.env` file:

```bash
RC_SERVER_HOST=local.remotecontrol.rs   # Server hostname/IP
RC_SERVER_API_PORT=7480                  # HTTP API port (via nginx)
RC_SERVER_SOCKET_PORT=7482               # UDP socket port (direct)
RC_SERVER_SCHEME=http                    # http or https
```

If the server is on a different machine, replace `local.remotecontrol.rs` with the server's LAN IP address.
