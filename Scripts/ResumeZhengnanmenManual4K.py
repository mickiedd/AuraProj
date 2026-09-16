"""Resume only this run's imported assets after UE clear-coat pin correction.

The initial import completed before material graph construction failed on a
Python enum naming difference. The target Blueprint was not modified then.
"""
from pathlib import Path
path=Path(__file__).with_name('ApplyZhengnanmenManual4K.py')
exec(compile(path.read_text(),str(path),'exec'),{'__file__':str(path),'__name__':'__main__','RESUME_TASK_ASSETS':True})
