import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from svgkit import Svg

s = Svg(1700, 1180, 'TIFF TESTER PRO - bench wiring (main system)',
        'Wiring diagram for the main bench: 12 V supply, fuse, DUT relay, current sensor, '
        'Arduino Nano safety controller, ESP32, SD, MCP2515 CAN, K-Line, INA219 and the device under test.')

s.text(40, 26, 'TIFF TESTER PRO - bench wiring (main system: Nano + ESP32, BLE app)', 17, 'bold')
s.text(40, 46, 'Power path shown is the recommended topology; the firmware does not fix where the sensors sit. '
               'Resistor values are examples - calibrate with the CAL_* commands.',
       11, fill='#566573', italic=True)

# ================= ROW 1: 12 V power path =================
s.text(40, 82, 'POWER PATH  (12 V, fused)', 12, 'bold', '#c0392b')

s.box('sup', 40, 90, 150, 100, '12 V BENCH SUPPLY', right=[('+12 V', 50), ('GND', 76)],
      sub='current limit 1 A to start')
sx, sy = s.pin('sup', '+12 V')
s.rect(225, sy - 12, 60, 24, fill='#fdf2e9', stroke='#c0392b', rx=4)
s.text(255, sy + 4, 'FUSE', 11, 'bold', anchor='middle')
s.wire([(sx, sy), (225, sy)], '12V', 3.4)
s.wire([(285, sy), (340, sy)], '12V', 3.4)
s.dot(340, sy, '#c0392b', 4.5)
s.flag_pin('sup', 'GND', 'GND', 'GND', 'r')

# supply divider -> Nano A1
s.box('divA', 310, 210, 130, 92, 'SUPPLY DIVIDER', left=[('IN', 54)], right=[('OUT', 54), ('GND', 74)],
      sub='30k / 10k = 4:1')
inx, iny = s.pin('divA', 'IN')
s.wire([(340, sy), (340, 185), (290, 185), (290, iny), (inx, iny)], '12V', 2.6)
s.flag_pin('divA', 'OUT', 'A1', 'AN', 'r')
s.flag_pin('divA', 'GND', 'GND', 'GND', 'r')

# relay module
s.box('rly', 580, 90, 240, 250, '4-CH RELAY MODULE', sub='active-LOW inputs (default)',
      left=[('IN1', 70), ('IN2', 110), ('IN3', 150), ('IN4', 190)],
      right=[('R1 COM', 60), ('R1 NO', 82), ('R2 COM/NO', 120), ('R3 COM/NO', 160), ('R4 COM/NO', 200)],
      bottom=[('VCC', 70), ('GND', 120)])
s.wire([(340, sy), (340, 66), (850, 66), (850, 150), (820, 150)], '12V', 3.4)
s.flag_pin('rly', 'R2 COM/NO', 'AUX LOAD 2', 'SIG', 'r')
s.flag_pin('rly', 'R3 COM/NO', 'AUX LOAD 3', 'SIG', 'r')
s.flag_pin('rly', 'R4 COM/NO', 'AUX LOAD 4', 'SIG', 'r')
s.flag_pin('rly', 'VCC', '5V', '5V', 'd')
s.flag_pin('rly', 'GND', 'GND', 'GND', 'd')
s.text(858, 120, 'relay 1 = DUT feed', 10, fill='#c0392b', italic=True)

# ACS712
s.box('acs', 960, 110, 170, 110, 'ACS712-5A', sub='current sensor (in series)',
      left=[('IP+', 62)], right=[('IP-', 62)], bottom=[('5V', 35), ('OUT', 85), ('GND', 135)])
s.wire([(820, 172), (960, 172)], '12V', 3.4)
s.flag_pin('acs', '5V', '5V', '5V', 'd')
s.flag_pin('acs', 'OUT', 'A2', 'AN', 'd')
s.flag_pin('acs', 'GND', 'GND', 'GND', 'd')

# INA219 (optional)
s.box('ina', 1190, 110, 170, 130, 'INA219 (optional)', sub='0.1 ohm shunt, addr 0x40', dashed=True,
      left=[('VIN+', 62)], right=[('VIN-', 62)],
      bottom=[('VCC', 30), ('GND', 65), ('SDA', 105), ('SCL', 140)])
