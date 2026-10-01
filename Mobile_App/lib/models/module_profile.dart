/// A module record in the local module database (Documentation/PRD.md §11,
/// vision brief §13 "Module Database" / §14 "Add New Module"). Purely
/// local/offline metadata the technician maintains on the phone — distinct
/// from the SD-card `.INI` module profiles the firmware serves over BLE
/// (`module_selection_screen.dart` / `SELECT_MODULE`). Linking a record here
/// to a firmware SD module is manual (`sdModuleId`) since there is no BLE
/// command to write a new `.INI` file to the SD card.
class ModuleProfile {
  final String id;
  final String name;
  final String vehicleId;
  final String moduleType;
  final String communicationProtocol;
  final String canSpeed;
  final String canTxId;
  final String canRxId;
  final String klineBaud;

  /// True if the module uses 29-bit CAN IDs. Written to the SD profile's
  /// `can_extended=`; the firmware defaults to 11-bit.
  final bool canExtended;

  /// ISO-TP padding: empty = firmware default (pad with AA), a hex byte
  /// (e.g. `00`, `55`), or `none` to send short frames. ECUs differ; written
  /// to the SD profile's `can_pad_byte=` / `can_padding=`.
  final String canPadding;

  /// K-Line ECU/tester addresses as hex (e.g. `33`, `F1`).
  final String klineTarget;
  final String klineSource;
  final String minVoltage;
  final String maxVoltage;
  final String maxCurrent;
  final String positionMin;
  final String positionMax;
  final String tempMin;
  final String tempMax;
  final String relayRequirements;
  final String mosfetRequirements;
  final String testProcedure;
  final String diagnosticCommands;
  final String passCriteria;
  final String failCriteria;
  final String notes;

  /// The `.INI` filename stem on the tester's SD card this record
  /// corresponds to, if any — matches what `SELECT_MODULE:<id>` expects
  /// (see `module_selection_screen.dart`). Optional: a technician can draft
  /// a module record here before the matching SD file exists.
  final String sdModuleId;

  const ModuleProfile({
    required this.id,
    required this.name,
    this.vehicleId = '',
    this.moduleType = '',
    this.communicationProtocol = '',
    this.canSpeed = '',
    this.canTxId = '',
    this.canRxId = '',
    this.klineBaud = '',
    this.canExtended = false,
    this.canPadding = '',
    this.klineTarget = '',
    this.klineSource = '',
    this.minVoltage = '',
    this.maxVoltage = '',
    this.maxCurrent = '',
    this.positionMin = '',
    this.positionMax = '',
    this.tempMin = '',
    this.tempMax = '',
    this.relayRequirements = '',
    this.mosfetRequirements = '',
    this.testProcedure = '',
    this.diagnosticCommands = '',
    this.passCriteria = '',
    this.failCriteria = '',
    this.notes = '',
    this.sdModuleId = '',
  });

  ModuleProfile copyWith({
    String? name,
    String? vehicleId,
    String? moduleType,
    String? communicationProtocol,
    String? canSpeed,
    String? canTxId,
    String? canRxId,
    String? klineBaud,
    bool? canExtended,
    String? canPadding,
    String? klineTarget,
    String? klineSource,
    String? minVoltage,
    String? maxVoltage,
    String? maxCurrent,
    String? positionMin,
    String? positionMax,
    String? tempMin,
    String? tempMax,
    String? relayRequirements,
    String? mosfetRequirements,
    String? testProcedure,
    String? diagnosticCommands,
    String? passCriteria,
    String? failCriteria,
    String? notes,
    String? sdModuleId,
  }) =>
      ModuleProfile(
        id: id,
        name: name ?? this.name,
        vehicleId: vehicleId ?? this.vehicleId,
        moduleType: moduleType ?? this.moduleType,
        communicationProtocol:
            communicationProtocol ?? this.communicationProtocol,
        canSpeed: canSpeed ?? this.canSpeed,
        canTxId: canTxId ?? this.canTxId,
        canRxId: canRxId ?? this.canRxId,
        klineBaud: klineBaud ?? this.klineBaud,
        canExtended: canExtended ?? this.canExtended,
        canPadding: canPadding ?? this.canPadding,
        klineTarget: klineTarget ?? this.klineTarget,
        klineSource: klineSource ?? this.klineSource,
        minVoltage: minVoltage ?? this.minVoltage,
        maxVoltage: maxVoltage ?? this.maxVoltage,
        maxCurrent: maxCurrent ?? this.maxCurrent,
        positionMin: positionMin ?? this.positionMin,
        positionMax: positionMax ?? this.positionMax,
        tempMin: tempMin ?? this.tempMin,
        tempMax: tempMax ?? this.tempMax,
        relayRequirements: relayRequirements ?? this.relayRequirements,
        mosfetRequirements: mosfetRequirements ?? this.mosfetRequirements,
        testProcedure: testProcedure ?? this.testProcedure,
        diagnosticCommands: diagnosticCommands ?? this.diagnosticCommands,
        passCriteria: passCriteria ?? this.passCriteria,
        failCriteria: failCriteria ?? this.failCriteria,
        notes: notes ?? this.notes,
        sdModuleId: sdModuleId ?? this.sdModuleId,
      );

