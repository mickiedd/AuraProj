"""Settle the unreal.Rotator argument order.

unreal.Rotator(0.0, 45.0, 0.0) reads back with pitch = 45, which means the
second positional argument is pitch, not yaw. That would explain why the
showcase landmarks are pitched over instead of turned. This checks the
positional order, the keyword order, and direct property assignment.
"""

import json

import unreal

report = {}

positional = unreal.Rotator(11.0, 22.0, 33.0)
report["positional_(11,22,33)"] = {"pitch": float(positional.pitch),
                                   "yaw": float(positional.yaw),
                                   "roll": float(positional.roll)}

keyword = unreal.Rotator(pitch=11.0, yaw=22.0, roll=33.0)
report["keyword_p11_y22_r33"] = {"pitch": float(keyword.pitch),
                                 "yaw": float(keyword.yaw),
                                 "roll": float(keyword.roll)}

yaw_only_kw = unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0)
report["keyword_yaw90"] = {"pitch": float(yaw_only_kw.pitch),
                           "yaw": float(yaw_only_kw.yaw),
                           "roll": float(yaw_only_kw.roll)}

# Can the struct fields be assigned directly?
try:
    assigned = unreal.Rotator(0.0, 0.0, 0.0)
    assigned.yaw = 90.0
    report["direct_yaw_assign"] = {"pitch": float(assigned.pitch),
                                   "yaw": float(assigned.yaw),
                                   "roll": float(assigned.roll)}
except Exception as exc:
    report["direct_yaw_assign"] = "failed: {}".format(exc)

# A constructor that takes a single yaw would be the safest call site.
try:
    report["make_from_yaw"] = str(unreal.Rotator.make_from_yaw(90.0))
except Exception as exc:
    report["make_from_yaw"] = "failed: {}".format(exc)

unreal.log("ROTATOR_ORDER " + json.dumps(report))
print("ROTATOR_ORDER", json.dumps(report, indent=2))