s.wire([(1130, 172), (1190, 172)], '12V', 3.4)
s.flag_pin('ina', 'VCC', '3V3', '3V3', 'd')
s.flag_pin('ina', 'GND', 'GND', 'GND', 'd')
s.flag_pin('ina', 'SDA', 'SDA', 'I2C', 'd')
s.flag_pin('ina', 'SCL', 'SCL', 'I2C', 'd')

# DUT
s.box('dut', 1450, 110, 220, 225, 'DEVICE UNDER TEST', sub='dummy load first',
      left=[('V+', 62), ('GND', 90), ('CAN_H', 125), ('CAN_L', 150), ('K-LINE', 180), ('POS/SENSOR', 205)])
s.wire([(1360, 172), (1450, 172)], '12V', 3.4)
s.dot(1370, 172, '#c0392b', 4.5)
s.flag_pin('dut', 'GND', 'GND', 'GND', 'l')
s.flag_pin('dut', 'CAN_H', 'CANH', 'CAN', 'l')
s.flag_pin('dut', 'CAN_L', 'CANL', 'CAN', 'l')
s.flag_pin('dut', 'K-LINE', 'K', 'K', 'l')
s.flag_pin('dut', 'POS/SENSOR', 'POS_RAW', 'AN', 'l')

# DUT voltage divider -> Nano A0
s.box('divD', 1250, 300, 120, 92, 'DUT DIVIDER', left=[('OUT', 54)], right=[('IN', 54)],
      bottom=[('GND', 60)], sub='30k / 10k = 4:1')
s.wire([(1370, 172), s.pin('divD', 'IN')], '12V', 2.6)
s.flag_pin('divD', 'OUT', 'A0', 'AN', 'l')
s.flag_pin('divD', 'GND', 'GND', 'GND', 'd')

# ================= ROW 2: control =================
s.text(40, 408, 'CONTROL', 12, 'bold', '#2471a3')

s.box('nano', 300, 430, 220, 360, 'ARDUINO NANO', sub='safety MCU - owns the relays', top_pad=22,
      top=[('A0', 40), ('A1', 85), ('A2', 130)],
      left=[('A3', 85), ('D3', 125)],
      right=[('D4', 85), ('D5', 115), ('D7', 145), ('D8', 175), ('D6', 225), ('D1 TX', 300), ('D0 RX', 335)],
      bottom=[('5V', 30), ('GND', 70), ('D2', 110), ('D11', 150), ('D12', 190)])
s.flag_pin('nano', 'A0', 'A0', 'AN', 'u')
s.flag_pin('nano', 'A1', 'A1', 'AN', 'u')
s.flag_pin('nano', 'A2', 'A2', 'AN', 'u')
s.flag_pin('nano', '5V', '5V', '5V', 'd')
s.flag_pin('nano', 'GND', 'GND', 'GND', 'd')
s.flag_pin('nano', 'D2', 'FB2', 'SIG', 'd')
s.flag_pin('nano', 'D11', 'FB3', 'SIG', 'd')
s.flag_pin('nano', 'D12', 'FB4', 'SIG', 'd')
s.text(400, 836, 'FB2-FB4 = optional relay-feedback inputs (relays 2-4)', 9.5, fill='#566573',
       anchor='middle', italic=True)

# Nano -> relay IN1..IN4
for lab, inlab, ch in (('D4', 'IN1', 548), ('D5', 'IN2', 556), ('D7', 'IN3', 564), ('D8', 'IN4', 572)):
    x1, y1 = s.pin('nano', lab)
    x2, y2 = s.pin('rly', inlab)
    s.wire([(x1, y1), (ch, y1), (ch, y2), (x2, y2)], 'SIG', 2.2)

# LM35 -> A3
s.box('lm35', 40, 465, 180, 95, 'LM35 TEMP (optional)', sub='needed for the over-temp trip',
      right=[('OUT', 50)], bottom=[('+5V', 50), ('GND', 130)])
s.wire([s.pin('lm35', 'OUT'), s.pin('nano', 'A3')], 'AN', 2.2)
s.flag_pin('lm35', '+5V', '5V', '5V', 'd')
s.flag_pin('lm35', 'GND', 'GND', 'GND', 'd')

# E-stop -> D3
s.box('est', 40, 610, 180, 80, 'E-STOP (normally CLOSED)', sub='no e-stop? jumper D3 to GND',
      right=[('A', 45), ('B', 66)])
