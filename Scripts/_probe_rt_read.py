"""Probe the render-target readback APIs."""
import unreal
for name in ("read_render_target_raw", "read_render_target_uv_area", "read_render_target_raw_uv_area", "read_render_target"):
    fn = getattr(unreal.RenderingLibrary, name, None)
    print("DOC", name, "|", (fn.__doc__ or "").replace("\r", " ").replace("\n", " ")[:300])
