import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from svgkit import Svg

s = Svg(1700, 1300, 'TIFF TESTER PRO V2 - bench wiring (Bluetooth Classic line)',
        'Wiring diagram for the V2 bench: ESP32 with the DUT relay on GPIO27, INA219, Nano V2 I2C output '
        'controller behind a level shifter, relay and MOSFET modules, SD, MCP2515 and K-Line.')

s.text(40, 26, 'TIFF TESTER PRO V2 - bench wiring (Bluetooth Classic line: ESP32 V2.2.1 + Nano V2)', 17, 'bold')
s.text(40, 46, 'Separate hardware path from the main system (see ESP32_Firmware_V2/README.md). '
               'Resistor values are examples except the supply divider, which matches the sketch (4.7037:1).',
       11, fill='#566573', italic=True)

# ================= ROW 1: power path =================
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

# supply divider -> ESP32 GPIO34 (matches SUPPLY_DIVIDER_RATIO 4.7037 = 127/27)
s.box('divS', 310, 210, 150, 92, 'SUPPLY DIVIDER', left=[('IN', 54)], right=[('OUT', 54), ('GND', 74)],
      sub='100k / 27k = 4.70:1')
inx, iny = s.pin('divS', 'IN')
s.wire([(340, sy), (340, 185), (290, 185), (290, iny), (inx, iny)], '12V', 2.6)
s.flag_pin('divS', 'OUT', 'ADC_SUP', 'AN', 'r')
s.flag_pin('divS', 'GND', 'GND', 'GND', 'r')

# DUT relay (ESP32 GPIO27, ACTIVE-HIGH)
s.box('dr', 580, 90, 230, 190, 'DUT RELAY MODULE', sub='ACTIVE-HIGH: GPIO27 HIGH = on',
      left=[('IN', 70)], right=[('COM', 60), ('NO', 82)], bottom=[('VCC', 70), ('GND', 120)])
s.flag_pin('dr', 'IN', 'DUT_RELAY', 'SIG', 'l')
s.flag_pin('dr', 'VCC', '5V', '5V', 'd')
s.flag_pin('dr', 'GND', 'GND', 'GND', 'd')
s.wire([(340, sy), (340, 66), (850, 66), (850, 150), (810, 150)], '12V', 3.4)
s.text(560, 322, 'the DUT relay is driven by the ESP32 (GPIO27), not the Nano', 10, fill='#c0392b', italic=True)

# INA219 (required)
s.box('ina', 900, 110, 170, 130, 'INA219 (required)', sub='0.1 ohm shunt, addr 0x40',
      left=[('VIN+', 62)], right=[('VIN-', 62)],
      bottom=[('VCC', 30), ('GND', 65), ('SDA', 105), ('SCL', 140)])
s.wire([(810, 172), (900, 172)], '12V', 3.4)
s.flag_pin('ina', 'VCC', '3V3', '3V3', 'd')
s.flag_pin('ina', 'GND', 'GND', 'GND', 'd')
s.flag_pin('ina', 'SDA', 'SDA', 'I2C', 'd')
s.flag_pin('ina', 'SCL', 'SCL', 'I2C', 'd')

# DUT
s.box('dut', 1300, 110, 220, 225, 'DEVICE UNDER TEST', sub='dummy load first',
      left=[('V+', 62), ('GND', 90), ('CAN_H', 125), ('CAN_L', 150), ('K-LINE', 180), ('POS/SENSOR', 205)])
s.wire([(1070, 172), (1300, 172)], '12V', 3.4)
s.flag_pin('dut', 'GND', 'GND', 'GND', 'l')
s.flag_pin('dut', 'CAN_H', 'CANH', 'CAN', 'l')
s.flag_pin('dut', 'CAN_L', 'CANL', 'CAN', 'l')
s.flag_pin('dut', 'K-LINE', 'K', 'K', 'l')
s.flag_pin('dut', 'POS/SENSOR', 'POS_RAW', 'AN', 'l')

# ================= ROW 2: control =================
s.text(40, 408, 'CONTROL', 12, 'bold', '#2471a3')

