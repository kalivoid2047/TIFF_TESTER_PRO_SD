import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../state/app_state.dart';
import '../theme/app_theme.dart';

/// Documentation/ROADMAP.md Phase 3 gap: the app could already send
/// `SELECT_MODULE` but had no UI to pick from the SD card's module list.
/// Reads the BLE modules characteristic (`readModuleList()` in
/// tiff_ble_service.dart) to populate the list.
class ModuleSelectionScreen extends StatefulWidget {
  const ModuleSelectionScreen({super.key});

  @override
  State<ModuleSelectionScreen> createState() => _ModuleSelectionScreenState();
}

class _ModuleSelectionScreenState extends State<ModuleSelectionScreen> {
  late Future<List<String>> _modules;
  String? _selecting;

  @override
  void initState() {
    super.initState();
    _modules = context.read<AppState>().fetchModuleList();
  }

  Future<void> _refresh() async {
    final future = context.read<AppState>().fetchModuleList();
    setState(() => _modules = future);
    await future;
  }

  Future<void> _select(String moduleId) async {
    setState(() => _selecting = moduleId);
    final app = context.read<AppState>();
    try {
      await app.selectModule(moduleId);
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Selected module: $moduleId')));
        Navigator.of(context).pop();
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text('Failed to select: $e')));
      }
    } finally {
      if (mounted) setState(() => _selecting = null);
    }
  }

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();

    return Scaffold(
      appBar: AppBar(
        title: const Text('SELECT MODULE'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: _refresh,
          ),
        ],
      ),
      body: !app.isConnected
          ? const Center(
              child: Text('Connect to a device first.',
                  style: TextStyle(color: AppColors.textSecondary)))
          : FutureBuilder<List<String>>(
              future: _modules,
              builder: (context, snapshot) {
                if (snapshot.connectionState != ConnectionState.done) {
                  return const Center(child: CircularProgressIndicator());
                }
                if (snapshot.hasError) {
                  return Center(
                    child: Text('Failed to load modules: ${snapshot.error}',
                        style: const TextStyle(color: AppColors.danger)),
                  );
                }
                final modules = snapshot.data ?? const [];
                if (modules.isEmpty) {
                  return const Center(
                    child: Text('No module profiles found on the SD card.',
                        style: TextStyle(color: AppColors.textSecondary)),
                  );
                }
                return ListView.separated(
                  padding: const EdgeInsets.all(16),
                  itemCount: modules.length,
                  separatorBuilder: (_, __) => const SizedBox(height: 8),
                  itemBuilder: (context, i) {
                    final id = modules[i];
                    final active = id == app.activeModuleId;
                    return Card(
                      child: ListTile(
                        title: Text(id),
                        trailing: _selecting == id
                            ? const SizedBox(
                                width: 20,
                                height: 20,
                                child: CircularProgressIndicator(strokeWidth: 2),
                              )
                            : active
                                ? const Icon(Icons.check_circle,
                                    color: AppColors.success)
                                : null,
                        onTap: _selecting == null ? () => _select(id) : null,
                      ),
                    );
                  },
                );
              },
            ),
    );
  }
}
