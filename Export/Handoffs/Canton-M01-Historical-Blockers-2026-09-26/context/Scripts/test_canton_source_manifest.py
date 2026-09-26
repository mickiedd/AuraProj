"""M01 source and prototype inventory; exit zero is integrity, not acceptance."""
import csv
import hashlib
from pathlib import Path
from validate_canton_terrain_days_01_05 import main
from refresh_canton_m01_provisional_manifest import FILES

ROOT = Path(__file__).resolve().parents[1]


def check_provisional_manifest():
    with (ROOT / 'Data/M01_Provisional_Artifacts.csv').open(newline='') as stream:
        rows = list(csv.DictReader(stream))
    if sorted(row['path'] for row in rows) != sorted(FILES):
        raise ValueError('Provisional inventory coverage mismatch')
    for row in rows:
        if hashlib.sha256((ROOT / row['path']).read_bytes()).hexdigest() != row['sha256']:
            raise ValueError('Provisional checksum mismatch: ' + row['path'])
        if row['status'] != 'Provisional; M01 Blocked':
            raise ValueError('Provisional output promoted without acceptance')


if __name__ == '__main__':
    check_provisional_manifest()
    main()
