import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/vehicle.dart';
import '../state/app_state.dart';

/// Add/edit form for a Vehicle database record (Documentation/PRD.md §11,
/// vision brief §15 "Vehicle Database").
class VehicleFormScreen extends StatefulWidget {
  /// Null when adding a new vehicle; set when editing an existing one.
  final Vehicle? vehicle;

  const VehicleFormScreen({super.key, this.vehicle});

  @override
  State<VehicleFormScreen> createState() => _VehicleFormScreenState();
}

class _VehicleFormScreenState extends State<VehicleFormScreen> {
  final _formKey = GlobalKey<FormState>();

  late final TextEditingController _manufacturer;
  late final TextEditingController _model;
  late final TextEditingController _year;
  late final TextEditingController _engine;
  late final TextEditingController _fuelType;
  late final TextEditingController _engineCode;
  late final TextEditingController _ecuInfo;
  late final TextEditingController _protocol;
  late final TextEditingController _notes;

  bool get _editing => widget.vehicle != null;

  @override
  void initState() {
    super.initState();
    final v = widget.vehicle;
    _manufacturer = TextEditingController(text: v?.manufacturer ?? '');
    _model = TextEditingController(text: v?.model ?? '');
    _year = TextEditingController(text: v?.year ?? '');
    _engine = TextEditingController(text: v?.engine ?? '');
    _fuelType = TextEditingController(text: v?.fuelType ?? '');
    _engineCode = TextEditingController(text: v?.engineCode ?? '');
    _ecuInfo = TextEditingController(text: v?.ecuInfo ?? '');
    _protocol = TextEditingController(text: v?.protocol ?? '');
    _notes = TextEditingController(text: v?.notes ?? '');
  }

  @override
  void dispose() {
    _manufacturer.dispose();
    _model.dispose();
    _year.dispose();
    _engine.dispose();
    _fuelType.dispose();
    _engineCode.dispose();
    _ecuInfo.dispose();
    _protocol.dispose();
    _notes.dispose();
    super.dispose();
  }

  void _save() {
    if (!_formKey.currentState!.validate()) return;
    final app = context.read<AppState>();

    if (_editing) {
      app.updateVehicle(widget.vehicle!.copyWith(
        manufacturer: _manufacturer.text.trim(),
        model: _model.text.trim(),
        year: _year.text.trim(),
        engine: _engine.text.trim(),
        fuelType: _fuelType.text.trim(),
        engineCode: _engineCode.text.trim(),
        ecuInfo: _ecuInfo.text.trim(),
        protocol: _protocol.text.trim(),
        notes: _notes.text.trim(),
      ));
    } else {
      app.addVehicle(Vehicle(
        id: DateTime.now().microsecondsSinceEpoch.toString(),
        manufacturer: _manufacturer.text.trim(),
        model: _model.text.trim(),
        year: _year.text.trim(),
        engine: _engine.text.trim(),
        fuelType: _fuelType.text.trim(),
        engineCode: _engineCode.text.trim(),
        ecuInfo: _ecuInfo.text.trim(),
        protocol: _protocol.text.trim(),
        notes: _notes.text.trim(),
      ));
    }
    Navigator.of(context).pop();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(_editing ? 'EDIT VEHICLE' : 'ADD VEHICLE')),
      body: Form(
        key: _formKey,
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            TextFormField(
              controller: _manufacturer,
              decoration: const InputDecoration(labelText: 'Manufacturer *'),
              validator: (v) =>
                  (v == null || v.trim().isEmpty) ? 'Required' : null,
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _model,
              decoration: const InputDecoration(labelText: 'Model *'),
              validator: (v) =>
                  (v == null || v.trim().isEmpty) ? 'Required' : null,
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _year,
              decoration: const InputDecoration(labelText: 'Year'),
              keyboardType: TextInputType.number,
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _engine,
              decoration: const InputDecoration(labelText: 'Engine'),
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _fuelType,
              decoration: const InputDecoration(labelText: 'Fuel type'),
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _engineCode,
              decoration: const InputDecoration(labelText: 'Engine code'),
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _ecuInfo,
              decoration:
                  const InputDecoration(labelText: 'ECU / module information'),
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _protocol,
              decoration: const InputDecoration(
                  labelText: 'Communication protocol (CAN / K-Line / …)'),
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _notes,
              decoration: const InputDecoration(labelText: 'Notes'),
              maxLines: 3,
            ),
            const SizedBox(height: 24),
            ElevatedButton(
              onPressed: _save,
              child: Text(_editing ? 'SAVE CHANGES' : 'SAVE VEHICLE'),
            ),
          ],
        ),
      ),
    );
  }
}
