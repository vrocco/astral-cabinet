#!/usr/bin/env python3
"""Create the compact daily planetary cache consumed by Astral Cabinet."""
from __future__ import annotations

import argparse
import math
from datetime import datetime, timedelta, timezone
from pathlib import Path

import ephem

SIGNS = (
    "Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo",
    "Libra", "Scorpio", "Sagittarius", "Capricorn", "Aquarius", "Pisces",
)
PLANETS = (
    ("Mercury", ephem.Mercury),
    ("Venus", ephem.Venus),
    ("Mars", ephem.Mars),
    ("Jupiter", ephem.Jupiter),
    ("Saturn", ephem.Saturn),
)
ASPECTS = ((0.0, "conjunct"), (60.0, "sextile"), (90.0, "square"), (120.0, "trine"), (180.0, "opposite"))


def longitude(planet_type: type[ephem.Planet], when: datetime) -> float:
    body = planet_type()
    body.compute(ephem.Date(when))
    return float(ephem.Ecliptic(body).lon) * 180.0 / math.pi


def signed_delta(start: float, end: float) -> float:
    return (end - start + 180.0) % 360.0 - 180.0


def zodiac(longitude_degrees: float) -> str:
    return SIGNS[int((longitude_degrees % 360.0) // 30.0)]


def epoch(date: ephem.Date) -> int:
    return int(date.datetime().replace(tzinfo=timezone.utc).timestamp())


def build_feed(when: datetime) -> str:
    positions = []
    tomorrow = when + timedelta(days=1)
    for name, planet_type in PLANETS:
        now = longitude(planet_type, when)
        movement = signed_delta(now, longitude(planet_type, tomorrow))
        positions.append((name, now, movement))

    now_ephem = ephem.Date(when)
    previous_new = ephem.previous_new_moon(now_ephem)
    lunar_age = float(now_ephem - previous_new)
    phase = "new moon" if lunar_age < 1.84566 else "waxing crescent" if lunar_age < 5.53699 else "first quarter" if lunar_age < 9.22831 else "waxing gibbous" if lunar_age < 12.91963 else "full moon" if lunar_age < 16.61096 else "waning gibbous" if lunar_age < 20.30228 else "last quarter" if lunar_age < 23.99361 else "waning crescent"
    moon = ephem.Moon(now_ephem)

    # Mercury is always retained because it is the most requested retrograde state.
    feature = positions[:4]
    lines = [
        "ASTRAL COSMIC WEATHER",
        f"Updated {when:%Y-%m-%d} UTC",
        f"Moon: {phase} {round(moon.phase)}%",
        f"Next full: {epoch(ephem.next_full_moon(now_ephem))}",
        f"Next new: {epoch(ephem.next_new_moon(now_ephem))}",
    ]
    for name, value, movement in feature:
        motion = "retrograde" if movement < 0 else "direct"
        lines.append(f"{name}: {motion} in {zodiac(value)}")

    nearest = None
    for index, (first_name, first_value, _) in enumerate(positions):
        for second_name, second_value, _ in positions[index + 1:]:
            separation = abs(signed_delta(first_value, second_value))
            for target, label in ASPECTS:
                distance = abs(separation - target)
                candidate = (distance, first_name, label, second_name)
                if nearest is None or candidate < nearest:
                    nearest = candidate
    if nearest and nearest[0] <= 1.5:
        _, first_name, label, second_name = nearest
        lines.append(f"Aspect: {first_name} {label} {second_name}")

    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("data/cosmic.txt"))
    args = parser.parse_args()
    now = datetime.now(timezone.utc).replace(second=0, microsecond=0)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(build_feed(now), encoding="utf-8")


if __name__ == "__main__":
    main()
