import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/module_profile.dart';
import '../state/app_state.dart';
import '../theme/app_theme.dart';

/// Add/edit form for a Module database record (Documentation/PRD.md §11,
/// vision brief §13 "Module Database" / §14 "Add New Module"). Covers the
/// field list from brief §13 that maps to this app's offline metadata
/// record (CAN/K-Line config, limits, test procedure/criteria) — separate
/// from actually pushing a `.INI` file to the tester's SD card, which has
/// no BLE command yet (see `sdModuleId` doc comment on `ModuleProfile`).
class ModuleFormScreen extends StatefulWidget {
  final ModuleProfile? module;

  const ModuleFormScreen({super.key, this.module});

  @override
  State<ModuleFormScreen> createState() => _ModuleFormScreenState();
}

class _ModuleFormScreenState extends State<ModuleFormScreen> {
  final _formKey = GlobalKey<FormState>();

  late final TextEditingController _name;
  late final TextEditingController _moduleType;
  late final TextEditingController _protocol;
  late final TextEditingController _canSpeed;
  late final TextEditingController _canTxId;
  late final TextEditingController _canRxId;
  late final TextEditingController _klineBaud;
  late final TextEditingController _klineTarget;
  late final TextEditingController _klineSource;
  late final TextEditingController _minVoltage;
  late final TextEditingController _maxVoltage;
  late final TextEditingController _maxCurrent;
  late final TextEditingController _positionMin;
  late final TextEditingController _positionMax;
  late final TextEditingController _tempMin;
  late final TextEditingController _tempMax;
  late final TextEditingController _relayRequirements;
  late final TextEditingController _mosfetRequirements;
  late final TextEditingController _testProcedure;
  late final TextEditingController _diagnosticCommands;
  late final TextEditingController _passCriteria;
  late final TextEditingController _failCriteria;
  late final TextEditingController _notes;
  late final TextEditingController _sdModuleId;

  String? _vehicleId;
  bool _canExtended = false;

  bool get _editing => widget.module != null;

  @override
  void initState() {
    super.initState();
    final m = widget.module;
    _name = TextEditingController(text: m?.name ?? '');
    _moduleType = TextEditingController(text: m?.moduleType ?? '');
    _protocol = TextEditingController(text: m?.communicationProtocol ?? '');
    _canSpeed = TextEditingController(text: m?.canSpeed ?? '');
    _canTxId = TextEditingController(text: m?.canTxId ?? '');
    _canRxId = TextEditingController(text: m?.canRxId ?? '');
    _klineBaud = TextEditingController(text: m?.klineBaud ?? '');
    _klineTarget = TextEditingController(text: m?.klineTarget ?? '');
    _klineSource = TextEditingController(text: m?.klineSource ?? '');
    _canExtended = m?.canExtended ?? false;
    _minVoltage = TextEditingController(text: m?.minVoltage ?? '');
    _maxVoltage = TextEditingController(text: m?.maxVoltage ?? '');
    _maxCurrent = TextEditingController(text: m?.maxCurrent ?? '');
    _positionMin = TextEditingController(text: m?.positionMin ?? '');
    _positionMax = TextEditingController(text: m?.positionMax ?? '');
    _tempMin = TextEditingController(text: m?.tempMin ?? '');
    _tempMax = TextEditingController(text: m?.tempMax ?? '');
    _relayRequirements =
        TextEditingController(text: m?.relayRequirements ?? '');
    _mosfetRequirements =
        TextEditingController(text: m?.mosfetRequirements ?? '');
    _testProcedure = TextEditingController(text: m?.testProcedure ?? '');
    _diagnosticCommands =
        TextEditingController(text: m?.diagnosticCommands ?? '');
    _passCriteria = TextEditingController(text: m?.passCriteria ?? '');
    _failCriteria = TextEditingController(text: m?.failCriteria ?? '');
    _notes = TextEditingController(text: m?.notes ?? '');
    _sdModuleId = TextEditingController(text: m?.sdModuleId ?? '');
    _vehicleId = m?.vehicleId.isNotEmpty == true ? m!.vehicleId : null;
  }

  @override
  void dispose() {
    _name.dispose();
    _moduleType.dispose();
    _protocol.dispose();
    _canSpeed.dispose();
    _canTxId.dispose();
    _canRxId.dispose();
    _klineBaud.dispose();
    _klineTarget.dispose();
    _klineSource.dispose();
    _minVoltage.dispose();
    _maxVoltage.dispose();
    _maxCurrent.dispose();
    _positionMin.dispose();
    _positionMax.dispose();
    _tempMin.dispose();
    _tempMax.dispose();
    _relayRequirements.dispose();
    _mosfetRequirements.dispose();
    _testProcedure.dispose();
    _diagnosticCommands.dispose();
    _passCriteria.dispose();
    _failCriteria.dispose();
    _notes.dispose();
    _sdModuleId.dispose();
    super.dispose();
  }

