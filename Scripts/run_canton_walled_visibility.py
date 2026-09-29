"""Verify the packaged walled-city map with its actual player camera and pixels."""
import argparse
import json
import plistlib
import shutil
import subprocess
import time
from pathlib import Path

from PIL import Image, ImageStat

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'QA/Canton_Continuation'


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--binary', type=Path, required=True)
    a = p.parse_args()
    binary = a.binary.resolve()
    app = next((item for item in binary.parents if item.suffix == '.app'), None)
    assert app, 'A packaged Mac app is required'
    bundle = plistlib.loads((app / 'Contents/Info.plist').read_bytes())['CFBundleIdentifier']
    container = Path.home() / 'Library/Containers' / bundle / 'Data/Documents/CantonQA'
    container.mkdir(parents=True, exist_ok=True)
    native = container / 'Walled_Runtime_Visibility.json'
    report = OUT / native.name
    screenshot = report.with_suffix('.png')
    log = ROOT / 'Saved/Logs/CantonWalledRuntimeVisibility.log'
    for path in (native, native.with_suffix('.png'), report, screenshot):
        path.unlink(missing_ok=True)
    command = [str(binary),
               '/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL',
               '-CantonWalledOutput=' + str(native),
               '-windowed', '-ResX=1280', '-ResY=720', '-ForceRes',
               '-unattended', '-nosplash', '-NoSound', '-nocef',
               '-ExecCmds=t.IdleWhenNotForeground 0',
               '-abslog=' + str(log), '-stdout', '-FullStdOutLogOutput']
    started = time.time()
    with log.open('w') as stream:
        process = subprocess.Popen(command, cwd=ROOT, stdout=stream,
                                   stderr=subprocess.STDOUT)
        try:
            exit_code = process.wait(timeout=100)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait(timeout=20)
            exit_code = 124
    errors = []
    if exit_code:
        errors.append('Game exit ' + str(exit_code))
    if not native.exists() or native.stat().st_mtime < started:
        errors.append('Fresh native report missing')
        data = {}
    else:
        shutil.copy2(native, report)
        data = json.loads(report.read_text())
        if not data.get('passed') or data.get('errors'):
            errors.append('Native runtime gate failed: ' + str(data.get('errors')))
        if data.get('editor_binary') is not False:
            errors.append('Editor evidence instead of packaged game')
        if data.get('historically_accepted') is not False:
            errors.append('Provisional terrain was promoted')
    pixels = {}
    source_image = native.with_suffix('.png')
    if not source_image.exists() or source_image.stat().st_mtime < started:
        errors.append('Fresh player-camera screenshot missing')
    else:
        shutil.copy2(source_image, screenshot)
        image = Image.open(screenshot).convert('RGB')
        sample = image.resize((256, 144))
        values = list(sample.getdata())
        visible = sum(max(rgb) > 30 for rgb in values) / len(values)
        pixels = {
            'width': image.width, 'height': image.height,
            'mean_rgb': [round(v, 2) for v in ImageStat.Stat(sample).mean],
            'nonblack_fraction': round(visible, 4),
            'distinct_sample_colors': len(set(values)),
        }
        if (image.size != (1280, 720) or visible < 0.25 or
                len(set(values)) < 128):
            errors.append('Player-camera frame is black or visually empty')
    result = {'passed': not errors, 'errors': errors, 'exit_code': exit_code,
              'elapsed_seconds': round(time.time() - started, 2),
              'native_report': str(report.relative_to(ROOT)),
              'screenshot': str(screenshot.relative_to(ROOT)),
              'pixels': pixels, 'command': command}
    (OUT / 'Walled_Runtime_Visibility_Check.json').write_text(
        json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
