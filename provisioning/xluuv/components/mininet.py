import pathlib

from xluuv.components.component import ProvisionComponent, Command


class Mininet(ProvisionComponent):
    def __init__(self, repository: pathlib.Path, macsec: bool, debug: bool):
        self.mininet_args = " --macsec" if macsec else ""
        self.mininet_args += " --debug" if debug else ""
        super().__init__(repository)

    @property
    def name(self) -> str:
        return "mininet"

    @property
    def path(self) -> pathlib.Path:
        return self.components_dir.joinpath("mininet")

    @property
    def ssh_key(self) -> pathlib.Path:
        return self.path.joinpath(".vagrant/machines/mininet/virtualbox/private_key")

    def post_start_commands(self) -> list[Command]:
        return [
            *self.install_xcookie(),
            self.ssh(x11_forwarding=True),
            Command(
                "sudo -E python3 network/network.py" + self.mininet_args, blocking=False
            ),
        ]
