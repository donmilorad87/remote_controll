import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/remote_provider.dart';

const Map<String, int> _charToKeysym = {
  'a': 0x0061, 'b': 0x0062, 'c': 0x0063, 'd': 0x0064, 'e': 0x0065,
  'f': 0x0066, 'g': 0x0067, 'h': 0x0068, 'i': 0x0069, 'j': 0x006a,
  'k': 0x006b, 'l': 0x006c, 'm': 0x006d, 'n': 0x006e, 'o': 0x006f,
  'p': 0x0070, 'q': 0x0071, 'r': 0x0072, 's': 0x0073, 't': 0x0074,
  'u': 0x0075, 'v': 0x0076, 'w': 0x0077, 'x': 0x0078, 'y': 0x0079,
  'z': 0x007a,
  '0': 0x0030, '1': 0x0031, '2': 0x0032, '3': 0x0033, '4': 0x0034,
  '5': 0x0035, '6': 0x0036, '7': 0x0037, '8': 0x0038, '9': 0x0039,
  ' ': 0x0020, '.': 0x002e, ',': 0x002c, '-': 0x002d, '/': 0x002f,
  '@': 0x0040, '!': 0x0021, '?': 0x003f, '#': 0x0023,
};

const int _ksBackspace = 0xFF08, _ksReturn = 0xFF0D, _ksEscape = 0xFF1B;
const int _ksTab = 0xFF09, _ksUp = 0xFF52, _ksDown = 0xFF54;
const int _ksLeft = 0xFF51, _ksRight = 0xFF53;

// For modifier combos (Ctrl+C etc), use VK-range codes (0xFE00+key)
// so Windows sends them as VK codes instead of Unicode
const int _vkA = 0xFE41, _vkC = 0xFE43, _vkV = 0xFE56, _vkZ = 0xFE5A;

class KeyboardInputWidget extends StatefulWidget {
  const KeyboardInputWidget({super.key});
  @override
  State<KeyboardInputWidget> createState() => _KeyboardInputWidgetState();
}

class _KeyboardInputWidgetState extends State<KeyboardInputWidget> {
  final _ctrl = TextEditingController();
  String _last = '';

  @override
  void dispose() { _ctrl.dispose(); super.dispose(); }

  void _onChanged() {
    final c = context.read<RemoteProvider>().activeConnection;
    if (c == null) return;
    final txt = _ctrl.text;
    if (txt.length > _last.length) {
      for (final ch in txt.substring(_last.length).split('')) {
        final lower = ch.toLowerCase();
        final ks = _charToKeysym[lower];
        if (ks != null) c.sendKeyPress(ks, mod: ch != lower && ch == ch.toUpperCase() ? 0x01 : 0);
      }
    } else if (txt.length < _last.length) {
      for (int i = 0; i < _last.length - txt.length; i++) c.sendKeyPress(_ksBackspace);
    }
    _last = txt;
  }

  void _key(int ks, {int mod = 0}) =>
      context.read<RemoteProvider>().activeConnection?.sendKeyPress(ks, mod: mod);

  Widget _kBtn(String label, int ks) => Material(
    color: Theme.of(context).colorScheme.surfaceContainerHighest,
    borderRadius: BorderRadius.circular(10),
    child: InkWell(borderRadius: BorderRadius.circular(10), onTap: () => _key(ks),
      child: Padding(padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
        child: Text(label, style: const TextStyle(fontSize: 13)))));

  Widget _mBtn(String label, int ks, int mod) => Material(
    color: Theme.of(context).colorScheme.secondaryContainer,
    borderRadius: BorderRadius.circular(10),
    child: InkWell(borderRadius: BorderRadius.circular(10), onTap: () => _key(ks, mod: mod),
      child: Padding(padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
        child: Text(label, style: TextStyle(fontSize: 13,
          color: Theme.of(context).colorScheme.onSecondaryContainer)))));

  @override
  Widget build(BuildContext context) {
    return Column(children: [
      Text('Keyboard', style: Theme.of(context).textTheme.titleSmall?.copyWith(color: Colors.grey)),
      const SizedBox(height: 8),
      TextField(controller: _ctrl,
        decoration: InputDecoration(hintText: 'Tap to type...',
          prefixIcon: const Icon(Icons.keyboard),
          border: OutlineInputBorder(borderRadius: BorderRadius.circular(12)),
          suffixIcon: IconButton(icon: const Icon(Icons.clear),
            onPressed: () { _ctrl.clear(); _last = ''; })),
        onChanged: (_) => _onChanged()),
      const SizedBox(height: 12),
      Wrap(spacing: 8, runSpacing: 8, alignment: WrapAlignment.center, children: [
        _kBtn('Esc', _ksEscape), _kBtn('Tab', _ksTab),
        _kBtn('Enter', _ksReturn), _kBtn('Bksp', _ksBackspace),
        _kBtn('\u2191', _ksUp), _kBtn('\u2193', _ksDown),
        _kBtn('\u2190', _ksLeft), _kBtn('\u2192', _ksRight)]),
      const SizedBox(height: 8),
      Wrap(spacing: 8, runSpacing: 8, alignment: WrapAlignment.center, children: [
        _mBtn('Ctrl+C', _vkC, 0x02), _mBtn('Ctrl+V', _vkV, 0x02),
        _mBtn('Ctrl+Z', _vkZ, 0x02), _mBtn('Ctrl+A', _vkA, 0x02),
        _mBtn('Alt+Tab', _ksTab, 0x04)]),
    ]);
  }
}
