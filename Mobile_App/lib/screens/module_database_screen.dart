import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/module_profile.dart';
import '../state/app_state.dart';
import '../theme/app_theme.dart';
import 'module_form_screen.dart';

/// Module database list (Documentation/PRD.md §11, vision brief §13
/// "Module Database" / §14 "Add New Module"). Distinct from
/// `module_selection_screen.dart`, which lists the firmware's SD-card
/// modules over BLE — this is the technician's own local record set, usable
/// fully offline.
class ModuleDatabaseScreen extends StatefulWidget {
  const ModuleDatabaseScreen({super.key});

  @override
  State<ModuleDatabaseScreen> createState() => _ModuleDatabaseScreenState();
}

class _ModuleDatabaseScreenState extends State<ModuleDatabaseScreen> {
  String _query = '';

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final query = _query.trim().toLowerCase();
    final modules = query.isEmpty
        ? app.moduleProfiles
        : app.moduleProfiles
            .where((m) => m.searchText.contains(query))
            .toList();

    return Scaffold(
      appBar: AppBar(title: const Text('MODULES')),
      body: Column(
        children: [
          Padding(
            padding: const EdgeInsets.all(16),
            child: TextField(
              decoration: const InputDecoration(
                prefixIcon: Icon(Icons.search),
                hintText: 'Search module name, type, protocol…',
              ),
              onChanged: (v) => setState(() => _query = v),
            ),
          ),
          Expanded(
            child: modules.isEmpty
                ? Center(
                    child: Text(
                      app.moduleProfiles.isEmpty
                          ? 'No modules yet — tap + to add one.'
                          : 'No modules match your search.',
                      style: const TextStyle(color: AppColors.textSecondary),
                    ),
                  )
                : ListView.separated(
                    padding: const EdgeInsets.symmetric(horizontal: 16),
                    itemCount: modules.length,
                    separatorBuilder: (_, _) => const Divider(height: 1),
                    itemBuilder: (context, i) {
                      final module = modules[i];
                      final vehicle = app.vehicles
                          .where((v) => v.id == module.vehicleId)
                          .firstOrNull;
                      return ListTile(
                        contentPadding: EdgeInsets.zero,
                        title: Text(module.name.isEmpty
                            ? '(unnamed module)'
                            : module.name),
                        subtitle: Text(
                          [
                            if (vehicle != null) vehicle.displayName,
                            if (module.moduleType.isNotEmpty)
                              module.moduleType,
                          ].join(' · '),
                          style:
                              const TextStyle(color: AppColors.textSecondary),
                        ),
                        onTap: () => Navigator.of(context).push(
                          MaterialPageRoute(
                            builder: (_) => ModuleFormScreen(module: module),
                          ),
                        ),
                        trailing: PopupMenuButton<String>(
                          onSelected: (action) =>
                              _handleAction(context, app, module, action),
                          itemBuilder: (context) => const [
                            PopupMenuItem(
                                value: 'duplicate',
                                child: Text('Duplicate')),
                            PopupMenuItem(
                                value: 'delete', child: Text('Delete')),
                          ],
                        ),
                      );
                    },
                  ),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () => Navigator.of(context).push(
          MaterialPageRoute(builder: (_) => const ModuleFormScreen()),
        ),
        child: const Icon(Icons.add),
      ),
    );
  }

  void _handleAction(BuildContext context, AppState app, ModuleProfile module,
      String action) {
    if (action == 'duplicate') {
      app.duplicateModuleProfile(module);
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('Module duplicated')));
      return;
    }
    if (action == 'delete') {
      showDialog<void>(
        context: context,
        builder: (dialogContext) => AlertDialog(
          title: const Text('Delete module?'),
          content: Text('Are you sure you want to delete this module? '
              'This cannot be undone.'),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(dialogContext).pop(),
              child: const Text('CANCEL'),
            ),
            TextButton(
              onPressed: () {
                app.deleteModuleProfile(module.id);
                Navigator.of(dialogContext).pop();
              },
              child: const Text('DELETE',
                  style: TextStyle(color: AppColors.danger)),
            ),
          ],
        ),
      );
    }
  }
}

extension _FirstOrNull<T> on Iterable<T> {
  T? get firstOrNull => isEmpty ? null : first;
}
