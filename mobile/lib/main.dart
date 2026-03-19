import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:flutter_dotenv/flutter_dotenv.dart';
import 'providers/auth_provider.dart';
import 'providers/connection_provider.dart';
import 'screens/login_screen.dart';
import 'screens/control_screen.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await dotenv.load(fileName: '.env');
  runApp(const RemoteControlApp());
}

class RemoteControlApp extends StatelessWidget {
  const RemoteControlApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => AuthProvider()),
        ChangeNotifierProxyProvider<AuthProvider, ConnectionProvider>(
          create: (_) => ConnectionProvider(),
          update: (_, auth, conn) => conn!..updateAuth(auth),
        ),
      ],
      child: MaterialApp(
        title: 'RemoteControl',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(
          colorSchemeSeed: const Color(0xFF3B82F6),
          useMaterial3: true,
          brightness: Brightness.dark,
        ),
        home: Consumer<AuthProvider>(
          builder: (context, auth, _) {
            if (auth.isAuthenticated) {
              return const ControlScreen();
            }
            return const LoginScreen();
          },
        ),
      ),
    );
  }
}
