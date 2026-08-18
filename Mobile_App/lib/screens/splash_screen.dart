import 'package:flutter/material.dart';

import '../theme/app_theme.dart';
import 'root_shell.dart';

/// Documentation/UI_UX_SPEC.md §4.1.
class SplashScreen extends StatefulWidget {
  const SplashScreen({super.key});

  @override
  State<SplashScreen> createState() => _SplashScreenState();
}

class _SplashScreenState extends State<SplashScreen> {
  @override
  void initState() {
    super.initState();
    Future.delayed(const Duration(milliseconds: 1400), () {
      if (!mounted) return;
      Navigator.of(context).pushReplacement(
        MaterialPageRoute(builder: (_) => const RootShell()),
      );
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.background,
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.directions_car_filled,
                color: AppColors.brandRed, size: 96),
            const SizedBox(height: 24),
            RichText(
              text: const TextSpan(
                style: TextStyle(
                  fontSize: 32,
                  fontWeight: FontWeight.w900,
                  color: AppColors.textPrimary,
                ),
                children: [
                  TextSpan(text: 'TIFF '),
                  TextSpan(
                      text: 'TESTER',
                      style: TextStyle(color: AppColors.brandRed)),
                  TextSpan(text: ' PRO'),
                ],
              ),
            ),
            const SizedBox(height: 8),
            const Text(
              'TOTAL INJECTION & IGNITION\nDIAGNOSTIC TESTER',
              textAlign: TextAlign.center,
              style: TextStyle(
                color: AppColors.textSecondary,
                letterSpacing: 1.2,
                fontSize: 12,
              ),
            ),
            const SizedBox(height: 48),
            const SizedBox(
              width: 22,
              height: 22,
              child: CircularProgressIndicator(
                strokeWidth: 2.5,
                color: AppColors.brandRed,
              ),
            ),
            const SizedBox(height: 12),
            const Text('Initializing...',
                style: TextStyle(color: AppColors.textSecondary)),
            const SizedBox(height: 32),
            const Text('Version 0.2.0',
                style: TextStyle(color: AppColors.textSecondary, fontSize: 12)),
          ],
        ),
      ),
    );
  }
}
