import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/remote_provider.dart';
import '../services/protocol.dart';

/// A large touch area that works like a laptop trackpad.
/// Drag your finger to move the cursor precisely.
/// Tap for left click. Two-finger tap for right click (via buttons below).
class TouchpadWidget extends StatefulWidget {
  const TouchpadWidget({super.key});

  @override
  State<TouchpadWidget> createState() => _TouchpadWidgetState();
}

class _TouchpadWidgetState extends State<TouchpadWidget> {
  Offset? _lastPos;
  double _sensitivity = 1.5;
  bool _dragging = false;

  void _onPanStart(DragStartDetails details) {
    _lastPos = details.localPosition;
    _dragging = true;
  }

  void _onPanUpdate(DragUpdateDetails details) {
    if (_lastPos == null) return;

    final dx = (details.localPosition.dx - _lastPos!.dx) * _sensitivity;
    final dy = (details.localPosition.dy - _lastPos!.dy) * _sensitivity;
    _lastPos = details.localPosition;

    if (dx.abs() > 0.5 || dy.abs() > 0.5) {
      final conn = context.read<RemoteProvider>().activeConnection;
      conn?.sendTouchpad(dx.round(), dy.round());
    }
  }

  void _onPanEnd(DragEndDetails details) {
    _lastPos = null;
    _dragging = false;
  }

  void _onTap() {
    // Single tap = left click
    final conn = context.read<RemoteProvider>().activeConnection;
    conn?.sendMouseClick(MouseButton.left);
  }

  void _onDoubleTap() {
    // Double tap = double click
    final conn = context.read<RemoteProvider>().activeConnection;
    conn?.sendMouseClick(MouseButton.left);
    conn?.sendMouseClick(MouseButton.left);
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text('Touchpad',
                style: Theme.of(context)
                    .textTheme
                    .titleSmall
                    ?.copyWith(color: Colors.grey)),
            Row(
              children: [
                const Icon(Icons.speed, size: 14, color: Colors.grey),
                const SizedBox(width: 4),
                SizedBox(
                  width: 120,
                  child: Slider(
                    value: _sensitivity,
                    min: 0.5,
                    max: 4.0,
                    divisions: 7,
                    label: '${_sensitivity.toStringAsFixed(1)}x',
                    onChanged: (v) => setState(() => _sensitivity = v),
                  ),
                ),
              ],
            ),
          ],
        ),
        const SizedBox(height: 4),
        GestureDetector(
          onPanStart: _onPanStart,
          onPanUpdate: _onPanUpdate,
          onPanEnd: _onPanEnd,
          onTap: _onTap,
          onDoubleTap: _onDoubleTap,
          child: Container(
            width: double.infinity,
            height: 220,
            decoration: BoxDecoration(
              color: _dragging
                  ? Theme.of(context).colorScheme.primary.withAlpha(30)
                  : Theme.of(context).colorScheme.surfaceContainerHighest,
              borderRadius: BorderRadius.circular(16),
              border: Border.all(
                color: _dragging
                    ? Theme.of(context).colorScheme.primary.withAlpha(80)
                    : Theme.of(context).colorScheme.outline.withAlpha(40),
                width: _dragging ? 2 : 1,
              ),
            ),
            child: Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Icon(
                    Icons.touch_app_outlined,
                    size: 32,
                    color: Colors.grey.withAlpha(80),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    'Drag to move cursor',
                    style: TextStyle(
                      color: Colors.grey.withAlpha(100),
                      fontSize: 13,
                    ),
                  ),
                  Text(
                    'Tap = click  |  Double-tap = double-click',
                    style: TextStyle(
                      color: Colors.grey.withAlpha(60),
                      fontSize: 11,
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
      ],
    );
  }
}
