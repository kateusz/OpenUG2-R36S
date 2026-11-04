"""Global fork invariant: no actor/quality/reload path can call a vehicle shadow renderer."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
for name in ("src/main.c", "src/render.c", "src/render.h"):
    source = (root / name).read_text(encoding="utf-8")
    assert not re.search(r"\b(CarShadow|render_car_shadow|free_car_shadow|mat_ground_shadow|player_shadow|opponent_shadow)\b", source), name
main = (root / "src/main.c").read_text(encoding="utf-8")
assert "pf_mark(PF_CAR_SHADOW)" not in main  # retained CSV column is always zero
assert "ST_GFX_CAR_SHADOW" not in main  # serialized setting never feeds rendering
assert "g_CarShadowEnable" in (root / "src/settings.c").read_text(encoding="utf-8")
assert "if (c->gfx == ST_GFX_CAR_SHADOW) continue;" in (root / "src/options_menu.c").read_text(encoding="utf-8")
for shader in ("src/shaders/ug2.frag", "src/shaders/vk/ug2.frag"):
    assert "else if(uSoft>1.5)" not in (root / shader).read_text(encoding="utf-8")
assert "render_headlight_shadows" in main and "mat_car_footprint" in main # world shadows and neon retained
print("PASS: vehicle shadow APIs, caster ownership and footprint shader removed globally; settings compatibility retained")
