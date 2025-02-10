import pathlib
import os

from xluuv.components.component import ProvisionComponent, Command


class CCC(ProvisionComponent):
    def __init__(self, repository: pathlib.Path, scenario: str, external: bool):
        super().__init__(repository)

        self.scenario = scenario
        self.external = external

    @property
    def name(self) -> str:
        return "ccc"

    @property
    def path(self) -> pathlib.Path:
        return self.components_dir.joinpath("ccc")

    @property
    def ssh_key(self) -> pathlib.Path:
        return self.path.joinpath(".vagrant/machines/ccc/virtualbox/private_key")

    def post_start_commands(self) -> list[Command]:
        return [
            *self.install_xcookie(),
            self.ssh(x11_forwarding=True),
            *self.start_ccc_components(),
        ]

    def start_ccc_components(self) -> list[Command]:
        return [
            *self.start_ccc(),
            *self.start_opencpn(),
        ]

    def start_ccc(self) -> list[Command]:
        return [
            Command("cd /home/vagrant/ccc/examples/", blocking=True),
            Command("python3 ./create_zips.py", blocking=True),
            Command(
                "sleep 15 && ccc-server --xluuv-grpc-host 192.168.3.1 &", blocking=False
            ),
            Command("sleep 15 && ccc-gui &", blocking=False),
            # Autostart Autopilot
            Command("sleep 60", blocking=True),
            Command(
                f"ccc-cli -r SEND_MISSION_ZIP --rpc-arg-file {self.scenario}.zip",
                blocking=True,
            ),
            Command(
                "ccc-cli -r SEND_MISSION_COMMAND --rpc-arg MC_START", blocking=True
            ),
        ]

    def start_opencpn(self) -> list[Command]:
        if self.external:
            os.system("opencpn &")
            return [
                Command("/home/vagrant/ccc/opencpn/tunnel.sh &", blocking=False),
            ]

        else:
            return [
                Command("/home/vagrant/scripts/monitor_opencpn.sh &", blocking=False),
            ]
