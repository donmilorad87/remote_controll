import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../models/event.dart';
import '../providers/connection_provider.dart';

class DPadWidget extends StatefulWidget {
  const DPadWidget({super.key});

  @override
  State<DPadWidget> createState() => _DPadWidgetState();
}

class _DPadWidgetState extends State<DPadWidget> {
  Timer? _repeatTimer;
  int _currentDirection = CursorDirection.stop;
  int _speed = 5;

  void _startMoving(int direction) {
    _currentDirection = direction;
    final conn = context.read<ConnectionProvider>();
    conn.sendCursorMove(direction, speed: _speed);

    _repeatTimer?.cancel();
    _repeatTimer = Timer.periodic(
      const Duration(milliseconds: 50),
      (_) => conn.sendCursorMove(direction, speed: _speed),
    );
  }

  void _stopMoving() {
    _repeatTimer?.cancel();
    _repeatTimer = null;
    _currentDirection = CursorDirection.stop;
  }

  Widget _directionButton(IconData icon, int direction) {
    return GestureDetector(
      onTapDown: (_) => _startMoving(direction),
      onTapUp: (_) => _stopMoving(),
      onTapCancel: _stopMoving,
      child: Container(
        width: 64,
        height: 64,
        decoration: BoxDecoration(
          color: _currentDirection == direction
              ? Theme.of(context).colorScheme.primary
              : Theme.of(context).colorScheme.surfaceContainerHighest,
          borderRadius: BorderRadius.circular(16),
        ),
        child: Icon(
          icon,
          size: 28,
          color: _currentDirection == direction
              ? Theme.of(context).colorScheme.onPrimary
              : Theme.of(context).colorScheme.onSurface,
        ),
      ),
    );
  }

  @override
  void dispose() {
    _repeatTimer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Text(
          'Cursor',
          style: Theme.of(context).textTheme.titleSmall?.copyWith(
            color: Colors.grey,
          ),
        ),
        const SizedBox(height: 8),

        // Speed slider
        Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.speed, size: 16, color: Colors.grey),
            const SizedBox(width: 8),
            SizedBox(
              width: 200,
              child: Slider(
                value: _speed.toDouble(),
                min: 1,
                max: 10,
                divisions: 9,
                label: 'Speed: $_speed',
                onChanged: (v) => setState(() => _speed = v.round()),
              ),
            ),
          ],
        ),
        const SizedBox(height: 8),

        // D-Pad layout
        Column(
          children: [
            // Top row: Up
            _directionButton(Icons.arrow_upward, CursorDirection.up),
            const SizedBox(height: 8),

            // Middle row: Left, Center, Right
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                _directionButton(Icons.arrow_back, CursorDirection.left),
                const SizedBox(width: 8),
                // Center area (empty or stop)
                Container(
                  width: 64,
                  height: 64,
                  decoration: BoxDecoration(
                    color: Theme.of(context).colorScheme.surfaceContainerLow,
                    borderRadius: BorderRadius.circular(16),
                    border: Border.all(
                      color: Theme.of(context).colorScheme.outline.withAlpha(50),
                    ),
                  ),
                  child: Icon(
                    Icons.control_camera,
                    color: Colors.grey.withAlpha(80),
                  ),
                ),
                const SizedBox(width: 8),
                _directionButton(Icons.arrow_forward, CursorDirection.right),
              ],
            ),
            const SizedBox(height: 8),

            // Bottom row: Down
            _directionButton(Icons.arrow_downward, CursorDirection.down),
          ],
        ),
      ],
    );
  }
}