# Nano V2
s.box('nano', 430, 430, 220, 330, 'ARDUINO NANO V2', sub='I2C slave 0x12 - output controller',
      left=[('D4', 70), ('D5', 100), ('D7', 130), ('D8', 160), ('D9', 230), ('D10', 255)],
      right=[('A4 SDA', 80), ('A5 SCL', 110)],
      bottom=[('5V', 30), ('GND', 70), ('D3', 150)])
s.flag_pin('nano', '5V', '5V', '5V', 'd')
s.flag_pin('nano', 'GND', 'GND', 'GND', 'd')

# 4-ch relay module (Nano relays 1-4)
s.box('r4', 170, 430, 200, 200, '4-CH RELAY MODULE', sub='Nano relays 1-4, active-LOW',
      right=[('IN1', 70), ('IN2', 100), ('IN3', 130), ('IN4', 160)],
      left=[('R1', 70), ('R2', 100), ('R3', 130), ('R4', 160)], bottom=[('VCC', 60), ('GND', 140)])
for i, (nlab, rlab) in enumerate((('D4', 'IN1'), ('D5', 'IN2'), ('D7', 'IN3'), ('D8', 'IN4'))):
    s.wire([s.pin('nano', nlab), s.pin('r4', rlab)], 'SIG', 2.2)
for i in range(4):
    s.flag_pin('r4', f'R{i + 1}', f'AUX LOAD {i + 1}', 'SIG', 'l')
s.flag_pin('r4', 'VCC', '5V', '5V', 'd')
s.flag_pin('r4', 'GND', 'GND', 'GND', 'd')

# MOSFET module (Nano D9/D10, active-HIGH)
s.box('mos', 170, 665, 200, 130, 'MOSFET MODULE', sub='active-HIGH gates',
      right=[('G1', 55), ('G2', 80)], left=[('OUT1', 55), ('OUT2', 80)], bottom=[('GND', 100)])
s.wire([s.pin('nano', 'D9'), (400, s.pin('nano', 'D9')[1]), (400, s.pin('mos', 'G1')[1]), s.pin('mos', 'G1')],
       'SIG', 2.2)
s.wire([s.pin('nano', 'D10'), (410, s.pin('nano', 'D10')[1]), (410, s.pin('mos', 'G2')[1]), s.pin('mos', 'G2')],
       'SIG', 2.2)
s.flag_pin('mos', 'OUT1', 'LOAD M1', 'SIG', 'l')
s.flag_pin('mos', 'OUT2', 'LOAD M2', 'SIG', 'l')
s.flag_pin('mos', 'GND', 'GND', 'GND', 'd')

# E-stop on Nano D3
s.box('est', 500, 840, 190, 100, 'E-STOP (normally CLOSED)', sub='no e-stop? jumper D3 to GND',
      top=[('A', 80)], right=[('B', 75)], top_pad=22)
ax, ay = s.pin('est', 'A')
dx3, dy3 = s.pin('nano', 'D3')
s.wire([(ax, ay), (ax, 800), (dx3, 800), (dx3, dy3)], 'SIG', 2.2)
s.flag_pin('est', 'B', 'GND', 'GND', 'r')

# I2C level shifter
s.box('lvl', 720, 430, 170, 170, 'I2C LEVEL SHIFTER', sub='BSS138 type, bidirectional', top_pad=22,
      left=[('HV1', 80), ('HV2', 110)], right=[('LV1', 80), ('LV2', 110)],
      top=[('HV', 40), ('LV', 130)], bottom=[('GND', 85)])
s.wire([s.pin('nano', 'A4 SDA'), s.pin('lvl', 'HV1')], 'I2C', 2.4)
s.wire([s.pin('nano', 'A5 SCL'), s.pin('lvl', 'HV2')], 'I2C', 2.4)
s.flag_pin('lvl', 'HV', '5V', '5V', 'u')
s.flag_pin('lvl', 'LV', '3V3', '3V3', 'u')
s.flag_pin('lvl', 'GND', 'GND', 'GND', 'd')

