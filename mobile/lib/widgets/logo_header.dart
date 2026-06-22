import 'package:flutter/material.dart';

/// XPRO branding shown at the top of the main screens. The logo art has a white
/// background, so we clip it to a rounded badge that reads as intentional in
/// both light and dark themes.
class LogoHeader extends StatelessWidget {
  const LogoHeader({super.key, this.height = 84});

  final double height;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 16),
      child: Center(
        child: ClipRRect(
          borderRadius: BorderRadius.circular(18),
          child: Image.asset(
            'assets/xpro-provisioning.png',
            height: height,
            filterQuality: FilterQuality.medium,
          ),
        ),
      ),
    );
  }
}
