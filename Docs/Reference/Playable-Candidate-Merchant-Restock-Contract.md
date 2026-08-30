# Playable Candidate merchant restock contract

The finite health-potion offer starts at 20. The authority evaluates server UTC, clamps the effective clock to the maximum observed value, and uses an inclusive 600-second interval. A startup check performs at most one catch-up refill; no offline loop advances multiple intervals. Restock timestamps and stock revision are persisted with the world record.
