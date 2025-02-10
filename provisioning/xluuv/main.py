import argparse
import pathlib

from xluuv.components.ot import OT
from xluuv.components.bridge_command import BridgeCommand
from xluuv.components.ccc import CCC
from xluuv.components.component import ProvisionComponent
from xluuv.components.mininet import Mininet
from xluuv.provisioning import ProvisionManager
from xluuv.window_layouts import load_layout, save_layout


def main(args: argparse.Namespace):
    repository = pathlib.Path(args.path).resolve()
    if not repository.is_dir():
        exit_with_error(f"Invalid path to repository: `{args.path}`")

    try:
        scenario = args.scenario
    except AttributeError:
        scenario = "SimpleEstuary"
    try:
        external = args.external
    except AttributeError:
        external = False
    try:
        macsec = args.macsec
    except AttributeError:
        macsec = False

    vms = [
        Mininet(repository, macsec, args.verbose),
        CCC(repository, scenario, external),
        BridgeCommand(repository, scenario, external),
        OT(repository, args.verbose),
    ]

    exit_on_invalid_vm_path(vms)
    exit_if_missing_vagrant_files(vms)
    xluuv = ProvisionManager(vms, args.verbose)

    if args.commands == "build":
        try:
            xluuv.provision_vms_sequentially(args.rebuild)
        except (KeyboardInterrupt, RuntimeError, SystemExit) as err:
            print(err)
        finally:
            xluuv.stop_components()
    elif args.commands == "up":
        try:
            xluuv.start_vms()
            xluuv.wait_until_vms_have_started()
            xluuv.exec_post_start_cmds()
            print("*** Apply window layout")
            load_layout(args.layout)
        except (KeyboardInterrupt, SystemExit):
            xluuv.stop_vms()
    elif args.commands == "layout":
        if args.save is not None:
            save_layout(args.save)
        else:
            load_layout(args.load)
    else:
        xluuv.stop_vms()


def exit_on_invalid_vm_path(vms: list[ProvisionComponent]):
    invalid_paths = [str(vm.path) for vm in vms if not vm.path.is_dir()]
    if len(invalid_paths) > 0:
        error_message = "Missing paths: " + ", ".join(invalid_paths)
        exit_with_error(error_message)


def exit_if_missing_vagrant_files(vms: list[ProvisionComponent]):
    missing_files = [
        str(vm.path.joinpath("Vagrantfile"))
        for vm in vms
        if not vm.path.joinpath("Vagrantfile").is_file()
    ]
    if len(missing_files) > 0:
        error_message = "Missing files: " + ", ".join(missing_files)
        exit_with_error(error_message)


def exit_with_error(msg: str):
    print(f"ERROR: {msg}")
    exit(1)


def entry_point():
    p = argparse.ArgumentParser("xluuv")
    p.add_argument(
        "-p",
        "--path",
        type=str,
        required=False,
        default=".",
        help="Path to the XLUUV repository root",
    )
    p_commands = p.add_subparsers(dest="commands", required=True)

    # xluuv up
    up = p_commands.add_parser("up", help="Start all XLUUV components / VMs")
    up.add_argument(
        "-s",
        "--scenario",
        type=str,
        required=False,
        help="Name of the Scenario: SimpleEstuary (default), or SantaCatalina",
        default="SimpleEstuary",
    )
    up.add_argument(
        "-e",
        "--external",
        action="store_true",
        default=False,
        help="Use an external BridgeCommand instance (Default: False)",
    )
    up.add_argument(
        "--macsec",
        action="store_true",
        default=False,
        help="Enable MACsec encryption between switches inside the network",
    )
    up.add_argument("-v", "--verbose", action="store_true", default=False)
    up.add_argument(
        "-l",
        "--layout",
        type=str,
        required=False,
        default=None,
        help="Path of the X11 window layout config to load.",
    )

    # xluuv down
    down = p_commands.add_parser("down", help="Stop all XLUUV components / VMs")
    down.add_argument("-v", "--verbose", action="store_true", default=False)

    # xluuv build
    build = p_commands.add_parser("build", help="Build all vagrant VMs.")
    build.add_argument(
        "-r",
        "--rebuild",
        action="store_true",
        default=False,
        help="Destroy existing VM before building new one (Default: False)",
    )
    build.add_argument("-v", "--verbose", action="store_true", default=False)

    layout = p_commands.add_parser(
        "layout", help="Set layout of (X11) application windows"
    )
    layout_commands = layout.add_mutually_exclusive_group(required=True)
    layout_commands.add_argument(
        "-s",
        "--save",
        type=str,
        default=None,
        help="Path to which the current window layout will be saved as JSON.",
    )
    layout_commands.add_argument(
        "-l",
        "--load",
        type=str,
        default=None,
        help="Path of the layout config to load.",
    )
    layout.add_argument("-v", "--verbose", action="store_true", default=False)
    _args = p.parse_args()
    main(_args)


if __name__ == "__main__":
    entry_point()
