import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/protocol.dart';
import '../providers/remote_provider.dart';

class MouseButtonsWidget extends StatelessWidget {
  const MouseButtonsWidget({super.key});

  Widget _btn(BuildContext ctx, String label, int button, IconData icon) {
    return Expanded(child: Padding(padding: const EdgeInsets.symmetric(horizontal: 4),
      child: Material(
        color: Theme.of(ctx).colorScheme.surfaceContainerHighest,
        borderRadius: BorderRadius.circular(16),
        child: InkWell(borderRadius: BorderRadius.circular(16),
          onTap: () => ctx.read<RemoteProvider>().activeConnection?.sendMouseClick(button),
          child: SizedBox(height: 64, child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [Icon(icon, size: 24), const SizedBox(height: 4),
              Text(label, style: const TextStyle(fontSize: 11))]))))));
  }

  @override
  Widget build(BuildContext context) {
    final c = context.read<RemoteProvider>().activeConnection;
    return Column(children: [
      Text('Mouse', style: Theme.of(context).textTheme.titleSmall?.copyWith(color: Colors.grey)),
      const SizedBox(height: 8),
      Row(children: [
        _btn(context, 'Left', MouseButton.left, Icons.mouse),
        _btn(context, 'Middle', MouseButton.middle, Icons.commit),
        _btn(context, 'Right', MouseButton.right, Icons.mouse)]),
      const SizedBox(height: 8),
      Row(children: [
        Expanded(child: Padding(padding: const EdgeInsets.symmetric(horizontal: 4),
          child: Material(color: Theme.of(context).colorScheme.surfaceContainerHighest,
            borderRadius: BorderRadius.circular(16),
            child: InkWell(borderRadius: BorderRadius.circular(16),
              onTap: () => c?.sendScrollUp(),
              child: const SizedBox(height: 48, child: Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [Icon(Icons.arrow_upward, size: 18), SizedBox(width: 4),
                  Text('Scroll Up', style: TextStyle(fontSize: 13))])))))),
        Expanded(child: Padding(padding: const EdgeInsets.symmetric(horizontal: 4),
          child: Material(color: Theme.of(context).colorScheme.surfaceContainerHighest,
            borderRadius: BorderRadius.circular(16),
            child: InkWell(borderRadius: BorderRadius.circular(16),
              onTap: () => c?.sendScrollDown(),
              child: const SizedBox(height: 48, child: Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [Icon(Icons.arrow_downward, size: 18), SizedBox(width: 4),
                  Text('Scroll Down', style: TextStyle(fontSize: 13))])))))),
      ]),
    ]);
  }
}
