import json
import pathlib
import time

from xluuv.window_layouts.util import run_command
from xluuv.window_layouts.windows import (
    WindowConfig,
    WindowTitles,
    configure_windows,
    wait_until_windows_ready,
    WindowNotFound,
)


def save_current_layout(path: pathlib.Path):
    layout = get_fixed_window_layout()
    json_layout = convert_layout_to_json(layout)
    with open(path, "w") as f:
        f.write(json_layout)


def load_and_apply_layout(path: pathlib.Path):
    with open(path, "r") as f:
        json_content = f.read()
    layout = json.loads(json_content, object_hook=lambda d: WindowConfig(**d))
    successfully_applied = False
    while not successfully_applied:
        try:
            wait_until_windows_ready(layout)
            configure_windows(layout)
            successfully_applied = True
        except WindowNotFound:
            pass


def convert_layout_to_json(layout: list[WindowConfig]) -> str:
    return json.dumps(layout, default=lambda obj: dict(obj), indent=2)


def get_wmctrl_geometry() -> list[WindowConfig]:
    targets = [WindowTitles.OPENCPN, WindowTitles.BC, WindowTitles.CCC]
    window_geometry = {}
    current_layout, _ = run_command("wmctrl -l -G")
    current_layout = current_layout.split("\n")
    current_layout = [w for w in current_layout if w != ""]
    for window in current_layout:
        try:
            window_id, desktop_id, x, y, width, height, client_machine_name, *title = (
                window.split()
            )
            title = " ".join(title)
            if title in targets:
                window_geometry[title] = (int(width), int(height), int(x), int(y))
        except ValueError:
            pass

    layouts = []
    for title, geometry in window_geometry.items():
        layouts.append(WindowConfig(title, *geometry, 0))
    return layouts


def get_fixed_window_layout():
    before = get_wmctrl_geometry()
    configure_windows(before)
    after = get_wmctrl_geometry()
    fixed_layouts = fix_coordinate_drift(after, before)
    configure_windows(fixed_layouts)
    return fixed_layouts


def fix_coordinate_drift(
    after: list[WindowConfig], before: list[WindowConfig]
) -> list[WindowConfig]:
    coordinates_drift = [
        (l2.top_left_x - l1.top_left_x, l2.top_left_y - l1.top_left_y)
        for l1, l2 in zip(before, after)
    ]
    fixed_layouts = []
    for moved_layout, (x, y) in zip(before, coordinates_drift):
        moved_layout.top_left_x -= x
        moved_layout.top_left_y -= y
        fixed_layouts.append(moved_layout)
    return fixed_layouts
