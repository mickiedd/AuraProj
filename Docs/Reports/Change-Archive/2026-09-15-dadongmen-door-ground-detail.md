# Dadongmen door and approach detail

The Great East Gate Blueprint now has a reference-shaped door detail layer and a corrected presentation for the two oversized imported ground slabs. The source combined mesh remains unchanged. The Blueprint adds paired timber leaves, raised vertical planks, horizontal braces, iron straps, studs, and a stone sill with package-local materials. The dirt and water source materials are hidden, and compact approach and water-edge pieces are added for the reference-shaped foreground.

The 59 added visual components use `NoCollision`, so the existing authored collision and walk-through behavior remain authoritative. A preview-only interior fill light makes the door legible in the validation capture; it does not belong to the production Blueprint.

Validation passed in the live editor and a fresh `UnrealEditor-Cmd.exe -run=pythonscript -NullRHI` process. The native SceneCapture visual check produced the facade, door close-up, and rear approach views.

[Visual summary](2026-09-15-dadongmen-door-ground-detail.svg)
