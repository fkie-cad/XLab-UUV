#!/usr/bin/env python3
from pathlib import Path
import json
import tempfile
import zipfile


tmp = Path(tempfile.gettempdir())

for p in Path("./missions/").glob("*.json"):
    with open(p, "rt") as fd:
        print(f"Working on {p}")

        mission = json.load(fd)
        print(f"creating a zip for '{mission['mission']['name']}' ({str(p)})")
        z_path = Path(p.stem + ".zip")
        if z_path.exists():
            z_path.unlink()
        res = zipfile.ZipFile(p.stem + ".zip", "x", zipfile.ZIP_DEFLATED)
        res.write(p, "mission.json")
        for route in mission["procedures"]["routes"]:
            r_path = Path(route["path"])
            res.write(r_path)
        for dive_procedure in mission["procedures"]["dive_procedures"]:
            r_path = Path(dive_procedure["path"])
            res.write(r_path)
        for loiter_position in mission["procedures"]["loiter_positions"]:
            r_path = Path(loiter_position["path"])
            res.write(r_path)
