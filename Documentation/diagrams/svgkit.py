# Regenerate the wiring diagrams with:  python make_main.py  and  python make_v2.py
"""Tiny SVG helper for wiring diagrams: boxes with named pins, wires with
junction dots, and net-label flags. Colours follow the signal type."""
from xml.sax.saxutils import escape

COL = {
    '12V': '#c0392b', '5V': '#e67e22', '3V3': '#d35400', 'GND': '#2c3e50',
    'SIG': '#2471a3', 'UART': '#1e8449', 'SPI': '#7d3c98', 'CAN': '#b7950b',
    'K': '#6c3483', 'I2C': '#117a65', 'AN': '#566573',
}


class Svg:
    def __init__(self, w, h, title, desc):
        self.w, self.h = w, h
        self.title, self.desc = title, desc
        self.parts = []
        self.pins = {}

    # ---- primitives ----
    def text(self, x, y, s, size=12, weight='normal', fill='#1b2631', anchor='start', italic=False):
        st = ' font-style="italic"' if italic else ''
        self.parts.append(
            f'<text xml:space="preserve" x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" fill="{fill}" '
            f'text-anchor="{anchor}"{st}>{escape(s)}</text>')

    def rect(self, x, y, w, h, fill='#ffffff', stroke='#34495e', dash=None, rx=6, sw=1.6):
        d = f' stroke-dasharray="{dash}"' if dash else ''
        self.parts.append(
            f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" '
            f'stroke="{stroke}" stroke-width="{sw}"{d}/>')

    def dot(self, x, y, color='#1b2631', r=3.5):
        self.parts.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}"/>')

    def wire(self, pts, kind='SIG', width=2.2, dash=None):
        col = COL[kind]
        d = f' stroke-dasharray="{dash}"' if dash else ''
        p = ' '.join(f'{x},{y}' for x, y in pts)
        self.parts.append(
            f'<polyline points="{p}" fill="none" stroke="{col}" stroke-width="{width}" '
            f'stroke-linejoin="round" stroke-linecap="round"{d}/>')

    # ---- components ----
    def box(self, key, x, y, w, h, title, left=(), right=(), top=(), bottom=(),
            dashed=False, sub=None, fill='#fdfefe', top_pad=0):
        """Pins: left/right = (label, dy); top/bottom = (label, dx).
        Returns/records pin coordinates as self.pins[(key, label)]."""
        self.rect(x, y, w, h, fill=fill, dash='6 4' if dashed else None)
        self.text(x + w / 2, y + 17 + top_pad, title, 12.5, 'bold', anchor='middle')
        if sub:
            self.text(x + w / 2, y + 31 + top_pad, sub, 10, fill='#566573', anchor='middle', italic=True)
        for label, dy in left:
            self.dot(x, y + dy, '#34495e', 3)
            self.text(x + 8, y + dy + 4, label, 11)
            self.pins[(key, label)] = (x, y + dy)
        for label, dy in right:
            self.dot(x + w, y + dy, '#34495e', 3)
            self.text(x + w - 8, y + dy + 4, label, 11, anchor='end')
            self.pins[(key, label)] = (x + w, y + dy)
        for label, dx in top:
            self.dot(x + dx, y, '#34495e', 3)
            self.text(x + dx, y + 15, label, 11, anchor='middle')
            self.pins[(key, label)] = (x + dx, y)
        for label, dx in bottom:
            self.dot(x + dx, y + h, '#34495e', 3)
            self.text(x + dx, y + h - 6, label, 11, anchor='middle')
            self.pins[(key, label)] = (x + dx, y + h)

    def pin(self, key, label):
        return self.pins[(key, label)]

    def flag(self, x, y, label, kind, side):
        """Net label attached to a pin at (x, y). side: l r u d (direction the flag extends)."""
        col = COL[kind]
        tw = max(26, 7.2 * len(label) + 12)
        if side == 'r':
            self.wire([(x, y), (x + 8, y)], kind, 2)
            self.parts.append(f'<rect x="{x + 8}" y="{y - 9}" width="{tw}" height="18" rx="3" '
                              f'fill="#fff" stroke="{col}" stroke-width="1.5"/>')
            self.text(x + 8 + tw / 2, y + 4, label, 10.5, 'bold', col, 'middle')
        elif side == 'l':
            self.wire([(x, y), (x - 8, y)], kind, 2)
            self.parts.append(f'<rect x="{x - 8 - tw}" y="{y - 9}" width="{tw}" height="18" rx="3" '
                              f'fill="#fff" stroke="{col}" stroke-width="1.5"/>')
            self.text(x - 8 - tw / 2, y + 4, label, 10.5, 'bold', col, 'middle')
        elif side == 'd':
            self.wire([(x, y), (x, y + 8)], kind, 2)
            self.parts.append(f'<rect x="{x - tw / 2}" y="{y + 8}" width="{tw}" height="18" rx="3" '
                              f'fill="#fff" stroke="{col}" stroke-width="1.5"/>')
            self.text(x, y + 8 + 13, label, 10.5, 'bold', col, 'middle')
        elif side == 'u':
            self.wire([(x, y), (x, y - 8)], kind, 2)
            self.parts.append(f'<rect x="{x - tw / 2}" y="{y - 26}" width="{tw}" height="18" rx="3" '
                              f'fill="#fff" stroke="{col}" stroke-width="1.5"/>')
            self.text(x, y - 13, label, 10.5, 'bold', col, 'middle')

    def flag_pin(self, key, label, net, kind, side):
        x, y = self.pin(key, label)
        self.flag(x, y, net, kind, side)

    def note(self, x, y, lines, size=11, fill='#1b2631', gap=15, weight='normal'):
        for i, ln in enumerate(lines):
            self.text(x, y + i * gap, ln, size, weight if i else 'bold', fill)

    def legend(self, x, y):
        self.rect(x, y, 330, 150, fill='#f8f9f9', stroke='#aab7b8')
        self.text(x + 10, y + 18, 'LEGEND', 11.5, 'bold')
        items = [('12V', '12 V power (fused)'), ('5V', '5 V rail'), ('3V3', '3.3 V rail'),
                 ('GND', 'Ground (all common)'), ('SIG', 'Digital signal / relay drive'),
                 ('UART', 'Nano - ESP32 UART'), ('SPI', 'SPI (SD + MCP2515 share)'),
                 ('I2C', 'I2C'), ('CAN', 'CAN bus'), ('K', 'K-Line'), ('AN', 'Analog sense')]
        for i, (k, label) in enumerate(items):
            cx = x + 12 + (i % 2) * 160
            cy = y + 36 + (i // 2) * 16
            self.wire([(cx, cy), (cx + 22, cy)], k, 3)
            self.text(cx + 28, cy + 4, label, 10)
        self.text(x + 12, y + 142, 'Same-named flags are connected; wire crossings are not.',
                  9.5, fill='#566573', italic=True)

    def save(self, path):
        head = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.w} {self.h}" '
                f'width="{self.w}" height="{self.h}" font-family="Segoe UI, Arial, sans-serif" role="img">'
                f'<title>{escape(self.title)}</title><desc>{escape(self.desc)}</desc>'
                f'<rect width="{self.w}" height="{self.h}" fill="#ffffff"/>')
        with open(path, 'w', encoding='utf-8') as f:
            f.write(head + '\n' + '\n'.join(self.parts) + '\n</svg>\n')
