import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/module_profile.dart';
import '../state/app_state.dart';
import '../theme/app_theme.dart';
import '../widgets/app_card.dart';

/// CAN / UDS / K-Line diagnostics (Documentation/DIAGNOSTICS.md). The ESP32
/// does the transport work (ISO-TP, KWP framing); this screen sends simple
/// text commands and shows the console output. Only read-oriented services
/// are accepted by the firmware — programming, security access, routine
/// control and resets are refused there regardless of what is typed here.
class DiagnosticsScreen extends StatefulWidget {
  const DiagnosticsScreen({super.key});

  @override
  State<DiagnosticsScreen> createState() => _DiagnosticsScreenState();
}

class _DiagnosticsScreenState extends State<DiagnosticsScreen> {
  late final AppState _app;

  // CAN
  int _canBitrate = 500000;
  int _canClock = 8;
  final _canTx = TextEditingController(text: '7E0');
  final _canRx = TextEditingController(text: '7E8');
  bool _canExt = false;
  bool _canMonitor = false;
  final _rawCanId = TextEditingController(text: '7E0');
  final _rawCanData = TextEditingController();

  // UDS
  String _udsSession = '03';
  final _did = TextEditingController(text: 'F190');
  final _udsRaw = TextEditingController();

  // K-Line
  int _kBaud = 10400;
  final _kTarget = TextEditingController(text: '33');
  final _kSource = TextEditingController(text: 'F1');
  bool _kMonitor = false;
  final _kwpRaw = TextEditingController();

  final _scroll = ScrollController();

  static final _hexOnly = RegExp(r'^[0-9a-fA-FxX\s,]+$');

  @override
  void initState() {
    super.initState();
    _app = context.read<AppState>();
    _prefillFromModule();
  }

  /// Pre-fills addressing from the local module record linked to the
  /// SD profile that's active on the tester, if any.
  void _prefillFromModule() {
    ModuleProfile? m;
    for (final p in _app.moduleProfiles) {
      if (p.sdModuleId.isNotEmpty && p.sdModuleId == _app.activeModuleId) {
        m = p;
        break;
      }
    }
    if (m == null) return;
    final speed = int.tryParse(m.canSpeed);
    if ([125000, 250000, 500000, 1000000].contains(speed)) _canBitrate = speed!;
    if (m.canTxId.isNotEmpty) {
      _canTx.text = m.canTxId;
      _rawCanId.text = m.canTxId;
    }
    if (m.canRxId.isNotEmpty) _canRx.text = m.canRxId;
    _canExt = m.canExtended;
    final kb = int.tryParse(m.klineBaud);
    if (kb == 9600 || kb == 10400) _kBaud = kb!;
    if (m.klineTarget.isNotEmpty) _kTarget.text = m.klineTarget;
    if (m.klineSource.isNotEmpty) _kSource.text = m.klineSource;
  }

  @override
  void dispose() {
    if (_canMonitor || _kMonitor) {
      _app.diagStop().catchError((_) {});
    }
    for (final c in [
      _canTx, _canRx, _rawCanId, _rawCanData, _did, _udsRaw, //
      _kTarget, _kSource, _kwpRaw
    ]) {
      c.dispose();
    }
    _scroll.dispose();
    super.dispose();
  }

  String _hex(TextEditingController c) =>
      c.text.trim().replaceAll(RegExp(r'0[xX]'), '');

  bool _validHex(String s) => s.isNotEmpty && _hexOnly.hasMatch(s);

