import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/vehicle.dart';
import '../state/app_state.dart';
import '../theme/app_theme.dart';
import 'vehicle_form_screen.dart';

/// Vehicle database list (Documentation/PRD.md §11, vision brief §15
/// "Vehicle Database" / §26 "Search"): search, add, edit, duplicate,
/// delete-with-confirmation.
class VehicleListScreen extends StatefulWidget {
  const VehicleListScreen({super.key});

  @override
  State<VehicleListScreen> createState() => _VehicleListScreenState();
}

class _VehicleListScreenState extends State<VehicleListScreen> {
  String _query = '';

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final query = _query.trim().toLowerCase();
    final vehicles = query.isEmpty
        ? app.vehicles
        : app.vehicles.where((v) => v.searchText.contains(query)).toList();

    return Scaffold(
      appBar: AppBar(title: const Text('VEHICLES')),
      body: Column(
        children: [
          Padding(
            padding: const EdgeInsets.all(16),
            child: TextField(
              decoration: const InputDecoration(
                prefixIcon: Icon(Icons.search),
                hintText: 'Search manufacturer, model, year, engine…',
              ),
              onChanged: (v) => setState(() => _query = v),
            ),
          ),
          Expanded(
            child: vehicles.isEmpty
                ? Center(
                    child: Text(
                      app.vehicles.isEmpty
                          ? 'No vehicles yet — tap + to add one.'
                          : 'No vehicles match your search.',
                      style: const TextStyle(color: AppColors.textSecondary),
                    ),
                  )
                : ListView.separated(
                    padding: const EdgeInsets.symmetric(horizontal: 16),
                    itemCount: vehicles.length,
                    separatorBuilder: (_, _) => const Divider(height: 1),
                    itemBuilder: (context, i) {
                      final vehicle = vehicles[i];
                      return ListTile(
                        contentPadding: EdgeInsets.zero,
                        title: Text(vehicle.displayName.isEmpty
                            ? '(unnamed vehicle)'
                            : vehicle.displayName),
                        subtitle: vehicle.engine.isNotEmpty
                            ? Text(vehicle.engine,
                                style: const TextStyle(
                                    color: AppColors.textSecondary))
                            : null,
                        onTap: () => Navigator.of(context).push(
                          MaterialPageRoute(
                            builder: (_) =>
                                VehicleFormScreen(vehicle: vehicle),
                          ),
                        ),
                        trailing: PopupMenuButton<String>(
                          onSelected: (action) =>
                              _handleAction(context, app, vehicle, action),
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
          MaterialPageRoute(builder: (_) => const VehicleFormScreen()),
        ),
        child: const Icon(Icons.add),
      ),
    );
  }

  void _handleAction(
      BuildContext context, AppState app, Vehicle vehicle, String action) {
    if (action == 'duplicate') {
      app.duplicateVehicle(vehicle);
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('Vehicle duplicated')));
      return;
    }
    if (action == 'delete') {
      showDialog<void>(
        context: context,
        builder: (dialogContext) => AlertDialog(
          title: const Text('Delete vehicle?'),
          content: Text(
              'Are you sure you want to delete "${vehicle.displayName}"? '
              'This cannot be undone.'),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(dialogContext).pop(),
              child: const Text('CANCEL'),
            ),
            TextButton(
              onPressed: () {
                app.deleteVehicle(vehicle.id);
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
