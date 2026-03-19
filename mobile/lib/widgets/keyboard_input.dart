import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import '../providers/connection_provider.dart';

/// Maps common characters to X11 keysyms (used across platforms).
/// The desktop apps translate these to platform-native keycodes.
const Map<String, int> _charToKeysym = {
  'a': 0x0061, 'b': 0x0062, 'c': 0x0063, 'd': 0x0064, 'e': 0x0065,
  'f': 0x0066, 'g': 0x0067, 'h': 0x0068, 'i': 0x0069, 'j': 0x006a,
  'k': 0x006b, 'l': 0x006c, 'm': 0x006d, 'n': 0x006e, 'o': 0x006f,
  'p': 0x0070, 'q': 0x0071, 'r': 0x0072, 's': 0x0073, 't': 0x0074,
  'u': 0x0075, 'v': 0x0076, 'w': 0x0077, 'x': 0x0078, 'y': 0x0079,
  'z': 0x007a,
  '0': 0x0030, '1': 0x0031, '2': 0x0032, '3': 0x0033, '4': 0x0034,
  '5': 0x0035, '6': 0x0036, '7': 0x0037, '8': 0x0038, '9': 0x0039,
  ' ': 0x0020, '\n': 0xFF0D, '\t': 0xFF09,
  '.': 0x002e, ',': 0x002c, ';': 0x003b, ':': 0x003a,
  '!': 0x0021, '?': 0x003f, '@': 0x0040, '#': 0x0023,
  '-': 0x002d, '_': 0x005f, '=': 0x003d, '+': 0x002b,
  '(': 0x0028, ')': 0x0029, '[': 0x005b, ']': 0x005d,
  '/': 0x002f, '\\': 0x005c, '\'': 0x0027, '"': 0x0022,
};

/// Special key keysyms
const int _ksBackspace = 0xFF08;
const int _ksReturn = 0xFF0D;
const int _ksEscape = 0xFF1B;
const int _ksTab = 0xFF09;
const int _ksUp = 0xFF52;
const int _ksDown = 0xFF54;
const int _ksLeft = 0xFF51;
const int _ksRight = 0xFF53;

class KeyboardInputWidget extends StatefulWidget {
  const KeyboardInputWidget({super.key});

  @override
  State<KeyboardInputWidget> createState() => _KeyboardInputWidgetState();
}

class _KeyboardInputWidgetState extends State<KeyboardInputWidget> {
  final _focusNode = FocusNode();
  final _controller = TextEditingController();
  String _lastText = '';

  @override
  void dispose() {
    _focusNode.dispose();
    _controller.dispose();
    super.dispose();
  }

  void _onTextChanged() {
    final conn = context.read<ConnectionProvider>();
    final newText = _controller.text;

    if (newText.length > _lastText.length) {
      // Character was added — send the new character
      final newChar = newText.substring(_lastText.length);
      for (final char in newChar.split('')) {
        final lower = char.toLowerCase();
        final keysym = _charToKeysym[lower];
        if (keysym != null) {
          // Determine if shift is needed (uppercase letter)
          final isUpper = char != lower && char == char.toUpperCase();
          conn.sendKeyPress(keysym, modifiers: isUpper ? 0x01 : 0);
        }
      }
    } else if (newText.length < _lastText.length) {
      // Character was deleted — send backspace
      final deleted = _lastText.length - newText.length;
      for (int i = 0; i < deleted; i++) {
        conn.sendKeyPress(_ksBackspace);
      }
    }

    _lastText = newText;
  }

  void _sendSpecialKey(int keysym, {int modifiers = 0}) {
    context.read<ConnectionProvider>().sendKeyPress(keysym, modifiers: modifiers);
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Text(
          'Keyboard',
          style: Theme.of(context).textTheme.titleSmall?.copyWith(
            color: Colors.grey,
          ),
        ),
        const SizedBox(height: 8),

        // Text input field — shows native keyboard
        TextField(
          controller: _controller,
          focusNode: _focusNode,
          decoration: InputDecoration(
            hintText: 'Tap to type...',
            prefixIcon: const Icon(Icons.keyboard),
            border: OutlineInputBorder(
              borderRadius: BorderRadius.circular(12),
            ),
            suffixIcon: IconButton(
              icon: const Icon(Icons.clear),
              onPressed: () {
                _controller.clear();
                _lastText = '';
              },
            ),
          ),
          onChanged: (_) => _onTextChanged(),
        ),
        const SizedBox(height: 12),

        // Special keys row
        Wrap(
          spacing: 8,
          runSpacing: 8,
          alignment: WrapAlignment.center,
          children: [
            _specialKeyButton('Esc', _ksEscape),
            _specialKeyButton('Tab', _ksTab),
            _specialKeyButton('Enter', _ksReturn),
            _specialKeyButton('Bksp', _ksBackspace),
            _specialKeyButton(String.fromCharCode(0x2191), _ksUp),    // ↑
            _specialKeyButton(String.fromCharCode(0x2193), _ksDown),  // ↓
            _specialKeyButton(String.fromCharCode(0x2190), _ksLeft),  // ←
            _specialKeyButton(String.fromCharCode(0x2192), _ksRight), // →
          ],
        ),
        const SizedBox(height: 8),

        // Modifier combos
        Wrap(
          spacing: 8,
          runSpacing: 8,
          alignment: WrapAlignment.center,
          children: [
            _modifierButton('Ctrl+C', 0x0063, 0x02),
            _modifierButton('Ctrl+V', 0x0076, 0x02),
            _modifierButton('Ctrl+Z', 0x007a, 0x02),
            _modifierButton('Ctrl+A', 0x0061, 0x02),
            _modifierButton('Alt+Tab', _ksTab, 0x04),
          ],
        ),
      ],
    );
  }

  Widget _specialKeyButton(String label, int keysym) {
    return Material(
      color: Theme.of(context).colorScheme.surfaceContainerHighest,
      borderRadius: BorderRadius.circular(10),
      child: InkWell(
        borderRadius: BorderRadius.circular(10),
        onTap: () => _sendSpecialKey(keysym),
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
          child: Text(label, style: const TextStyle(fontSize: 13)),
        ),
      ),
    );
  }

  Widget _modifierButton(String label, int keysym, int modifiers) {
    return Material(
      color: Theme.of(context).colorScheme.secondaryContainer,
      borderRadius: BorderRadius.circular(10),
      child: InkWell(
        borderRadius: BorderRadius.circular(10),
        onTap: () => _sendSpecialKey(keysym, modifiers: modifiers),
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
          child: Text(
            label,
            style: TextStyle(
              fontSize: 13,
              color: Theme.of(context).colorScheme.onSecondaryContainer,
            ),
          ),
        ),
      ),
    );
  }
}