  void _save() {
    if (!_formKey.currentState!.validate()) return;
    final app = context.read<AppState>();

    if (_editing) {
      app.updateModuleProfile(widget.module!.copyWith(
        name: _name.text.trim(),
        vehicleId: _vehicleId ?? '',
        moduleType: _moduleType.text.trim(),
        communicationProtocol: _protocol.text.trim(),
        canSpeed: _canSpeed.text.trim(),
        canTxId: _canTxId.text.trim(),
        canRxId: _canRxId.text.trim(),
        klineBaud: _klineBaud.text.trim(),
        canExtended: _canExtended,
        klineTarget: _klineTarget.text.trim(),
        klineSource: _klineSource.text.trim(),
        minVoltage: _minVoltage.text.trim(),
        maxVoltage: _maxVoltage.text.trim(),
        maxCurrent: _maxCurrent.text.trim(),
        positionMin: _positionMin.text.trim(),
        positionMax: _positionMax.text.trim(),
        tempMin: _tempMin.text.trim(),
        tempMax: _tempMax.text.trim(),
        relayRequirements: _relayRequirements.text.trim(),
        mosfetRequirements: _mosfetRequirements.text.trim(),
        testProcedure: _testProcedure.text.trim(),
        diagnosticCommands: _diagnosticCommands.text.trim(),
        passCriteria: _passCriteria.text.trim(),
        failCriteria: _failCriteria.text.trim(),
        notes: _notes.text.trim(),
        sdModuleId: _sdModuleId.text.trim(),
      ));
    } else {
      app.addModuleProfile(ModuleProfile(
        id: DateTime.now().microsecondsSinceEpoch.toString(),
        name: _name.text.trim(),
        vehicleId: _vehicleId ?? '',
        moduleType: _moduleType.text.trim(),
        communicationProtocol: _protocol.text.trim(),
        canSpeed: _canSpeed.text.trim(),
        canTxId: _canTxId.text.trim(),
        canRxId: _canRxId.text.trim(),
        klineBaud: _klineBaud.text.trim(),
        canExtended: _canExtended,
        klineTarget: _klineTarget.text.trim(),
        klineSource: _klineSource.text.trim(),
        minVoltage: _minVoltage.text.trim(),
        maxVoltage: _maxVoltage.text.trim(),
        maxCurrent: _maxCurrent.text.trim(),
        positionMin: _positionMin.text.trim(),
        positionMax: _positionMax.text.trim(),
        tempMin: _tempMin.text.trim(),
        tempMax: _tempMax.text.trim(),
        relayRequirements: _relayRequirements.text.trim(),
        mosfetRequirements: _mosfetRequirements.text.trim(),
        testProcedure: _testProcedure.text.trim(),
        diagnosticCommands: _diagnosticCommands.text.trim(),
        passCriteria: _passCriteria.text.trim(),
        failCriteria: _failCriteria.text.trim(),
        notes: _notes.text.trim(),
        sdModuleId: _sdModuleId.text.trim(),
      ));
    }
    Navigator.of(context).pop();
  }

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();

