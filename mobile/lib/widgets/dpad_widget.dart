import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/protocol.dart';
import '../providers/remote_provider.dart';

class DPadWidget extends StatefulWidget {
  const DPadWidget({super.key});
  @override
  State<DPadWidget> createState() => _DPadWidgetState();
}

class _DPadWidgetState extends State<DPadWidget> {
  Timer? _timer;
  int _activeDir = CursorDir.stop;
  int _speed = 5;

  void _start(int dir) {
    _activeDir = dir;
    final c = context.read<RemoteProvider>().activeConnection;
    c?.sendCursorMove(dir, speed: _speed);
    _timer?.cancel();
    _timer = Timer.periodic(const Duration(milliseconds: 50),
        (_) => c?.sendCursorMove(dir, speed: _speed));
    setState(() {});
  }

  void _stop() { _timer?.cancel(); _timer = null; _activeDir = CursorDir.stop; setState(() {}); }

  Widget _btn(IconData icon, int dir) {
    final active = _activeDir == dir;
    return GestureDetector(
      onTapDown: (_) => _start(dir),
      onTapUp: (_) => _stop(),
      onTapCancel: _stop,
      child: Container(width: 64, height: 64,
        decoration: BoxDecoration(
          color: active ? Theme.of(context).colorScheme.primary
              : Theme.of(context).colorScheme.surfaceContainerHighest,
          borderRadius: BorderRadius.circular(16)),
        child: Icon(icon, size: 28,
          color: active ? Theme.of(context).colorScheme.onPrimary
              : Theme.of(context).colorScheme.onSurface)));
  }

  @override
  void dispose() { _timer?.cancel(); super.dispose(); }

  @override
  Widget build(BuildContext context) {
    return Column(children: [
      Row(mainAxisAlignment: MainAxisAlignment.spaceBetween, children: [
        Text('D-Pad', style: Theme.of(context).textTheme.titleSmall?.copyWith(color: Colors.grey)),
        Row(children: [
          const Icon(Icons.speed, size: 14, color: Colors.grey),
          const SizedBox(width: 4),
          SizedBox(width: 120, child: Slider(value: _speed.toDouble(), min: 1, max: 10,
            divisions: 9, label: '$_speed',
            onChanged: (v) => setState(() => _speed = v.round())))]),
      ]),
      const SizedBox(height: 4),
      _btn(Icons.arrow_upward, CursorDir.up),
      const SizedBox(height: 8),
      Row(mainAxisAlignment: MainAxisAlignment.center, children: [
        _btn(Icons.arrow_back, CursorDir.left),
        const SizedBox(width: 72),
        _btn(Icons.arrow_forward, CursorDir.right)]),
      const SizedBox(height: 8),
      _btn(Icons.arrow_downward, CursorDir.down),
    ]);
  }
}
