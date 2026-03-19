import 'dart:typed_data';

/// Event types matching the C server protocol
class EventType {
  static const int cursorMove = 0x01;
  static const int mouseClick = 0x02;
  static const int mouseScroll = 0x03;
  static const int keyPress = 0x04;
  static const int keyRelease = 0x05;
  static const int volume = 0x06;
  static const int auth = 0xFE;
  static const int heartbeat = 0xFF;
}

/// Mouse buttons
class MouseButton {
  static const int left = 0;
  static const int middle = 1;
  static const int right = 2;
}

/// Cursor directions (discrete D-pad)
class CursorDirection {
  static const int up = 0;
  static const int down = 1;
  static const int left = 2;
  static const int right = 3;
  static const int upLeft = 4;
  static const int upRight = 5;
  static const int downLeft = 6;
  static const int downRight = 7;
  static const int stop = 8;
}

/// Volume actions
class VolumeAction {
  static const int down = 0;
  static const int up = 1;
  static const int mute = 2;
}

/// Serialize events to binary protocol format
class EventSerializer {
  static Uint8List cursorMove(int direction, int speed) {
    return Uint8List.fromList([EventType.cursorMove, direction, speed]);
  }

  static Uint8List mouseClick(int button) {
    return Uint8List.fromList([EventType.mouseClick, button]);
  }

  static Uint8List mouseScroll(int direction) {
    return Uint8List.fromList([EventType.mouseScroll, direction & 0xFF]);
  }

  static Uint8List keyPress(int keycode, int modifiers) {
    return Uint8List.fromList([
      EventType.keyPress,
      (keycode >> 8) & 0xFF,
      keycode & 0xFF,
      modifiers,
    ]);
  }

  static Uint8List keyRelease(int keycode, int modifiers) {
    return Uint8List.fromList([
      EventType.keyRelease,
      (keycode >> 8) & 0xFF,
      keycode & 0xFF,
      modifiers,
    ]);
  }

  static Uint8List volumeControl(int action) {
    return Uint8List.fromList([EventType.volume, action]);
  }

  static Uint8List authToken(String token) {
    final tokenBytes = token.codeUnits;
    final len = tokenBytes.length;
    final buf = BytesBuilder();
    buf.addByte(EventType.auth);
    buf.addByte((len >> 8) & 0xFF);
    buf.addByte(len & 0xFF);
    buf.add(tokenBytes);
    return buf.toBytes();
  }

  static Uint8List heartbeat() {
    return Uint8List.fromList([EventType.heartbeat]);
  }
}
