/// A vehicle record in the local vehicle database (Documentation/PRD.md §11,
/// vision brief §15 "Vehicle Database"). Purely local/offline — distinct
/// from the SD-card module profiles the firmware serves over BLE
/// (`module_selection_screen.dart`); this is metadata the technician
/// maintains on the phone to organize which module profile belongs to
/// which vehicle.
class Vehicle {
  final String id;
  final String manufacturer;
  final String model;
  final String year;
  final String engine;
  final String fuelType;
  final String engineCode;
  final String ecuInfo;
  final String protocol;
  final String notes;

  const Vehicle({
    required this.id,
    required this.manufacturer,
    required this.model,
    required this.year,
    this.engine = '',
    this.fuelType = '',
    this.engineCode = '',
    this.ecuInfo = '',
    this.protocol = '',
    this.notes = '',
  });

  Vehicle copyWith({
    String? manufacturer,
    String? model,
    String? year,
    String? engine,
    String? fuelType,
    String? engineCode,
    String? ecuInfo,
    String? protocol,
    String? notes,
  }) =>
      Vehicle(
        id: id,
        manufacturer: manufacturer ?? this.manufacturer,
        model: model ?? this.model,
        year: year ?? this.year,
        engine: engine ?? this.engine,
        fuelType: fuelType ?? this.fuelType,
        engineCode: engineCode ?? this.engineCode,
        ecuInfo: ecuInfo ?? this.ecuInfo,
        protocol: protocol ?? this.protocol,
        notes: notes ?? this.notes,
      );

  String get displayName {
    final yearSuffix = year.isNotEmpty ? ' ($year)' : '';
    return '$manufacturer $model$yearSuffix'.trim();
  }

  /// Text this record should match against for the global vehicle search.
  String get searchText =>
      '$manufacturer $model $year $engine $fuelType $engineCode $ecuInfo $protocol $notes'
          .toLowerCase();

  Map<String, dynamic> toJson() => {
        'id': id,
        'manufacturer': manufacturer,
        'model': model,
        'year': year,
        'engine': engine,
        'fuelType': fuelType,
        'engineCode': engineCode,
        'ecuInfo': ecuInfo,
        'protocol': protocol,
        'notes': notes,
      };

  factory Vehicle.fromJson(Map<String, dynamic> json) => Vehicle(
        id: json['id'] as String,
        manufacturer: json['manufacturer'] as String? ?? '',
        model: json['model'] as String? ?? '',
        year: json['year'] as String? ?? '',
        engine: json['engine'] as String? ?? '',
        fuelType: json['fuelType'] as String? ?? '',
        engineCode: json['engineCode'] as String? ?? '',
        ecuInfo: json['ecuInfo'] as String? ?? '',
        protocol: json['protocol'] as String? ?? '',
        notes: json['notes'] as String? ?? '',
      );
}
