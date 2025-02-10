import time
from dataclasses import dataclass, asdict

from xluuv.window_layouts.util import run_command


@dataclass
class WindowConfig:
    title: str
    width: int
    height: int
    top_left_x: int
    top_left_y: int
    gravity: int

    def __iter__(self):
        return iter(asdict(self).items())


class WindowTitles:
    BC = "Bridge Command 5.8.1"
    CCC = "CccGui"
    OPENCPN = "OpenCPN 5.8.4"


class WindowNotFound(ValueError):
    pass


def get_window_id(title: str) -> str:
    windows, _ = run_command("wmctrl -l")
    for window in windows.split("\n"):
        window = window.split()
        try:
            wid = window[0]
            wtitle = " ".join(window[3:])
            if wtitle == title:
                return wid
        except IndexError:
            pass
    raise WindowNotFound(f'Window "{title}" not found!')


def wait_until_windows_ready(window_configs: list[WindowConfig]):
    while not windows_ready(window_configs):
        time.sleep(1)


def windows_ready(window_configs: list[WindowConfig]) -> bool:
    for config in window_configs:
        try:
            get_window_id(config.title)
        except WindowNotFound:
            return False
    return True


def configure_windows(window_configs: list[WindowConfig]):
    for config in window_configs:
        wid = get_window_id(config.title)
        run_command(f"wmctrl -i -r {wid} -b remove,maximized_vert,maximized_horz")
        run_command(
            f"wmctrl -i -r {wid} -e {config.gravity},{config.top_left_x},{config.top_left_y},{config.width},{config.height}"
        )
