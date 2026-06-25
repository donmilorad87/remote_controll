import 'dart:typed_data';

class EventType {
  static const int cursorMove = 0x01;
  static const int mouseClick = 0x02;
  static const int mouseScroll = 0x03;
  static const int keyPress = 0x04;
  static const int keyRelease = 0x05;
  static const int volume = 0x06;
  static const int touchpad = 0x07;
  static const int announce = 0xA0;
  static const int connect = 0xA1;
  static const int connected = 0xA2;
  static const int heartbeat = 0xFF;
}

class MouseButton { static const int left = 0, middle = 1, right = 2; }
class CursorDir { static const int up = 0, down = 1, left = 2, right = 3, stop = 8; }
class VolumeAction { static const int down = 0, up = 1, mute = 2; }

const int rcPort = 7482;
const int rcAnnouncePort = 7483;

class Protocol {
  static Uint8List cursorMove(int dir, int speed) =>
      Uint8List.fromList([EventType.cursorMove, dir, speed]);

  static Uint8List touchpad(int dx, int dy) => Uint8List.fromList([
    EventType.touchpad,
    (dx >> 8) & 0xFF, dx & 0xFF,
    (dy >> 8) & 0xFF, dy & 0xFF,
  ]);

  static Uint8List mouseClick(int btn) =>
      Uint8List.fromList([EventType.mouseClick, btn]);

  static Uint8List mouseScroll(int dir) =>
      Uint8List.fromList([EventType.mouseScroll, dir & 0xFF]);

  static Uint8List keyPress(int kc, int mod) =>
      Uint8List.fromList([EventType.keyPress, (kc >> 8) & 0xFF, kc & 0xFF, mod]);

  static Uint8List keyRelease(int kc, int mod) =>
      Uint8List.fromList([EventType.keyRelease, (kc >> 8) & 0xFF, kc & 0xFF, mod]);

  static Uint8List volume(int action) =>
      Uint8List.fromList([EventType.volume, action]);

  static Uint8List connectPacket() =>
      Uint8List.fromList([EventType.connect]);

  static Uint8List heartbeat() =>
      Uint8List.fromList([EventType.heartbeat]);
}

/// Parsed desktop announcement
class DesktopInfo {
  final String name;
  final String ip;
  final int port;
  DateTime lastSeen;

  DesktopInfo({required this.name, required this.ip, required this.port})
      : lastSeen = DateTime.now();

  String get id => '$ip:$port';

  static DesktopInfo? fromAnnounce(Uint8List data, String senderIp) {
    if (data.isEmpty || data[0] != EventType.announce || data.length < 4) return null;
    final nameLen = data[1];
    if (data.length < 2 + nameLen + 2) return null;
    final name = String.fromCharCodes(data, 2, 2 + nameLen);
    final port = (data[2 + nameLen] << 8) | data[3 + nameLen];
    return DesktopInfo(name: name, ip: senderIp, port: port);
  }
}
