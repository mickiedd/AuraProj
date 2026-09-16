"""Allow editor texture resources to settle before running V3 validation."""
import sys
import time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
time.sleep(15)
import ValidateV3Buildings
ValidateV3Buildings.main()
