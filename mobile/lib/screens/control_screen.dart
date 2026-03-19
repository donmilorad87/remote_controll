import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import '../providers/auth_provider.dart';
import '../providers/connection_provider.dart';
import '../widgets/dpad_widget.dart';
import '../widgets/mouse_buttons.dart';
import '../widgets/volume_controls.dart';
import '../widgets/keyboard_input.dart';

class ControlScreen extends StatelessWidget {
  const ControlScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('RemoteControl'),
        centerTitle: true,
        actions: [
          // Connection status indicator
          Consumer<ConnectionProvider>(
            builder: (context, conn, _) {
              return Padding(
                padding: const EdgeInsets.only(right: 8),
                child: Icon(
                  Icons.circle,
                  size: 12,
                  color: conn.isConnected
                      ? Colors.greenAccent
                      : conn.isConnecting
                          ? Colors.orangeAccent
                          : Colors.redAccent,
                ),
              );
            },
          ),
          // Menu
          PopupMenuButton<String>(
            onSelected: (value) async {
              if (value == 'reconnect') {
                context.read<ConnectionProvider>().reconnect();
              } else if (value == 'logout') {
                context.read<ConnectionProvider>().disconnect();
                await context.read<AuthProvider>().logout();
              }
            },
            itemBuilder: (_) => [
              const PopupMenuItem(value: 'reconnect', child: Text('Reconnect')),
              const PopupMenuItem(value: 'logout', child: Text('Logout')),
            ],
          ),
        ],
      ),
      body: const SafeArea(
        child: SingleChildScrollView(
          padding: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
          child: Column(
            children: [
              // D-Pad for cursor control
              DPadWidget(),
              SizedBox(height: 16),

              // Mouse buttons row
              MouseButtonsWidget(),
              SizedBox(height: 16),

              // Scroll + Volume row
              Row(
                children: [
                  Expanded(child: VolumeControlsWidget()),
                ],
              ),
              SizedBox(height: 16),

              // Keyboard input
              KeyboardInputWidget(),
              SizedBox(height: 24),
            ],
          ),
        ),
      ),
    );
  }
}
