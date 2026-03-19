import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/connection_provider.dart';

class VolumeControlsWidget extends StatelessWidget {
  const VolumeControlsWidget({super.key});

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Text(
          'Volume',
          style: Theme.of(context).textTheme.titleSmall?.copyWith(
            color: Colors.grey,
          ),
        ),
        const SizedBox(height: 8),
        Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            // Volume Down
            _volumeButton(
              context,
              icon: Icons.volume_down,
              label: '-',
              onTap: () => context.read<ConnectionProvider>().sendVolumeDown(),
            ),
            const SizedBox(width: 12),

            // Mute
            _volumeButton(
              context,
              icon: Icons.volume_off,
              label: 'Mute',
              onTap: () => context.read<ConnectionProvider>().sendVolumeMute(),
              highlight: true,
            ),
            const SizedBox(width: 12),

            // Volume Up
            _volumeButton(
              context,
              icon: Icons.volume_up,
              label: '+',
              onTap: () => context.read<ConnectionProvider>().sendVolumeUp(),
            ),
          ],
        ),
      ],
    );
  }

  Widget _volumeButton(
    BuildContext context, {
    required IconData icon,
    required String label,
    required VoidCallback onTap,
    bool highlight = false,
  }) {
    return Material(
      color: highlight
          ? Theme.of(context).colorScheme.errorContainer
          : Theme.of(context).colorScheme.surfaceContainerHighest,
      borderRadius: BorderRadius.circular(16),
      child: InkWell(
        borderRadius: BorderRadius.circular(16),
        onTap: onTap,
        child: SizedBox(
          width: 80,
          height: 64,
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(
                icon,
                size: 28,
                color: highlight
                    ? Theme.of(context).colorScheme.onErrorContainer
                    : null,
              ),
              const SizedBox(height: 2),
              Text(
                label,
                style: TextStyle(
                  fontSize: 11,
                  color: highlight
                      ? Theme.of(context).colorScheme.onErrorContainer
                      : null,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
