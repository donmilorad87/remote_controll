import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/remote_provider.dart';

class VolumeControlsWidget extends StatelessWidget {
  const VolumeControlsWidget({super.key});

  Widget _btn(BuildContext ctx, {required IconData icon, required String label,
      required VoidCallback onTap, bool highlight = false}) {
    return Material(
      color: highlight ? Theme.of(ctx).colorScheme.errorContainer
          : Theme.of(ctx).colorScheme.surfaceContainerHighest,
      borderRadius: BorderRadius.circular(16),
      child: InkWell(borderRadius: BorderRadius.circular(16), onTap: onTap,
        child: SizedBox(width: 80, height: 64, child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [Icon(icon, size: 28, color: highlight
              ? Theme.of(ctx).colorScheme.onErrorContainer : null),
            const SizedBox(height: 2),
            Text(label, style: TextStyle(fontSize: 11, color: highlight
                ? Theme.of(ctx).colorScheme.onErrorContainer : null))]))));
  }

  @override
  Widget build(BuildContext context) {
    final c = context.read<RemoteProvider>().activeConnection;
    return Column(children: [
      Text('Volume', style: Theme.of(context).textTheme.titleSmall?.copyWith(color: Colors.grey)),
      const SizedBox(height: 8),
      Row(mainAxisAlignment: MainAxisAlignment.center, children: [
        _btn(context, icon: Icons.volume_down, label: '-', onTap: () => c?.sendVolumeDown()),
        const SizedBox(width: 12),
        _btn(context, icon: Icons.volume_off, label: 'Mute', onTap: () => c?.sendVolumeMute(), highlight: true),
        const SizedBox(width: 12),
        _btn(context, icon: Icons.volume_up, label: '+', onTap: () => c?.sendVolumeUp()),
      ]),
    ]);
  }
}
