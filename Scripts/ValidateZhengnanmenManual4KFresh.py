"""Fresh serialized-state validation entrypoint for UnrealEditor-Cmd."""
from pathlib import Path
path=Path(__file__).with_name('ValidateZhengnanmenManual4K.py')
exec(compile(path.read_text(),str(path),'exec'),{'__file__':str(path),'__name__':'__main__','VALIDATION_MODE':'fresh'})
