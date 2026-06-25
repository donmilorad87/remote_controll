import 'dart:async';
import 'dart:io';
import 'dart:typed_data';
import 'protocol.dart';

/// Manages a single connection to one desktop
class DesktopConnection {
  final DesktopInfo desktop;
  RawDatagramSocket? _socket;
  Timer? _heartbeat;
  bool connected = false;

  DesktopConnection(this.desktop);

  StreamSubscription? _sub;

  Future<bool> connect() async {
    print('[conn] Connecting to ${desktop.ip}:${desktop.port} (${desktop.name})');
    _socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);
    print('[conn] Local socket bound to port ${_socket!.port}');

    final addr = InternetAddress(desktop.ip);
    _socket!.send(Protocol.connectPacket(), addr, desktop.port);

    final completer = Completer<bool>();

    _sub = _socket!.listen((event) {
      if (event == RawSocketEvent.read) {
        final dg = _socket?.receive();
        if (dg != null && dg.data.isNotEmpty) {
          print('[conn] Got reply: 0x${dg.data[0].toRadixString(16)} from ${dg.address.address}');
          if (dg.data[0] == EventType.connected && !completer.isCompleted) {
            completer.complete(true);
          }
        }
      }
    });

    final timeout = Timer(const Duration(seconds: 3), () {
      if (!completer.isCompleted) {
        print('[conn] Connection timeout!');
        completer.complete(false);
      }
    });

    connected = await completer.future;
    timeout.cancel();

    if (connected) {
      print('[conn] Connected successfully!');
      _heartbeat = Timer.periodic(
          const Duration(seconds: 8), (_) => _send(Protocol.heartbeat()));
    } else {
      print('[conn] Connection FAILED');
      _sub?.cancel();
      _sub = null;
      _socket?.close();
      _socket = null;
    }
    return connected;
  }

  void disconnect() {
    _heartbeat?.cancel();
    _sub?.cancel();
    _sub = null;
    _socket?.close();
    _socket = null;
    connected = false;
  }

  void _send(Uint8List data) {
    if (_socket == null || !connected) {
      print('[conn] SEND BLOCKED: socket=${_socket != null}, connected=$connected');
      return;
    }
    final sent = _socket!.send(data, InternetAddress(desktop.ip), desktop.port);
    if (sent != data.length) {
      print('[conn] SEND PARTIAL: $sent/${data.length}');
    }
  }

  void sendCursorMove(int dir, {int speed = 5}) =>
      _send(Protocol.cursorMove(dir, speed));
  void sendTouchpad(int dx, int dy) => _send(Protocol.touchpad(dx, dy));
  void sendMouseClick(int btn) => _send(Protocol.mouseClick(btn));
  void sendScrollUp() => _send(Protocol.mouseScroll(1));
  void sendScrollDown() => _send(Protocol.mouseScroll(-1));
  void sendKeyPress(int kc, {int mod = 0}) {
    _send(Protocol.keyPress(kc, mod));
    _send(Protocol.keyRelease(kc, mod));
  }
  void sendVolumeUp() => _send(Protocol.volume(VolumeAction.up));
  void sendVolumeDown() => _send(Protocol.volume(VolumeAction.down));
  void sendVolumeMute() => _send(Protocol.volume(VolumeAction.mute));
}

/// Discovers desktops on all LAN interfaces by scanning each /24 subnet
class DiscoveryService {
  RawDatagramSocket? _scanSocket;
  final _desktops = <String, DesktopInfo>{};
  final _controller = StreamController<List<DesktopInfo>>.broadcast();
  Timer? _cleanupTimer;
  Timer? _scanTimer;

  Stream<List<DesktopInfo>> get desktopsStream => _controller.stream;
  List<DesktopInfo> get desktops => _desktops.values.toList();

  Future<void> startListening() async {
    _scanSocket = await RawDatagramSocket.bind(
        InternetAddress.anyIPv4, 0, reuseAddress: true);

    _scanSocket!.listen((event) {
      if (event != RawSocketEvent.read) return;
      final dg = _scanSocket!.receive();
      if (dg == null) return;

      // Parse ANNOUNCE reply from desktop
      final info = DesktopInfo.fromAnnounce(dg.data, dg.address.address);
      if (info != null) {
        _desktops[info.id] = info;
        _controller.add(desktops);
      }
    });

    // Scan immediately, then every 3 seconds
    _scanAllSubnets();
    _scanTimer =
        Timer.periodic(const Duration(seconds: 3), (_) => _scanAllSubnets());

    // Cleanup stale desktops every 6s
    _cleanupTimer = Timer.periodic(const Duration(seconds: 6), (_) {
      final now = DateTime.now();
      _desktops
          .removeWhere((_, d) => now.difference(d.lastSeen).inSeconds > 12);
      _controller.add(desktops);
    });
  }

  /// Scan every /24 subnet on every network interface
  Future<void> _scanAllSubnets() async {
    if (_scanSocket == null) return;

    final interfaces = await NetworkInterface.list(
        type: InternetAddressType.IPv4, includeLoopback: false);

    for (final iface in interfaces) {
      for (final addr in iface.addresses) {
        final parts = addr.address.split('.');
        if (parts.length != 4) continue;

        // Skip Docker/virtual interfaces (172.x, 10.x ranges that are common)
        // Only scan 192.168.x.x and similar home/office ranges
        final subnet = '${parts[0]}.${parts[1]}.${parts[2]}';
        final myLast = int.parse(parts[3]);

        // Send CONNECT probe to all IPs on this subnet
        final packet = Protocol.connectPacket();
        for (int i = 1; i <= 254; i++) {
          if (i == myLast) continue;
          try {
            _scanSocket!
                .send(packet, InternetAddress('$subnet.$i'), rcPort);
          } catch (_) {}
        }
      }
    }
  }

  void stopListening() {
    _scanTimer?.cancel();
    _cleanupTimer?.cancel();
    _scanSocket?.close();
    if (!_controller.isClosed) _controller.close();
  }
}