ax, ay = s.pin('est', 'A')
nx, ny = s.pin('nano', 'D3')
s.wire([(ax, ay), (262, ay), (262, ny), (nx, ny)], 'SIG', 2.2)
s.flag_pin('est', 'B', 'GND', 'GND', 'r')

# buzzer
s.box('buz', 590, 630, 110, 46, 'BUZZER', sub='active', left=[('+', 25)], right=[('-', 25)])
s.wire([s.pin('nano', 'D6'), s.pin('buz', '+')], 'SIG', 2.2)
s.flag_pin('buz', '-', 'GND', 'GND', 'r')

# UART divider (aligned so Nano TX runs straight in)
s.box('uart', 590, 678, 100, 70, 'UART DIV', sub='1k / 2k', left=[('IN', 52)],
      right=[('GND', 30), ('OUT', 52)])
s.flag_pin('uart', 'GND', 'GND', 'GND', 'r')

# buck
s.box('buck', 60, 830, 200, 100, '12 V -> 5 V BUCK', sub='relay coils, Nano, ESP32',
      top=[('IN+', 40), ('IN-', 80)], right=[('5V OUT', 66), ('GND', 84)], top_pad=22)
s.flag_pin('buck', 'IN+', '12V', '12V', 'u')
s.flag_pin('buck', 'IN-', 'GND', 'GND', 'u')
s.flag_pin('buck', '5V OUT', '5V', '5V', 'r')
s.flag_pin('buck', 'GND', 'GND', 'GND', 'r')

# ESP32
s.box('esp', 800, 430, 230, 470, 'ESP32 DEVKIT', sub='main controller (BLE + Wi-Fi AP)',
      left=[('GPIO16 RX2', 85), ('GPIO17 TX2', 115)],
      right=[('GPIO18 SCK', 60), ('GPIO23 MOSI', 85), ('GPIO19 MISO', 110), ('GPIO5 SD CS', 140),
             ('GPIO15 CAN CS', 165), ('GPIO4 CAN INT', 190), ('GPIO25 K-RX', 240), ('GPIO26 K-TX', 265),
             ('GPIO21 SDA', 315), ('GPIO22 SCL', 340), ('GPIO33 POS', 390)],
      bottom=[('5V/VIN', 50), ('GND', 115), ('3V3', 180)])
s.flag_pin('esp', '5V/VIN', '5V', '5V', 'd')
s.flag_pin('esp', 'GND', 'GND', 'GND', 'd')
s.flag_pin('esp', '3V3', '3V3', '3V3', 'd')
for lab, net, kind in (('GPIO18 SCK', 'SPI_SCK', 'SPI'), ('GPIO23 MOSI', 'SPI_MOSI', 'SPI'),
                       ('GPIO19 MISO', 'SPI_MISO', 'SPI'), ('GPIO5 SD CS', 'SD_CS', 'SPI'),
                       ('GPIO15 CAN CS', 'CAN_CS', 'SPI'), ('GPIO4 CAN INT', 'CAN_INT', 'SPI'),
                       ('GPIO25 K-RX', 'K_RX', 'K'), ('GPIO26 K-TX', 'K_TX', 'K'),
                       ('GPIO21 SDA', 'SDA', 'I2C'), ('GPIO22 SCL', 'SCL', 'I2C'),
                       ('GPIO33 POS', 'POS', 'AN')):
    s.flag_pin('esp', lab, net, kind, 'r')

# Nano <-> ESP32 UART
o = s.pin('uart', 'OUT')
tx = s.pin('nano', 'D1 TX')
rx = s.pin('nano', 'D0 RX')
g16 = s.pin('esp', 'GPIO16 RX2')
g17 = s.pin('esp', 'GPIO17 TX2')
s.wire([tx, s.pin('uart', 'IN')], 'UART', 2.6)
s.wire([o, (752, o[1]), (752, g16[1]), g16], 'UART', 2.6)
s.wire([g17, (784, g17[1]), (784, rx[1]), rx], 'UART', 2.6)
s.text(746, 600, 'Nano TX (5 V)', 9.5, fill='#1e8449', anchor='end', italic=True)
s.text(746, 612, 'via the divider', 9.5, fill='#1e8449', anchor='end', italic=True)
s.text(778, 786, 'ESP32 TX (3.3 V) direct', 9.5, fill='#1e8449', anchor='end', italic=True)