    return Scaffold(
      appBar: AppBar(title: Text(_editing ? 'EDIT MODULE' : 'ADD NEW MODULE')),
      body: Form(
        key: _formKey,
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            TextFormField(
              controller: _name,
              decoration: const InputDecoration(labelText: 'Module name *'),
              validator: (v) =>
                  (v == null || v.trim().isEmpty) ? 'Required' : null,
            ),
            const SizedBox(height: 12),
            DropdownButtonFormField<String>(
              initialValue: _vehicleId,
              decoration: const InputDecoration(labelText: 'Vehicle'),
              items: [
                const DropdownMenuItem<String>(
                    value: null, child: Text('(none)')),
                ...app.vehicles.map((v) => DropdownMenuItem<String>(
                      value: v.id,
                      child: Text(v.displayName),
                    )),
              ],
              onChanged: (v) => setState(() => _vehicleId = v),
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _moduleType,
              decoration: const InputDecoration(
                  labelText: 'Module type (e.g. Turbo Actuator)'),
            ),
            const SizedBox(height: 20),
            const _SectionLabel('COMMUNICATION'),
            TextFormField(
              controller: _protocol,
              decoration: const InputDecoration(
                  labelText: 'Protocol (CAN / K-Line / …)'),
            ),
            const SizedBox(height: 12),
            Row(children: [
              Expanded(
                child: TextFormField(
                  controller: _canSpeed,
                  decoration: const InputDecoration(labelText: 'CAN speed'),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: TextFormField(
                  controller: _klineBaud,
                  decoration:
                      const InputDecoration(labelText: 'K-Line baud'),
                ),
              ),
            ]),
            const SizedBox(height: 12),
            Row(children: [
              Expanded(
                child: TextFormField(
                  controller: _canTxId,
                  decoration: const InputDecoration(labelText: 'CAN TX ID'),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: TextFormField(
                  controller: _canRxId,
                  decoration: const InputDecoration(labelText: 'CAN RX ID'),
                ),
              ),
            ]),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('29-bit (extended) CAN IDs'),
              value: _canExtended,
              onChanged: (v) => setState(() => _canExtended = v),
            ),
            Row(children: [
              Expanded(
                child: TextFormField(
                  controller: _klineTarget,
                  decoration: const InputDecoration(
                      labelText: 'K-Line ECU address (hex)'),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: TextFormField(
                  controller: _klineSource,
                  decoration: const InputDecoration(
                      labelText: 'K-Line tester address (hex)'),
                ),
              ),
            ]),
            const SizedBox(height: 20),
            const _SectionLabel('LIMITS'),
            Row(children: [
              Expanded(
                child: TextFormField(
                  controller: _minVoltage,
                  decoration:
                      const InputDecoration(labelText: 'Min voltage'),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: TextFormField(
                  controller: _maxVoltage,
                  decoration:
                      const InputDecoration(labelText: 'Max voltage'),
                ),
              ),
            ]),
            const SizedBox(height: 12),
            TextFormField(
              controller: _maxCurrent,
              decoration: const InputDecoration(labelText: 'Max current'),
            ),
            const SizedBox(height: 12),
            Row(children: [
              Expanded(
                child: TextFormField(
                  controller: _positionMin,
                  decoration:
                      const InputDecoration(labelText: 'Position min'),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: TextFormField(
                  controller: _positionMax,
                  decoration:
                      const InputDecoration(labelText: 'Position max'),
                ),
              ),
            ]),
            const SizedBox(height: 12),
            Row(children: [
              Expanded(
                child: TextFormField(
                  controller: _tempMin,
                  decoration:
                      const InputDecoration(labelText: 'Temperature min'),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: TextFormField(
                  controller: _tempMax,
                  decoration:
                      const InputDecoration(labelText: 'Temperature max'),
                ),
              ),
            ]),
            const SizedBox(height: 20),
            const _SectionLabel('HARDWARE REQUIREMENTS'),
            TextFormField(
              controller: _relayRequirements,
              decoration:
                  const InputDecoration(labelText: 'Relay requirements'),
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _mosfetRequirements,
              decoration:
                  const InputDecoration(labelText: 'MOSFET requirements'),
            ),
            const SizedBox(height: 20),
            const _SectionLabel('TEST PROCEDURE'),
            TextFormField(
              controller: _testProcedure,
              decoration: const InputDecoration(
                  labelText: 'Test procedure (step-by-step notes)'),
              maxLines: 4,
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _diagnosticCommands,
              decoration:
                  const InputDecoration(labelText: 'Diagnostic commands'),
              maxLines: 2,
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _passCriteria,
              decoration: const InputDecoration(labelText: 'Pass criteria'),
            ),
            const SizedBox(height: 12),
            TextFormField(
              controller: _failCriteria,
              decoration: const InputDecoration(labelText: 'Fail criteria'),
            ),
            const SizedBox(height: 20),
            const _SectionLabel('OTHER'),
            TextFormField(
              controller: _sdModuleId,
              decoration: const InputDecoration(
                labelText: 'SD module ID (matches tester\'s .INI filename)',
              ),
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
              child: Text(_editing ? 'SAVE CHANGES' : 'SAVE MODULE'),
            ),
          ],
        ),
      ),
    );
  }
}

class _SectionLabel extends StatelessWidget {
  final String text;
  const _SectionLabel(this.text);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: Text(
        text,
        style: const TextStyle(
          fontWeight: FontWeight.bold,
          fontSize: 12,
          letterSpacing: 0.5,
          color: AppColors.textSecondary,
        ),
      ),
    );
  }
}
