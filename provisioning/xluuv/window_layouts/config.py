import os


class LayoutNotFound(FileNotFoundError):
    pass


def get_last_layout() -> str:
    config_path = os.path.expanduser("~/.config/xluuv/")
    layout_file_path = os.path.join(config_path, "last-layout.json")

    if not os.path.exists(config_path):
        os.makedirs(config_path)
        raise LayoutNotFound(f"The config folder `{config_path}` does not exist.")

    if not os.path.exists(layout_file_path):
        raise LayoutNotFound(f"No last layout `{layout_file_path}` was saved.")

    if not os.path.islink(layout_file_path):
        os.remove(layout_file_path)
        raise LayoutNotFound(
            f"The file at `{layout_file_path}` is not a symlink. File was deleted. "
            f"Layout cannot be loaded."
        )

    target_path = os.path.realpath(layout_file_path)
    return target_path


def create_link_to_last_layout(path):
    absolute_path = os.path.abspath(path)

    if not os.path.isfile(absolute_path):
        raise FileNotFoundError(f"The given path '{absolute_path}' is not a file.")

    config_path = os.path.expanduser("~/.config/xluuv/")
    layout_file_path = os.path.join(config_path, "last-layout.json")

    if not os.path.exists(config_path):
        os.makedirs(config_path)

    if os.path.islink(layout_file_path):
        os.remove(layout_file_path)

    os.symlink(absolute_path, layout_file_path)
