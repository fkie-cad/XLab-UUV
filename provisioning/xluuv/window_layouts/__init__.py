import pathlib

from xluuv.window_layouts.config import (
    get_last_layout,
    create_link_to_last_layout,
    LayoutNotFound,
)
from xluuv.window_layouts.layout import save_current_layout, load_and_apply_layout
from xluuv.window_layouts.util import exit_with_error


def save_layout(path: str) -> None:
    path = pathlib.Path(path)
    if path.exists():
        exit_with_error(f"File `{path}` already exists. Abort saving layout ...")
    save_current_layout(path.absolute())
    create_link_to_last_layout(path)


def load_layout(path: str | None) -> None:
    if path is None:
        try:
            path = get_last_layout()
        except LayoutNotFound:
            return

    path = pathlib.Path(path)
    if not path.is_file():
        exit_with_error(f"Cannot load config file `{path}`.")
    load_and_apply_layout(path)
    create_link_to_last_layout(path)