# ESP32 V2
s.box('esp', 960, 430, 230, 500, 'ESP32 V2.2.1', sub='Bluetooth Classic "TIFF_TESTER_V2"',
      left=[('GPIO21 SDA', 80), ('GPIO22 SCL', 110)],
      right=[('GPIO18 SCK', 70), ('GPIO23 MOSI', 95), ('GPIO19 MISO', 120), ('GPIO13 SD CS', 150),
             ('GPIO5 CAN CS', 175), ('GPIO16 K-RX', 225), ('GPIO17 K-TX', 250), ('GPIO34 ADC', 310),
             ('GPIO36 POS', 340), ('GPIO39 TEMP', 370), ('GPIO27 RELAY', 420)],
      bottom=[('5V/VIN', 50), ('GND', 115), ('3V3', 180)])
s.flag_pin('esp', '5V/VIN', '5V', '5V', 'd')
s.flag_pin('esp', 'GND', 'GND', 'GND', 'd')
s.flag_pin('esp', '3V3', '3V3', '3V3', 'd')
s.wire([s.pin('lvl', 'LV1'), s.pin('esp', 'GPIO21 SDA')], 'I2C', 2.4)
s.wire([s.pin('lvl', 'LV2'), s.pin('esp', 'GPIO22 SCL')], 'I2C', 2.4)
s.dot(925, s.pin('esp', 'GPIO21 SDA')[1], '#117a65', 4)
s.flag(925, s.pin('esp', 'GPIO21 SDA')[1], 'SDA', 'I2C', 'u')
s.dot(925, s.pin('esp', 'GPIO22 SCL')[1], '#117a65', 4)
s.flag(925, s.pin('esp', 'GPIO22 SCL')[1], 'SCL', 'I2C', 'd')
for lab, net, kind in (('GPIO18 SCK', 'SPI_SCK', 'SPI'), ('GPIO23 MOSI', 'SPI_MOSI', 'SPI'),
                       ('GPIO19 MISO', 'SPI_MISO', 'SPI'), ('GPIO13 SD CS', 'SD_CS', 'SPI'),
                       ('GPIO5 CAN CS', 'CAN_CS', 'SPI'), ('GPIO16 K-RX', 'K_RX', 'K'),
                       ('GPIO17 K-TX', 'K_TX', 'K'), ('GPIO34 ADC', 'ADC_SUP', 'AN'),
                       ('GPIO36 POS', 'POS', 'AN'), ('GPIO39 TEMP', 'TEMP', 'AN'),
                       ('GPIO27 RELAY', 'DUT_RELAY', 'SIG')):
    s.flag_pin('esp', lab, net, kind, 'r')

# right-hand modules
s.box('sd', 1380, 440, 190, 150, 'microSD MODULE', sub='FAT32',
      left=[('CS', 52), ('SCK', 75), ('MOSI', 98), ('MISO', 121)], bottom=[('VCC*', 60), ('GND', 130)])
for lab, net in (('CS', 'SD_CS'), ('SCK', 'SPI_SCK'), ('MOSI', 'SPI_MOSI'), ('MISO', 'SPI_MISO')):
    s.flag_pin('sd', lab, net, 'SPI', 'l')
s.flag_pin('sd', 'VCC*', '5V', '5V', 'd')
s.flag_pin('sd', 'GND', 'GND', 'GND', 'd')

s.box('mcp', 1380, 640, 190, 180, 'MCP2515 CAN MODULE', sub='check the crystal: 8 or 16 MHz',
      left=[('CS', 52), ('SCK', 75), ('SI', 98), ('SO', 121)],
      right=[('CAN_H', 60), ('CAN_L', 85)], bottom=[('VCC*', 60), ('GND', 130)])
for lab, net in (('CS', 'CAN_CS'), ('SCK', 'SPI_SCK'), ('SI', 'SPI_MOSI'), ('SO', 'SPI_MISO')):
    s.flag_pin('mcp', lab, net, 'SPI', 'l')
