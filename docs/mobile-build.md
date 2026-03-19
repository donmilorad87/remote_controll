# Mobile App — Build Instructions (Flutter)

## Prerequisites

- **Flutter SDK 3.2+**: https://docs.flutter.dev/get-started/install
- **Android Studio** (for Android emulator and SDK)
- **Xcode** (for iOS, macOS only)
- **An Android device or emulator** / **An iOS device or simulator**

### Verify Flutter Setup
```bash
flutter doctor
```

All checks should pass for your target platform (Android/iOS).

---

## Project Setup

```bash
cd mobile

# Create the Flutter project structure (if not already created)
flutter create . --project-name remotecontrol_mobile --org rs.remotecontrol

# Get dependencies
flutter pub get
```

---

## Configure Server Address

Edit `mobile/.env`:

```bash
# For local development (server on same machine)
RC_SERVER_HOST=10.0.2.2          # Android emulator → host machine
RC_SERVER_API_PORT=7480
RC_SERVER_SOCKET_PORT=7482
RC_SERVER_SCHEME=http

# For real device on LAN
RC_SERVER_HOST=192.168.1.100     # Your server's LAN IP
RC_SERVER_API_PORT=7480
RC_SERVER_SOCKET_PORT=7482
RC_SERVER_SCHEME=http
```

**Important notes:**
- Android emulator: Use `10.0.2.2` to reach the host machine's `localhost`
- iOS simulator: Use `localhost` or `127.0.0.1`
- Physical device: Use the server machine's LAN IP (e.g., `192.168.1.100`)

---

## Android

### Build & Run (Debug)
```bash
cd mobile
flutter run -d android
```

### Build APK (Release)
```bash
flutter build apk --release
```

Output: `mobile/build/app/outputs/flutter-apk/app-release.apk`

### Build App Bundle (Play Store)
```bash
flutter build appbundle --release
```

### Android Permissions
The app needs internet access. Add to `android/app/src/main/AndroidManifest.xml`:
```xml
<uses-permission android:name="android.permission.INTERNET" />
```

### Network Security (for HTTP in debug)
For development with HTTP (not HTTPS), create `android/app/src/main/res/xml/network_security_config.xml`:
```xml
<?xml version="1.0" encoding="utf-8"?>
<network-security-config>
    <domain-config cleartextTrafficPermitted="true">
        <domain includeSubdomains="true">local.remotecontrol.rs</domain>
        <domain includeSubdomains="true">10.0.2.2</domain>
        <domain includeSubdomains="true">192.168.0.0/16</domain>
    </domain-config>
</network-security-config>
```

And reference it in `AndroidManifest.xml`:
```xml
<application
    android:networkSecurityConfig="@xml/network_security_config"
    ...>
```

---

## iOS

### Build & Run (Debug)
```bash
cd mobile
flutter run -d ios
```

### Build IPA (Release)
```bash
flutter build ipa --release
```

### iOS Info.plist
For HTTP in development, add to `ios/Runner/Info.plist`:
```xml
<key>NSAppTransportSecurity</key>
<dict>
    <key>NSAllowsLocalNetworking</key>
    <true/>
    <key>NSAllowsArbitraryLoads</key>
    <true/>
</dict>
```

**Note:** Remove `NSAllowsArbitraryLoads` for production. Use HTTPS instead.

---

## App Structure

```
mobile/lib/
├── main.dart                 # App entry point, providers setup
├── models/
│   └── event.dart            # Protocol events (serialization)
├── providers/
│   ├── auth_provider.dart    # Login/register/logout state
│   └── connection_provider.dart  # Socket connection + send events
├── services/
│   ├── api_service.dart      # HTTP API client (register/login)
│   └── socket_service.dart   # UDP socket client
├── screens/
│   ├── login_screen.dart     # Login/register UI
│   └── control_screen.dart   # Main control interface
└── widgets/
    ├── dpad_widget.dart      # D-pad cursor control
    ├── mouse_buttons.dart    # L/M/R click + scroll
    ├── volume_controls.dart  # Volume +/-/mute
    └── keyboard_input.dart   # Text input + special keys
```

---

## Testing on Physical Devices

### Android
1. Enable Developer Mode on your phone
2. Enable USB Debugging
3. Connect via USB
4. `flutter devices` should show your device
5. `flutter run -d <device-id>`

### iOS
1. Connect iPhone via USB
2. Open `ios/Runner.xcworkspace` in Xcode
3. Set your team/signing identity
4. `flutter run -d <device-id>`

---

## UI Overview

The control screen has these sections from top to bottom:

1. **D-Pad**: 4 directional buttons (up/down/left/right) with adjustable speed slider
2. **Mouse Buttons**: Left click, middle click, right click
3. **Scroll**: Scroll up / scroll down buttons
4. **Volume**: Volume up (+), volume down (-), mute toggle
5. **Keyboard**: Text field that opens native keyboard (each keystroke sent immediately), plus special keys (Esc, Tab, Enter, Backspace, arrows) and common shortcuts (Ctrl+C, Ctrl+V, Ctrl+Z, Ctrl+A, Alt+Tab)
