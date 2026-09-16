"""Target Blueprint roof/timber material inspection view."""
from pathlib import Path
path=Path(__file__).with_name('CaptureZhengnanmenManual4K.py')
exec(compile(path.read_text(),str(path),'exec'),{'__file__':str(path),'__name__':'__main__','CAPTURE_STAGE':'after','CAPTURE_VIEW':'roof'})
