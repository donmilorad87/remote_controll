import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'providers/remote_provider.dart';
import 'screens/discovery_screen.dart';

void main() {
  runApp(const RemoteControlApp());
}

class RemoteControlApp extends StatelessWidget {
  const RemoteControlApp({super.key});

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (_) => RemoteProvider(),
      child: MaterialApp(
        title: 'RemoteControl',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(
          colorSchemeSeed: const Color(0xFF3B82F6),
          useMaterial3: true,
          brightness: Brightness.dark,
        ),
        home: const DiscoveryScreen(),
      ),
    );
  }
}