  /// Text this record should match against for the global module search.
  String get searchText => [
        name,
        moduleType,
        communicationProtocol,
        sdModuleId,
        notes,
      ].join(' ').toLowerCase();

  Map<String, dynamic> toJson() => {
        'id': id,
        'name': name,
        'vehicleId': vehicleId,
        'moduleType': moduleType,
        'communicationProtocol': communicationProtocol,
        'canSpeed': canSpeed,
        'canTxId': canTxId,
        'canRxId': canRxId,
        'klineBaud': klineBaud,
        'canExtended': canExtended,
        'canPadding': canPadding,
        'klineTarget': klineTarget,
        'klineSource': klineSource,
        'minVoltage': minVoltage,
        'maxVoltage': maxVoltage,
        'maxCurrent': maxCurrent,
        'positionMin': positionMin,
        'positionMax': positionMax,
        'tempMin': tempMin,
        'tempMax': tempMax,
        'relayRequirements': relayRequirements,
        'mosfetRequirements': mosfetRequirements,
        'testProcedure': testProcedure,
        'diagnosticCommands': diagnosticCommands,
        'passCriteria': passCriteria,
        'failCriteria': failCriteria,
        'notes': notes,
        'sdModuleId': sdModuleId,
      };

  factory ModuleProfile.fromJson(Map<String, dynamic> json) => ModuleProfile(
        id: json['id'] as String,
        name: json['name'] as String? ?? '',
        vehicleId: json['vehicleId'] as String? ?? '',
        moduleType: json['moduleType'] as String? ?? '',
        communicationProtocol: json['communicationProtocol'] as String? ?? '',
        canSpeed: json['canSpeed'] as String? ?? '',
        canTxId: json['canTxId'] as String? ?? '',
        canRxId: json['canRxId'] as String? ?? '',
        klineBaud: json['klineBaud'] as String? ?? '',
        canExtended: json['canExtended'] as bool? ?? false,
        canPadding: json['canPadding'] as String? ?? '',
        klineTarget: json['klineTarget'] as String? ?? '',
        klineSource: json['klineSource'] as String? ?? '',
        minVoltage: json['minVoltage'] as String? ?? '',
        maxVoltage: json['maxVoltage'] as String? ?? '',
        maxCurrent: json['maxCurrent'] as String? ?? '',
        positionMin: json['positionMin'] as String? ?? '',
        positionMax: json['positionMax'] as String? ?? '',
        tempMin: json['tempMin'] as String? ?? '',
        tempMax: json['tempMax'] as String? ?? '',
        relayRequirements: json['relayRequirements'] as String? ?? '',
        mosfetRequirements: json['mosfetRequirements'] as String? ?? '',
        testProcedure: json['testProcedure'] as String? ?? '',
        diagnosticCommands: json['diagnosticCommands'] as String? ?? '',
        passCriteria: json['passCriteria'] as String? ?? '',
        failCriteria: json['failCriteria'] as String? ?? '',
        notes: json['notes'] as String? ?? '',
        sdModuleId: json['sdModuleId'] as String? ?? '',
      );
}
