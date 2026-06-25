import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/remote_provider.dart';
import '../services/protocol.dart';
import 'control_screen.dart';

class DiscoveryScreen extends StatefulWidget {
  const DiscoveryScreen({super.key});

  @override
  State<DiscoveryScreen> createState() => _DiscoveryScreenState();
}

class _DiscoveryScreenState extends State<DiscoveryScreen> {
  @override
  void initState() {
    super.initState();
    context.read<RemoteProvider>().startDiscovery();
  }

  Future<void> _connect(DesktopInfo desktop) async {
    final provider = context.read<RemoteProvider>();
    final ok = await provider.connectToDesktop(desktop);
    if (ok && mounted) {
      Navigator.of(context).push(
        MaterialPageRoute(builder: (_) => const ControlScreen()));
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('RemoteControl'),
        centerTitle: true,
      ),
      body: Consumer<RemoteProvider>(
        builder: (context, remote, _) {
          final desktops = remote.discoveredDesktops;
          final connected = remote.connectedDesktops;

          return Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              // Connected desktops (quick switch)
              if (connected.isNotEmpty) ...[
                Padding(
                  padding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
                  child: Text('Connected',
                    style: Theme.of(context).textTheme.titleSmall?.copyWith(
                      color: Colors.grey))),
                SizedBox(
                  height: 52,
                  child: ListView.builder(
                    scrollDirection: Axis.horizontal,
                    padding: const EdgeInsets.symmetric(horizontal: 12),
                    itemCount: connected.length,
                    itemBuilder: (ctx, i) {
                      final c = connected[i];
                      final isActive = remote.activeDesktopId == c.desktop.id;
                      return Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 4),
                        child: ActionChip(
                          avatar: Icon(Icons.computer, size: 18,
                            color: isActive ? Colors.greenAccent : null),
                          label: Text(c.desktop.name),
                          backgroundColor: isActive
                            ? Theme.of(ctx).colorScheme.primaryContainer : null,
                          onPressed: () {
                            remote.switchToDesktop(c.desktop.id);
                            Navigator.of(context).push(MaterialPageRoute(
                              builder: (_) => const ControlScreen()));
                          },
                        ),
                      );
                    },
                  ),
                ),
                const Divider(),
              ],

              // Available desktops
              Padding(
                padding: const EdgeInsets.fromLTRB(16, 8, 16, 8),
                child: Row(
                  children: [
                    Text('Available Desktops',
                      style: Theme.of(context).textTheme.titleSmall?.copyWith(
                        color: Colors.grey)),
                    const Spacer(),
                    if (desktops.isEmpty)
                      const SizedBox(width: 16, height: 16,
                        child: CircularProgressIndicator(strokeWidth: 2)),
                  ],
                ),
              ),

              if (remote.error != null)
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 16),
                  child: Text(remote.error!,
                    style: const TextStyle(color: Colors.redAccent))),

              Expanded(
                child: desktops.isEmpty
                  ? Center(child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(Icons.search, size: 64,
                          color: Colors.grey.withAlpha(60)),
                        const SizedBox(height: 16),
                        Text('Searching for desktops on your network...',
                          style: TextStyle(color: Colors.grey.withAlpha(150))),
                        const SizedBox(height: 8),
                        Text('Make sure the desktop app is running.',
                          style: TextStyle(color: Colors.grey.withAlpha(100),
                            fontSize: 12)),
                      ]))
                  : ListView.builder(
                      padding: const EdgeInsets.symmetric(horizontal: 16),
                      itemCount: desktops.length,
                      itemBuilder: (ctx, i) {
                        final d = desktops[i];
                        final alreadyConnected = remote.connectedDesktops
                            .any((c) => c.desktop.id == d.id);

                        return Card(
                          margin: const EdgeInsets.only(bottom: 8),
                          child: ListTile(
                            leading: Icon(Icons.computer,
                              color: alreadyConnected
                                ? Colors.greenAccent
                                : Theme.of(ctx).colorScheme.primary),
                            title: Text(d.name,
                              style: const TextStyle(fontWeight: FontWeight.w600)),
                            subtitle: Text('${d.ip}:${d.port}',
                              style: TextStyle(color: Colors.grey.withAlpha(150),
                                fontSize: 12)),
                            trailing: remote.connecting
                              ? const SizedBox(width: 24, height: 24,
                                  child: CircularProgressIndicator(strokeWidth: 2))
                              : Icon(alreadyConnected
                                  ? Icons.check_circle : Icons.arrow_forward_ios,
                                  size: 20),
                            onTap: remote.connecting ? null : () => _connect(d),
                          ),
                        );
                      }),
              ),
            ],
          );
        },
      ),
    );
  }
}
