import 'dart:async';
import 'package:flutter/foundation.dart';
import '../services/connection_service.dart';
import '../services/protocol.dart';

class RemoteProvider extends ChangeNotifier {
  final DiscoveryService _discovery = DiscoveryService();
  final Map<String, DesktopConnection> _connections = {};

  List<DesktopInfo> discoveredDesktops = [];
  String? _activeDesktopId;
  String? error;
  bool connecting = false;

  DesktopConnection? get activeConnection =>
      _activeDesktopId != null ? _connections[_activeDesktopId] : null;

  List<DesktopConnection> get connectedDesktops =>
      _connections.values.where((c) => c.connected).toList();

  String? get activeDesktopId => _activeDesktopId;

  StreamSubscription? _discoverySub;

  void startDiscovery() {
    _discovery.startListening();
    _discoverySub = _discovery.desktopsStream.listen((list) {
      discoveredDesktops = list;
      notifyListeners();
    });
  }

  void stopDiscovery() {
    _discoverySub?.cancel();
    _discovery.stopListening();
  }

  Future<bool> connectToDesktop(DesktopInfo desktop) async {
    if (_connections.containsKey(desktop.id) &&
        _connections[desktop.id]!.connected) {
      _activeDesktopId = desktop.id;
      notifyListeners();
      return true;
    }

    connecting = true;
    error = null;
    notifyListeners();

    final conn = DesktopConnection(desktop);
    final ok = await conn.connect();

    if (ok) {
      _connections[desktop.id] = conn;
      _activeDesktopId = desktop.id;
      error = null;
    } else {
      error = 'Failed to connect to ${desktop.name}';
    }

    connecting = false;
    notifyListeners();
    return ok;
  }

  void switchToDesktop(String id) {
    if (_connections.containsKey(id)) {
      _activeDesktopId = id;
      notifyListeners();
    }
  }

  void disconnectDesktop(String id) {
    _connections[id]?.disconnect();
    _connections.remove(id);
    if (_activeDesktopId == id) {
      _activeDesktopId = _connections.keys.isNotEmpty
          ? _connections.keys.first : null;
    }
    notifyListeners();
  }

  /// Reconnect all previously connected desktops (after app sleep/resume)
  Future<void> reconnectAll() async {
    print('[remote] Reconnecting all desktops...');
    final entries = Map<String, DesktopConnection>.from(_connections);

    for (final entry in entries.entries) {
      final old = entry.value;
      final desktop = old.desktop;
      old.disconnect();

      final fresh = DesktopConnection(desktop);
      final ok = await fresh.connect();
      if (ok) {
        _connections[entry.key] = fresh;
        print('[remote] Reconnected to ${desktop.name}');
      } else {
        _connections.remove(entry.key);
        print('[remote] Failed to reconnect to ${desktop.name}');
      }
    }

    if (_activeDesktopId != null && !_connections.containsKey(_activeDesktopId)) {
      _activeDesktopId = _connections.keys.isNotEmpty
          ? _connections.keys.first : null;
    }
    notifyListeners();
  }

  void disconnectAll() {
    for (final c in _connections.values) { c.disconnect(); }
    _connections.clear();
    _activeDesktopId = null;
    notifyListeners();
  }

  @override
  void dispose() {
    disconnectAll();
    stopDiscovery();
    super.dispose();
  }
}
