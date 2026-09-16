"""Capture refreshed front and rear audit views for all V3 buildings."""
from CaptureV3Buildings import capture

for index in range(3):
    capture(index, 'front')
    capture(index, 'rear')

