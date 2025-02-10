import os
import pathlib

from xluuv.components.component import ProvisionComponent, Command


class BridgeCommand(ProvisionComponent):
    def __init__(self, repository: pathlib.Path, scenario: str, external: bool):
        super().__init__(repository)

        self.scenario = scenario
        self.external = external

    @property
    def name(self) -> str:
        return "bridgecommand"

    @property
    def path(self) -> pathlib.Path:
        return self.components_dir.joinpath("bc")

    @property
    def ssh_key(self) -> pathlib.Path:
        return self.path.joinpath(
            ".vagrant/machines/bridgecommand/virtualbox/private_key"
        )

    def post_start_commands(self) -> list[Command]:
        return [
            *self.install_xcookie(),
            self.ssh(x11_forwarding=True),
            *self.start_bridgecommand(),
        ]

    def start_bridgecommand(self) -> list[Command]:
        if self.external:
            print("!!! Starting bridgecommand-bc externally with our bc5-ext.ini file.")
            print("!!! Make sure you have bridgecommand compiled in vms/bc/bc.")
            cwd = os.getcwd()
            os.chdir(f"{self.path}/bc/bin/")
            os.system(
                f"{self.path}/bc/bin/bridgecommand-bc -c {self.path}/bc5-ext.ini -s {self.scenario} &"
            )
            os.chdir(cwd)

            return [
                Command("cd /home/vagrant/scripts", blocking=True),
                Command("./tunnel-10112.sh &", blocking=False),
                Command("./tunnel-10113.sh &", blocking=False),
                Command("./tunnel-10114.sh &", blocking=False),
            ]

        else:
            return [
                Command("cd /home/vagrant/bc/bin", blocking=True),
                Command(f"./bridgecommand-bc -a -s {self.scenario}", blocking=False),
            ]
