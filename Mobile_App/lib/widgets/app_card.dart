import 'package:flutter/material.dart';

import '../theme/app_theme.dart';

/// The bordered "card" grouping used throughout the reference screenshots
/// (Connection card, ECU Control card, Quick Tests card, etc. — see
/// Documentation/UI_UX_SPEC.md §1).
class AppCard extends StatelessWidget {
  final String? title;
  final Widget child;

  const AppCard({super.key, this.title, required this.child});

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            if (title != null) ...[
              Text(
                title!.toUpperCase(),
                style: const TextStyle(
                  fontWeight: FontWeight.bold,
                  fontSize: 13,
                  letterSpacing: 0.5,
                  color: AppColors.textSecondary,
                ),
              ),
              const SizedBox(height: 12),
            ],
            child,
          ],
        ),
      ),
    );
  }
}

/// A "Label ................ Value" row, e.g. "Status" / "CONNECTED".
class StatusRow extends StatelessWidget {
  final String label;
  final String value;
  final Color? valueColor;

  const StatusRow({
    super.key,
    required this.label,
    required this.value,
    this.valueColor,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(color: AppColors.textSecondary)),
          Text(
            value,
            style: TextStyle(
              fontWeight: FontWeight.bold,
              color: valueColor ?? AppColors.textPrimary,
            ),
          ),
        ],
      ),
    );
  }
}
