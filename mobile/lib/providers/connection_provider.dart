import 'package:flutter/foundation.dart';
import '../services/socket_service.dart';
import '../models/event.dart';
import 'auth_provider.dart';

class ConnectionProvider extends ChangeNotifier {
  final SocketService _socket = SocketService();
  AuthProvider? _auth;
  bool _connecting = false;
  String? _error;

  bool get isConnected => _socket.isConnected;
  bool get isConnecting => _connecting;
  String? get error => _error;
  SocketService get socket => _socket;

  void updateAuth(AuthProvider auth) {
    _auth = auth;

    if (auth.isAuthenticated && !_socket.isConnected && !_connecting) {
      connect();
    } else if (!auth.isAuthenticated && _socket.isConnected) {
      disconnect();
    }
  }

  Future<void> connect() async {
    if (_auth?.token == null) return;

    _connecting = true;
    _error = null;
    notifyListeners();

    try {
      await _socket.connect(_auth!.token!);
      _connecting = false;
      notifyListeners();
    } catch (e) {
      _error = 'Failed to connect: $e';
      _connecting = false;
      notifyListeners();
    }
  }

  void disconnect() {
    _socket.disconnect();
    notifyListeners();
  }

  Future<void> reconnect() async {
    disconnect();
    await Future.delayed(const Duration(milliseconds: 500));
    await connect();
  }

  // ─── Control methods ─────────────────────────────────────────────

  void sendCursorMove(int direction, {int speed = 5}) {
    _socket.sendCursorMove(direction, speed: speed);
  }

  void sendMouseClick(int button) {
    _socket.sendMouseClick(button);
  }

  void sendScrollUp() {
    _socket.sendMouseScroll(1);
  }

  void sendScrollDown() {
    _socket.sendMouseScroll(-1);
  }

  void sendKeyPress(int keycode, {int modifiers = 0}) {
    _socket.sendKeyPress(keycode, modifiers: modifiers);
    _socket.sendKeyRelease(keycode, modifiers: modifiers);
  }

  void sendVolumeUp() {
    _socket.sendVolume(VolumeAction.up);
  }

  void sendVolumeDown() {
    _socket.sendVolume(VolumeAction.down);
  }

  void sendVolumeMute() {
    _socket.sendVolume(VolumeAction.mute);
  }

  @override
  void dispose() {
    _socket.disconnect();
    super.dispose();
  }
}