s.flag_pin('mcp', 'CAN_H', 'CANH', 'CAN', 'r')
s.flag_pin('mcp', 'CAN_L', 'CANL', 'CAN', 'r')
s.flag_pin('mcp', 'VCC*', '5V', '5V', 'd')
s.flag_pin('mcp', 'GND', 'GND', 'GND', 'd')
s.text(1475, 856, '120 ohm termination at each end', 9.5, fill='#b7950b', anchor='middle', italic=True)

s.box('kl', 1380, 890, 190, 140, 'L9637D K-LINE', sub='ISO 9141 / 14230',
      left=[('RX out', 52), ('TX in', 77), ('VBAT', 112)], right=[('K-LINE', 52)],
      bottom=[('VCC*', 60), ('GND', 130)])
s.flag_pin('kl', 'RX out', 'K_RX', 'K', 'l')
s.flag_pin('kl', 'TX in', 'K_TX', 'K', 'l')
s.flag_pin('kl', 'VBAT', '12V', '12V', 'l')
s.flag_pin('kl', 'K-LINE', 'K', 'K', 'r')
s.flag_pin('kl', 'VCC*', '5V', '5V', 'd')
s.flag_pin('kl', 'GND', 'GND', 'GND', 'd')

s.box('posd', 1380, 1085, 190, 70, 'POSITION DIVIDER', sub='keep <= 3.3 V at GPIO36',
      left=[('OUT', 52)], right=[('IN', 52)])
s.flag_pin('posd', 'OUT', 'POS', 'AN', 'l')
s.flag_pin('posd', 'IN', 'POS_RAW', 'AN', 'r')

s.box('tmp', 1380, 1180, 190, 85, 'TEMP SENSOR (optional)', sub='V2 reports raw ADC volts, not C',
      left=[('OUT', 52)], right=[('GND', 52)])
s.flag_pin('tmp', 'OUT', 'TEMP', 'AN', 'l')
s.flag_pin('tmp', 'GND', 'GND', 'GND', 'r')

# buck
s.box('buck', 60, 960, 200, 100, '12 V -> 5 V BUCK', sub='relay coils, Nano, ESP32',
      top=[('IN+', 40), ('IN-', 80)], right=[('5V OUT', 66), ('GND', 84)], top_pad=22)
s.flag_pin('buck', 'IN+', '12V', '12V', 'u')
s.flag_pin('buck', 'IN-', 'GND', 'GND', 'u')
s.flag_pin('buck', '5V OUT', '5V', '5V', 'r')
s.flag_pin('buck', 'GND', 'GND', 'GND', 'r')

# ================= notes + legend =================
s.legend(40, 1100)
s.note(400, 1020, [
    'MUST-CHECK BEFORE POWER-UP',
    '1  I2C LEVELS: ESP32 is 3.3 V, the Nano is 5 V. Use the level shifter; never join SDA/SCL directly.',
    '    The Nano V2 sketch turns its internal pull-ups off; the shifter board supplies pull-ups on both sides.',
    '2  The DUT relay on GPIO27 is ACTIVE-HIGH (HIGH = on). Do NOT use an active-LOW relay module there:',
    '    it would be energised whenever the pin is LOW, including at boot and when "off".',
    '3  Nano relay modules are active-LOW; MOSFET gates are active-HIGH. All outputs are off at Nano boot.',
    '4  E-STOP is a normally-CLOSED contact D3-GND (pressed OR broken wire = e-stop); jumper D3 to GND',
    '    if none is fitted, or the Nano stays faulted and refuses every ON command.',
    '5  Supply divider 100k/27k: 15 V gives 3.19 V at GPIO34. Meter it before connecting. Never 12 V to a GPIO.',
    '6  * MCP2515 / L9637D / some SD boards are 5 V logic: their outputs can put 5 V on ESP32 pins.',
    '    Use 3.3 V-logic boards or level-shift those lines.',
    '7  INA219 measures DUT voltage AND current, and the sketch refuses PRETEST without it (about 3.2 A max).',
    '8  Pair "TIFF_TESTER_V2" in Android Bluetooth settings (no PIN) before connecting from the app.',
], 10.5, gap=16)

s.save(os.path.join(HERE, '..', 'WIRING_V2.svg'))
print('v2 ok')