# modules on the right
s.box('sd', 1200, 440, 190, 150, 'microSD MODULE', sub='FAT32',
      left=[('CS', 52), ('SCK', 75), ('MOSI', 98), ('MISO', 121)], bottom=[('VCC*', 60), ('GND', 130)])
for lab, net in (('CS', 'SD_CS'), ('SCK', 'SPI_SCK'), ('MOSI', 'SPI_MOSI'), ('MISO', 'SPI_MISO')):
    s.flag_pin('sd', lab, net, 'SPI', 'l')
s.flag_pin('sd', 'VCC*', '5V', '5V', 'd')
s.flag_pin('sd', 'GND', 'GND', 'GND', 'd')

s.box('mcp', 1200, 640, 190, 200, 'MCP2515 CAN MODULE', sub='check the crystal: 8 or 16 MHz',
      left=[('CS', 52), ('INT', 75), ('SCK', 98), ('SI', 121), ('SO', 144)],
      right=[('CAN_H', 65), ('CAN_L', 90)], bottom=[('VCC*', 60), ('GND', 130)])
for lab, net, kind in (('CS', 'CAN_CS', 'SPI'), ('INT', 'CAN_INT', 'SPI'), ('SCK', 'SPI_SCK', 'SPI'),
                       ('SI', 'SPI_MOSI', 'SPI'), ('SO', 'SPI_MISO', 'SPI')):
    s.flag_pin('mcp', lab, net, kind, 'l')
s.flag_pin('mcp', 'CAN_H', 'CANH', 'CAN', 'r')
s.flag_pin('mcp', 'CAN_L', 'CANL', 'CAN', 'r')
s.flag_pin('mcp', 'VCC*', '5V', '5V', 'd')
s.flag_pin('mcp', 'GND', 'GND', 'GND', 'd')
s.text(1295, 876, '120 ohm termination at each end of the bus', 9.5, fill='#b7950b',
       anchor='middle', italic=True)

s.box('kl', 1200, 900, 190, 140, 'L9637D K-LINE', sub='ISO 9141 / 14230',
      left=[('RX out', 52), ('TX in', 77), ('VBAT', 112)], right=[('K-LINE', 52)],
      bottom=[('VCC*', 60), ('GND', 130)])
s.flag_pin('kl', 'RX out', 'K_RX', 'K', 'l')
s.flag_pin('kl', 'TX in', 'K_TX', 'K', 'l')
s.flag_pin('kl', 'VBAT', '12V', '12V', 'l')
s.flag_pin('kl', 'K-LINE', 'K', 'K', 'r')
s.flag_pin('kl', 'VCC*', '5V', '5V', 'd')
s.flag_pin('kl', 'GND', 'GND', 'GND', 'd')

s.box('posd', 1200, 1085, 190, 70, 'POSITION DIVIDER', sub='keep <= 3.3 V at GPIO33',
      left=[('OUT', 52)], right=[('IN', 52)])
s.flag_pin('posd', 'OUT', 'POS', 'AN', 'l')
s.flag_pin('posd', 'IN', 'POS_RAW', 'AN', 'r')

# ================= notes + legend =================
s.legend(40, 960)
s.note(400, 960, [
    'MUST-CHECK BEFORE POWER-UP',
    '1  Nano TX is 5 V; ESP32 RX is NOT 5 V tolerant - keep the 1k/2k divider in the UART line.',
    '2  * MCP2515, L9637D and some SD boards are 5 V logic: their SO / INT / RX outputs can put',
    '    5 V on ESP32 pins. Use 3.3 V-logic boards or level-shift those lines.',
    '3  Never connect 12 V to any GPIO. Meter every divider output before connecting it.',
    '4  E-STOP is a normally-CLOSED contact D3-GND: pressing it OR a broken wire = e-stop.',
    '    No e-stop fitted? Jumper D3 to GND or the Nano stays in ESTOP.',
    '5  Nano D0/D1 are shared with USB: unplug the UART link while uploading the Nano.',
    '6  ESP32 GPIO32/34/35 are unused by this firmware - leave them unconnected.',
    '7  All grounds common. Use a current-limited supply and a fuse in the 12 V feed.',
], 10.5, gap=16)

s.save(os.path.join(HERE, '..', 'WIRING_MAIN.svg'))
print('main ok')
