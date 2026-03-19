import 'dart:async';
import 'dart:io';
import 'dart:typed_data';
import 'package:flutter_dotenv/flutter_dotenv.dart';
import '../models/event.dart';

class SocketService {
  RawDatagramSocket? _socket;
  InternetAddress? _serverAddress;
  late int _serverPort;
  Timer? _heartbeatTimer;
  bool _connected = false;

  bool get isConnected => _connected;

  Future<void> connect(String token) async {
    final host = dotenv.env['RC_SERVER_HOST'] ?? 'local.remotecontrol.rs';
    _serverPort = int.parse(dotenv.env['RC_SERVER_SOCKET_PORT'] ?? '7482');

    // Resolve hostname
    final addresses = await InternetAddress.lookup(host);
    if (addresses.isEmpty) {
      throw SocketException('Cannot resolve $host');
    }
    _serverAddress = addresses.first;

    // Bind to any available port
    _socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);

    // Send auth packet
    _send(EventSerializer.authToken(token));
    _connected = true;

    // Start heartbeat
    _heartbeatTimer = Timer.periodic(
      const Duration(seconds: 8),
      (_) => _send(EventSerializer.heartbeat()),
    );
  }

  void disconnect() {
    _heartbeatTimer?.cancel();
    _heartbeatTimer = null;
    _socket?.close();
    _socket = null;
    _connected = false;
  }

  void _send(Uint8List data) {
    if (_socket == null || _serverAddress == null) return;
    _socket!.send(data, _serverAddress!, _serverPort);
  }

  // ─── Control methods ─────────────────────────────────────────────

  void sendCursorMove(int direction, {int speed = 5}) {
    _send(EventSerializer.cursorMove(direction, speed));
  }

  void sendMouseClick(int button) {
    _send(EventSerializer.mouseClick(button));
  }

  void sendMouseScroll(int direction) {
    _send(EventSerializer.mouseScroll(direction));
  }

  void sendKeyPress(int keycode, {int modifiers = 0}) {
    _send(EventSerializer.keyPress(keycode, modifiers));
  }

  void sendKeyRelease(int keycode, {int modifiers = 0}) {
    _send(EventSerializer.keyRelease(keycode, modifiers));
  }

  void sendVolume(int action) {
    _send(EventSerializer.volumeControl(action));
  }
}