  Future<void> _send(String command) async {
    try {
      await _app.sendDiag(command);
    } catch (e) {
      if (!mounted) return;
      final msg = e is StateError ? e.message : 'Failed to reach device';
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));
    }
  }

  void _sendHexCommand(String prefix, TextEditingController c) {
    final h = _hex(c);
    if (!_validHex(h)) {
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('Enter hex bytes, e.g. 22 F1 90')));
      return;
    }
    _send('$prefix:$h');
  }

  Future<bool> _confirm(String title, String body) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        title: Text(title),
        content: Text(body),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(context, false),
              child: const Text('CANCEL')),
          TextButton(
              onPressed: () => Navigator.pop(context, true),
              child: const Text('SEND')),
        ],
      ),
    );
    return ok ?? false;
  }

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final connected = app.isConnected;

    return DefaultTabController(
      length: 3,
      child: Scaffold(
        appBar: AppBar(
          title: const Text('DIAGNOSTICS'),
          actions: [
            TextButton(
              onPressed: connected
                  ? () {
                      setState(() => _canMonitor = _kMonitor = false);
                      app.diagStop();
                    }
                  : null,
              child: const Text('STOP'),
            ),
          ],
          bottom: const TabBar(tabs: [
            Tab(text: 'CAN'),
            Tab(text: 'UDS'),
            Tab(text: 'K-LINE'),
          ]),
        ),
        body: Column(
          children: [
            Expanded(
              flex: 5,
              child: TabBarView(children: [
                _scrollable(_canTab(connected)),
                _scrollable(_udsTab(connected)),
                _scrollable(_klineTab(connected)),
              ]),
            ),
            const Divider(height: 1),
            Expanded(flex: 3, child: _console(app)),
          ],
        ),
      ),
    );
  }

  Widget _scrollable(List<Widget> children) =>
      ListView(padding: const EdgeInsets.all(12), children: children);

  Widget _console(AppState app) {
    final lines = app.diagLog;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_scroll.hasClients) {
        _scroll.jumpTo(_scroll.position.maxScrollExtent);
      }
    });
    return Container(
      color: Colors.black,
      child: Stack(
        children: [
          ListView.builder(
            controller: _scroll,
            padding: const EdgeInsets.all(8),
            itemCount: lines.length,
            itemBuilder: (_, i) {
              final l = lines[i];
              return Text(
                l.display,
                style: TextStyle(
                  fontFamily: 'monospace',
                  fontSize: 12,
                  color: l.sent
                      ? AppColors.textSecondary
                      : (l.ok ? AppColors.success : AppColors.danger),
                ),
              );
            },
          ),
          Positioned(
            right: 4,
            top: 4,
            child: IconButton(
              tooltip: 'Clear console',
              icon: const Icon(Icons.delete_outline, size: 18),
              onPressed: app.clearDiagLog,
            ),
          ),
        ],
      ),
    );
  }

  Widget _row(List<Widget> kids) => Padding(
        padding: const EdgeInsets.only(top: 8),
        child: Row(children: [
          for (var i = 0; i < kids.length; i++) ...[
            if (i > 0) const SizedBox(width: 8),
            Expanded(child: kids[i]),
          ]
        ]),
      );

  Widget _btn(String label, VoidCallback? onTap) =>
      OutlinedButton(onPressed: onTap, child: Text(label));

  Widget _field(TextEditingController c, String label) => TextField(
        controller: c,
        decoration: InputDecoration(labelText: label),
        textCapitalization: TextCapitalization.characters,
      );

  Widget _note(String text) => Padding(
        padding: const EdgeInsets.only(top: 8),
        child: Text(text,
            style: const TextStyle(
                color: AppColors.textSecondary, fontSize: 12)),
      );

  // ---------------- CAN ----------------

  List<Widget> _canTab(bool connected) => [
        AppCard(
          title: 'CAN bus',
          child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: [
            _row([
              DropdownButtonFormField<int>(
                initialValue: _canBitrate,
                decoration: const InputDecoration(labelText: 'Speed (bit/s)'),
                items: const [125000, 250000, 500000, 1000000]
                    .map((v) => DropdownMenuItem(value: v, child: Text('$v')))
                    .toList(),
                onChanged: (v) => setState(() => _canBitrate = v ?? 500000),
              ),
              DropdownButtonFormField<int>(
                initialValue: _canClock,
                decoration:
                    const InputDecoration(labelText: 'MCP2515 crystal (MHz)'),
                items: const [8, 16]
                    .map((v) => DropdownMenuItem(value: v, child: Text('$v')))
                    .toList(),
                onChanged: (v) => setState(() => _canClock = v ?? 8),
              ),
            ]),
            _note('The crystal must match the one printed on your MCP2515 '
                'board, otherwise bus timing is wrong. Saved on the tester.'),
            _row([
              _btn('CAN INIT',
                  connected ? () => _send('CAN_INIT:$_canBitrate,$_canClock') : null),
            ]),
          ]),
        ),
        AppCard(
          title: 'Diagnostic addressing',
          child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: [
            _row([_field(_canTx, 'TX ID (hex)'), _field(_canRx, 'RX ID (hex)')]),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('29-bit IDs'),
              value: _canExt,
              onChanged: (v) => setState(() => _canExt = v),
            ),
            _btn(
                'APPLY IDS',
                connected
                    ? () => _send(
                        'CAN_CONFIG:${_hex(_canTx)},${_hex(_canRx)},${_canExt ? 1 : 0}')
                    : null),
          ]),
        ),
        AppCard(
          title: 'Monitor & raw transmit',
          child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: [
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('CAN monitor (RX)'),
              value: _canMonitor,
              onChanged: connected
                  ? (v) {
                      setState(() => _canMonitor = v);
                      _send('CAN_MONITOR:${v ? 1 : 0}');
                    }
                  : null,
            ),
            _row([_field(_rawCanId, 'ID (hex)'), _field(_rawCanData, 'Data (hex, ≤8 bytes)')]),
            _row([
              _btn('SEND RAW FRAME', connected ? _sendRawCan : null),
            ]),
            _note('Raw frames go straight onto the bus. Use only on a bench '
                'module, never on a vehicle network.'),
          ]),
        ),
      ];

  Future<void> _sendRawCan() async {
    final id = _hex(_rawCanId);
    final data = _hex(_rawCanData);
    if (!_validHex(id) || !_validHex(data)) {
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('ID and data must be hex')));
      return;
    }
    final ok = await _confirm('Send raw CAN frame?',
        'ID 0x$id, data $data\nThis transmits directly onto the CAN bus.');
    if (ok) _send('CAN_TX:$id,${_canExt ? 1 : 0},$data');
  }

  // ---------------- UDS ----------------

  List<Widget> _udsTab(bool connected) => [
        AppCard(
          title: 'UDS over CAN (ISO 14229)',
          child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: [
            _row([
              DropdownButtonFormField<String>(
                initialValue: _udsSession,
                decoration: const InputDecoration(labelText: 'Session'),
                items: const [
                  DropdownMenuItem(value: '01', child: Text('Default (01)')),
                  DropdownMenuItem(value: '03', child: Text('Extended (03)')),
                ],
                onChanged: (v) => setState(() => _udsSession = v ?? '03'),
              ),
              _btn('START SESSION',
                  connected ? () => _send('UDS_SESSION:$_udsSession') : null),
            ]),
            _row([
              _field(_did, 'DID (hex, e.g. F190)'),
              _btn('READ DID', connected ? () => _sendHexCommand('UDS_READ_DID', _did) : null),
            ]),
            _row([
              _btn('READ DTC', connected ? () => _send('UDS_READ_DTC') : null),
              _btn('TESTER PRESENT',
                  connected ? () => _send('UDS_TESTER_PRESENT') : null),
            ]),
            _row([
              _btn('CLEAR DTC', connected ? _clearUdsDtc : null),
            ]),
          ]),
        ),
        AppCard(
          title: 'Raw UDS request',
          child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: [
            _field(_udsRaw, 'Request bytes (hex)'),
            _row([
              _btn('SEND',
                  connected ? () => _sendHexCommand('UDS_REQUEST', _udsRaw) : null),
            ]),
            _note('The tester only allows services 10 (default/extended '
                'session), 3E, 22, 19 and 14. Programming, security access, '
                'routine control and resets are refused by the firmware.'),
          ]),
        ),
      ];

  Future<void> _clearUdsDtc() async {
    final ok = await _confirm('Clear DTCs?',
        'This erases stored fault codes on the module.');
    if (ok) _send('UDS_CLEAR_DTC');
  }

  // ---------------- K-Line ----------------

  List<Widget> _klineTab(bool connected) => [
        AppCard(
          title: 'K-Line (ISO 9141 / ISO 14230)',
          child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: [
            _row([
              DropdownButtonFormField<int>(
                initialValue: _kBaud,
                decoration: const InputDecoration(labelText: 'Baud'),
                items: const [9600, 10400]
                    .map((v) => DropdownMenuItem(value: v, child: Text('$v')))
                    .toList(),
                onChanged: (v) => setState(() => _kBaud = v ?? 10400),
              ),
              _field(_kTarget, 'ECU address (hex)'),
              _field(_kSource, 'Tester address (hex)'),
            ]),
            _row([
              _btn(
                  'APPLY',
                  connected
                      ? () => _send(
                          'KLINE_CONFIG:$_kBaud,${_hex(_kTarget)},${_hex(_kSource)}')
                      : null),
              _btn('FAST INIT', connected ? () => _send('KLINE_INIT') : null),
              _btn('5-BAUD INIT',
                  connected ? () => _send('KLINE_5BAUD_INIT') : null),
            ]),
            _note('5-baud init takes about 2.5 s. Both init methods are '
                'unvalidated against real ECUs — check with a scope first.'),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('K-Line raw monitor (RX)'),
              value: _kMonitor,
              onChanged: connected
                  ? (v) {
                      setState(() => _kMonitor = v);
                      _send('KLINE_MONITOR:${v ? 1 : 0}');
                    }
                  : null,
            ),
          ]),
        ),
        AppCard(
          title: 'KWP2000',
          child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: [
            _row([
              _btn('START SESSION',
                  connected ? () => _send('KWP_START_SESSION') : null),
              _btn('TESTER PRESENT',
                  connected ? () => _send('KWP_TESTER_PRESENT') : null),
            ]),
            _row([
              _btn('READ DTC', connected ? () => _send('KWP_READ_DTC') : null),
              _btn('CLEAR DTC', connected ? _clearKwpDtc : null),
            ]),
            const SizedBox(height: 8),
            _field(_kwpRaw, 'Raw KWP request (hex)'),
            _row([
              _btn('SEND',
                  connected ? () => _sendHexCommand('KWP_REQUEST', _kwpRaw) : null),
            ]),
            _note('Allowed: 10 81, 3E, 18, 17, 21, 1A, 14, 82. Everything '
                'else is refused by the firmware.'),
          ]),
        ),
      ];

  Future<void> _clearKwpDtc() async {
    final ok = await _confirm('Clear DTCs?',
        'This erases stored fault codes on the module.');
    if (ok) _send('KWP_CLEAR_DTC');
  }
}
