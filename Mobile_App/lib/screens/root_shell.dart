import 'package:flutter/material.dart';

import 'home_screen.dart';
import 'results_screen.dart';
import 'settings_screen.dart';
import 'tests_screen.dart';

/// Bottom-nav host for the four top-level tabs (Documentation/UI_UX_SPEC.md
/// §2/§3): Home, Tests, Results, Settings. Scan/Connect/individual test
/// screens are pushed on top of this via Navigator, not tabs themselves.
class RootShell extends StatefulWidget {
  const RootShell({super.key});

  @override
  State<RootShell> createState() => _RootShellState();
}

class _RootShellState extends State<RootShell> {
  int _index = 0;

  static const _screens = [
    HomeScreen(),
    TestsScreen(),
    ResultsScreen(),
    SettingsScreen(),
  ];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: IndexedStack(index: _index, children: _screens),
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: _index,
        onTap: (i) => setState(() => _index = i),
        items: const [
          BottomNavigationBarItem(
              icon: Icon(Icons.home_outlined), label: 'Home'),
          BottomNavigationBarItem(
              icon: Icon(Icons.science_outlined), label: 'Tests'),
          BottomNavigationBarItem(
              icon: Icon(Icons.list_alt_outlined), label: 'Results'),
          BottomNavigationBarItem(
              icon: Icon(Icons.settings_outlined), label: 'Settings'),
        ],
      ),
    );
  }
}
