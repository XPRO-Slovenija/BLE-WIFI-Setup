import 'package:flutter/material.dart';

import 'screens/scan_screen.dart';
import 'settings_controller.dart';
import 'theme.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final settings = await SettingsController.load();
  runApp(RpiProvisionerApp(settings: settings));
}

class RpiProvisionerApp extends StatelessWidget {
  const RpiProvisionerApp({super.key, required this.settings});

  final SettingsController settings;

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: settings,
      builder: (context, _) => MaterialApp(
        title: 'Pi WiFi Setup',
        debugShowCheckedModeBanner: false,
        theme: lightTheme,
        darkTheme: darkTheme,
        themeMode: settings.themeMode,
        home: ScanScreen(settings: settings),
      ),
    );
  }
}
