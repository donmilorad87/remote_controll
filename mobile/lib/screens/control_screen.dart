import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/remote_provider.dart';
import '../widgets/touchpad_widget.dart';
import '../widgets/dpad_widget.dart';
import '../widgets/mouse_buttons.dart';
import '../widgets/volume_controls.dart';
import '../widgets/keyboard_input.dart';

class ControlScreen extends StatefulWidget {
  const ControlScreen({super.key});

  @override
  State<ControlScreen> createState() => _ControlScreenState();
}

class _ControlScreenState extends State<ControlScreen> with WidgetsBindingObserver {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.resumed) {
      // App came back from sleep — auto-reconnect
      final remote = context.read<RemoteProvider>();
      remote.reconnectAll();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<RemoteProvider>(
      builder: (context, remote, _) {
        final conn = remote.activeConnection;
        final connected = remote.connectedDesktops;

        return Scaffold(
          appBar: AppBar(
            title: Text(conn?.desktop.name ?? 'RemoteControl'),
            centerTitle: true,
            leading: IconButton(
              icon: const Icon(Icons.arrow_back),
              onPressed: () => Navigator.of(context).pop(),
            ),
            actions: [
              // Connection indicator
              Padding(
                padding: const EdgeInsets.only(right: 8),
                child: Icon(Icons.circle, size: 12,
                  color: conn?.connected == true
                      ? Colors.greenAccent : Colors.redAccent),
              ),
              // Desktop switcher (if multiple connected)
              if (connected.length > 1)
                PopupMenuButton<String>(
                  icon: const Icon(Icons.swap_horiz),
                  onSelected: (id) => remote.switchToDesktop(id),
                  itemBuilder: (_) => connected.map((c) =>
                    PopupMenuItem(
                      value: c.desktop.id,
                      child: Row(children: [
                        Icon(Icons.computer, size: 18,
                          color: c.desktop.id == remote.activeDesktopId
                              ? Colors.greenAccent : null),
                        const SizedBox(width: 8),
                        Text(c.desktop.name),
                      ]),
                    )).toList(),
                ),
              PopupMenuButton<String>(
                onSelected: (v) {
                  if (v == 'disconnect' && conn != null) {
                    remote.disconnectDesktop(conn.desktop.id);
                    Navigator.of(context).pop();
                  }
                },
                itemBuilder: (_) => [
                  const PopupMenuItem(value: 'disconnect',
                    child: Text('Disconnect')),
                ],
              ),
            ],
          ),
          body: conn == null
            ? const Center(child: Text('No desktop selected'))
            : const SafeArea(
                child: SingleChildScrollView(
                  padding: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                  child: Column(children: [
                    TouchpadWidget(),
                    SizedBox(height: 16),
                    DPadWidget(),
                    SizedBox(height: 16),
                    MouseButtonsWidget(),
                    SizedBox(height: 16),
                    VolumeControlsWidget(),
                    SizedBox(height: 16),
                    KeyboardInputWidget(),
                    SizedBox(height: 24),
                  ]),
                ),
              ),
        );
      },
    );
  }
}
