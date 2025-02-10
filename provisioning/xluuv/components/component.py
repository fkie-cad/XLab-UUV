import os
import pathlib
import re
import subprocess
from abc import ABC, abstractmethod
from collections import namedtuple

Command = namedtuple("Command", "cmd blocking")


class ProvisionComponent(ABC):
    def __init__(self, repository: pathlib.Path):
        self.repository = repository
        self.components_dir = self.repository.joinpath("vms")
        self._port = None
        self.ssh_user = "vagrant"

    @property
    @abstractmethod
    def name(self) -> str: ...  # noqa: E704

    @property
    @abstractmethod
    def path(self) -> pathlib.Path: ...  # noqa: E704

    @property
    def port(self) -> int:
        if self._port is None:
            self._port = self.get_vagrant_port()[-1]
        return self._port

    def get_vagrant_port(self) -> list[int]:
        try:
            output = subprocess.check_output(
                "vagrant port", shell=True, text=True, cwd=self.path
            )
            forwarded_ports = re.findall(r"(\d+) \(guest\) => (\d+) \(host\)", output)
            host_ports = [int(match[1]) for match in forwarded_ports]
            if len(host_ports) == 0:
                print(f"ERROR: No forwarding port found for VM `{self.name}`")
                exit(1)
            return host_ports
        except subprocess.CalledProcessError as e:
            print(f"Error: {e}")
            exit(1)

    @property
    @abstractmethod
    def ssh_key(self) -> pathlib.Path:
        return self.path.joinpath(
            f".vagrant/machines/{self.name}/virtualbox/private_key"
        )

    def start_commands(self) -> list[Command]:
        return [
            Command(f"cd {self.path}", blocking=True),
            self.vagrant_up(),
        ]

    def provision_commands(self, rebuild: bool) -> list[Command]:
        if rebuild:
            return [
                Command(f"cd {self.path}", blocking=True),
                self.vagrant_destroy(),
                self.vagrant_provision(),
            ]

        else:
            return [Command(f"cd {self.path}", blocking=True), self.vagrant_provision()]

    @abstractmethod
    def post_start_commands(self) -> list[Command]: ...  # noqa: E704

    def stop_commands(self) -> list[Command]:
        timeout = 30
        return self.strings_to_blocking_commands(
            [f"cd {self.path}", f"vagrant halt && sleep {timeout}"]
        )

    @staticmethod
    def vagrant_up() -> Command:
        return Command("vagrant up", blocking=True)

    @staticmethod
    def vagrant_provision() -> Command:
        return Command("vagrant up --provision", blocking=True)

    @staticmethod
    def vagrant_destroy() -> Command:
        return Command("vagrant destroy -f", blocking=True)

    @staticmethod
    def vagrant_ssh() -> Command:
        return Command("vagrant ssh", blocking=False)

    def ssh(self, x11_forwarding: bool = True) -> Command:
        return Command(
            f'ssh -o StrictHostKeyChecking=no {"-X" if x11_forwarding else ""} -p {self.port} -i {self.ssh_key} {self.ssh_user}@localhost',
            blocking=False,
        )

    def install_xcookie(self) -> list[Command]:
        home_directory = os.path.expanduser("~")
        xcookie_path = os.path.join(home_directory, "xcookie")
        return [
            self.ssh(x11_forwarding=True),
            Command("xauth nextract /home/vagrant/xcookie $DISPLAY", blocking=True),
            Command("exit", blocking=False),
            Command(
                f"scp -P {self.port} -i {self.ssh_key} {self.ssh_user}@localhost:/home/vagrant/xcookie {xcookie_path}",
                blocking=True,
            ),
            Command(f"xauth nmerge {xcookie_path}", blocking=True),
        ]

    @staticmethod
    def strings_to_blocking_commands(commands: list[str]) -> list[Command]:
        return [
            Command(cmd, blocking=True) if isinstance(cmd, str) else cmd
            for cmd in commands
        ]
