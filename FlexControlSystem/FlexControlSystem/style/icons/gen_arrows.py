# -*- coding: utf-8 -*-
"""生成 聚拢 / 振散 两个图标的 SVG。

为什么用"描边杆 + 实心三角头"而不是纯折线箭头:
  纯折线的 28 度箭头在 32px 显示尺寸下会被读成"方括号",
  4 支拼起来更像一个方框, 现场分不清"聚拢"和"振散"。
  实心三角头在同样尺寸下轮廓清楚, 一眼是箭头。

坐标一律用"沿箭头朝向的带符号距离 r"表示: 点 = 圆心 + 朝向 * r,
r > 0 在箭头前方, r < 0 在箭头后方。
  聚拢: 朝向指向圆心, 尖端 r=-3.5(离圆心 3.5, 在近心侧), 杆从 r=-30(外圈)伸进来
  振散: 朝向背离圆心, 尖端 r=25(外圈), 杆从 r=4(靠圆心)伸出去
杆只画到三角底边以内 1.5px, 避免杆的圆头把尖端顶粗。
"""
import math
import os

CX = CY = 32.0
STROKE = 5.5
DIAG = [(1, 1), (1, -1), (-1, 1), (-1, -1)]

# name: (朝向是否指向圆心, 尖端 r, 三角头长, 三角头半宽, 杆起点 r)
CFG = {
    'gather':   (True,  -3.5, 8.5, 7.4, -30.0),
    'disperse': (False, 25.0, 9.5, 6.8, 4.0),
}
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src')


def fmt(v):
    return ('%.1f' % v).rstrip('0').rstrip('.')


def pt(x, y):
    return '%s,%s' % (fmt(x), fmt(y))


def build(inward, r_tip, head_h, head_w, r_tail, with_dot):
    strokes, heads = [], []
    for dx, dy in DIAG:
        n = math.hypot(dx, dy)
        ux, uy = dx / n, dy / n
        if inward:
            ux, uy = -ux, -uy

        def at(r):
            return (CX + ux * r, CY + uy * r)

        tip = at(r_tip)
        base = at(r_tip - head_h)
        shaft_end = at(r_tip - head_h + 1.5)
        start = at(r_tail)
        strokes.append('M%s L%s' % (pt(*start), pt(*shaft_end)))

        px, py = -uy, ux     # 底边的垂直方向
        heads.append('M%s L%s L%s Z' % (
            pt(*tip),
            pt(base[0] + px * head_w, base[1] + py * head_w),
            pt(base[0] - px * head_w, base[1] - py * head_w)))

    dot = '\n  <circle cx="32" cy="32" r="3.6" fill="#16A34A"/>' if with_dot else ''
    return '\n    '.join('<path d="%s"/>' % d for d in strokes), \
           '\n    '.join('<path d="%s"/>' % d for d in heads), dot


TPL = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <!-- %s -->
  <g fill="none" stroke="#16A34A" stroke-width="%s" stroke-linecap="round">
    %s
  </g>
  <g fill="#16A34A">
    %s
  </g>%s
</svg>
'''

for name, (inward, r_tip, head_h, head_w, r_tail) in CFG.items():
    note = ('聚拢: 四支箭头指向圆心' if inward else '振散: 四支箭头由圆心向外')
    # 振散保留中心点(参照 image/散开.png); 聚拢不要点, 否则两者太像
    st, hd, dot = build(inward, r_tip, head_h, head_w, r_tail, with_dot=not inward)
    with open(os.path.join(OUT, name + '.svg'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(TPL % (note, fmt(STROKE), st, hd, dot))
    print('written', os.path.join('src', name + '.svg'))
