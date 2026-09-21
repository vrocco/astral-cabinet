#!/usr/bin/env python3
"""Validate the SD-backed offline Elemental Ritual library."""
import json
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "assets/sd/astral/rituals.json"
data = json.loads(path.read_text(encoding="utf-8"))
assert data["format"] == 1
expected = ("fire", "earth", "air", "water")
fields = ("intention", "practice", "release")
for element in expected:
    rituals = data[element]
    assert len(rituals) >= 12, (element, len(rituals))
    for ritual in rituals:
        assert set(ritual) == set(fields), ritual
        for field in fields:
            assert 1 <= len(ritual[field]) <= 120, (element, field, ritual[field])
print(f"Elemental ritual library passed: {sum(len(data[key]) for key in expected)} rituals")
