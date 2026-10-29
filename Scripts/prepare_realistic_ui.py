"""Prepare quiet, non-tonal UI foley from the existing Kenney CC0 sources."""
from pathlib import Path
import html
import json
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceArt/Audio/CartoonStarterKit'
OUT = ROOT / 'SourceArt/Audio/RealisticUI'
sys.path.insert(0, str(SOURCE / '_tools'))
import numpy as np
import soundfile as sf

SR = 48000
SPECS = {
    'Hover': ('轻微衣料摩擦', 'RPG', ['cloth1', 'cloth2', 'handleSmallLeather2'], .10, -35, -24),
    'Click': ('短促木质轻触', 'Impact', ['impactWood_light_000', 'impactWood_light_001', 'impactWood_light_002'], .13, -29, -17),
    'Confirm': ('轻放物件', 'RPG', ['bookPlace1', 'bookPlace2'], .20, -28, -16),
    'Cancel': ('轻翻纸张', 'RPG', ['bookFlip1', 'bookFlip2'], .18, -31, -20),
    'Error': ('低沉双敲', 'Impact', ['impactWood_medium_000', 'impactWood_medium_001'], .12, -30, -19),
    'Open': ('打开纸页', 'RPG', ['bookOpen', 'bookFlip3'], .30, -31, -20),
    'Close': ('合拢书页或皮革', 'RPG', ['bookClose', 'dropLeather'], .25, -30, -18),
    'Toggle': ('机械按键', 'UI', ['mouseclick1', 'mouserelease1'], .11, -30, -19),
}


def prepare(pack, name, duration, rms_db, peak_db, double=False):
    x, rate = sf.read(SOURCE / 'Library' / pack / 'Audio' / (name + '.ogg'), always_2d=True)
    x = x.mean(axis=1)
    x = np.interp(np.arange(int(round(len(x) * SR / rate))) * rate / SR, np.arange(len(x)), x)
    x -= x.mean()
    active = np.flatnonzero(np.abs(x) > np.max(np.abs(x)) * .008)
    x = x[max(0, active[0] - 144):min(len(x), active[-1] + 480)]
    limit = int(duration * SR)
    if len(x) > limit:
        # Keep the strongest short action, with a little lead-in before the transient.
        start = max(0, min(int(np.argmax(np.abs(x))) - 480, len(x) - limit))
        x = x[start:start + limit]
    frequencies = np.fft.rfftfreq(len(x), 1 / SR)
    x = np.fft.irfft(np.fft.rfft(x) / np.sqrt(1 + (frequencies / 3200) ** 8), n=len(x))
    fade = min(240, len(x) // 4)
    x[:fade] *= np.linspace(0, 1, fade)
    tail = min(720, len(x) // 3)
    x[-tail:] *= np.linspace(1, 0, tail)
    if double:
        offset = len(x) + int(.055 * SR)
        result = np.zeros(offset + len(x))
        result[:len(x)] = x
        result[offset:] = x * .6
        x = result
    gain = min(10 ** (rms_db / 20) / np.sqrt(np.mean(x * x)), 10 ** (peak_db / 20) / np.max(np.abs(x)))
    return x * gain


OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'ReadyToImport').mkdir(exist_ok=True)
rows = []
for event, (label, pack, names, duration, rms_db, peak_db) in SPECS.items():
    for i, name in enumerate(names, 1):
        x = prepare(pack, name, duration, rms_db, peak_db, event == 'Error')
        filename = f'SFX_UI_Realistic_{event}_{i:02}.wav'
        target = OUT / 'ReadyToImport' / filename
        sf.write(target, x, SR, subtype='PCM_16')
        decoded, rate = sf.read(target)
        assert rate == SR and np.isfinite(decoded).all() and 0 < np.max(np.abs(decoded)) < .2
        assert np.max(np.abs(decoded[[0, -1]])) <= 1 / 32768
        rows.append(dict(event='UI.' + event, name=target.stem, file='ReadyToImport/' + filename,
                         label=label, source=f'../CartoonStarterKit/Library/{pack}/Audio/{name}.ogg',
                         license='CC0-1.0', seconds=round(len(x) / SR, 3),
                         peak_db=round(20 * np.log10(np.max(np.abs(decoded))), 2),
                         processing='mono PCM16; trim; 3.2kHz soft low-pass; fades; RMS target with peak ceiling'))

parts, comparisons, timeline = [], [], []
for event in SPECS:
    row = next(r for r in rows if r['event'] == 'UI.' + event)
    new = sf.read(OUT / row['file'])[0]
    old = sf.read(SOURCE / f'ReadyToImport/UI/SFX_UI_{event}_01.wav')[0]
    timeline.append(dict(event=row['event'], new_preview_start=round(sum(len(x) for x in parts) / SR, 3)))
    parts.extend([new, np.zeros(int(.40 * SR))])
    comparisons.extend([old, np.zeros(int(.35 * SR)), new, np.zeros(int(.65 * SR))])
sf.write(OUT / 'Preview_RealisticUI.wav', np.concatenate(parts), SR, subtype='PCM_16')
sf.write(OUT / 'Compare_OldThenNew.wav', np.concatenate(comparisons), SR, subtype='PCM_16')
(OUT / 'manifest.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf-8')
(OUT / 'preview_timeline.json').write_text(json.dumps(timeline, ensure_ascii=False, indent=2), encoding='utf-8')
for pack in ['RPG', 'Impact', 'UI']:
    (OUT / 'Licenses').mkdir(exist_ok=True)
    shutil.copy2(SOURCE / 'Library' / pack / 'License.txt', OUT / 'Licenses' / (pack + '.txt'))
cards = ''.join(f'<article><b>{r["event"]} / {html.escape(r["label"])}</b><p>{r["name"]} · {r["seconds"]:.2f}s</p>'
                f'<audio controls preload="none" src="{r["file"]}"></audio></article>' for r in rows)
(OUT / '试听.html').write_text('''<!doctype html><meta charset="utf-8"><title>写实 UI 试听</title>
<style>body{font:16px/1.6 system-ui;max-width:1050px;margin:40px auto;background:#f3f0e9;color:#282b27;padding:20px}article{background:white;padding:18px;border-radius:10px}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:16px}audio{width:100%}p{font-size:14px}</style>
<h1>写实 UI · 轻触、纸张与机械按键</h1><p>18 条拟音变体，沿用 UI 事件名称。峰值约 -24 至 -16 dBFS，无合成提示音或旋律。初版需试听确认，尚未在游戏内做主观混音评审。</p>
<h2>新版串烧</h2><p>悬停 → 点击 → 确认 → 返回 → 失败 → 打开 → 关闭 → 切换</p><audio controls src="Preview_RealisticUI.wav"></audio>
<h2>新旧对比</h2><p>每个事件先旧版、后新版，顺序与上方一致；请保持播放器音量一致。</p><audio controls src="Compare_OldThenNew.wav"></audio><h2>逐条试听</h2><main>'''
    + cards + '''</main><script>document.addEventListener('play',e=>{if(e.target.tagName==='AUDIO')document.querySelectorAll('audio').forEach(a=>{if(a!==e.target)a.pause()})},true)</script>''', encoding='utf-8')
print(json.dumps({'sounds':len(rows), 'events':len(SPECS), 'preview_seconds':sf.info(OUT / 'Preview_RealisticUI.wav').duration}))
