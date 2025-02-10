import pathlib

from xluuv.components.component import ProvisionComponent, Command


class OT(ProvisionComponent):
    def __init__(self, repository: pathlib.Path, debug: bool):
        super().__init__(repository)

        self.debug = debug

    @property
    def name(self) -> str:
        return "OT"

    @property
    def path(self) -> pathlib.Path:
        return self.components_dir.joinpath("ot")

    @property
    def ssh_key(self) -> pathlib.Path:
        return self.path.joinpath(".vagrant/machines/ot/virtualbox/private_key")

    def provision_commands(self, rebuild) -> list[Command]:
        if rebuild:
            return [
                Command(f"cd {self.path}", blocking=True),
                self.vagrant_destroy(),
                self.vagrant_provision(),
            ]

        else:
            return [
                Command(f"cd {self.path}", blocking=True),
                self.vagrant_provision(),
            ]

    def post_start_commands(self) -> list[Command]:
        return [
            self.ssh(x11_forwarding=False),
            *self.start_proxies(),
        ]

    def start_proxies(self) -> list[Command]:
        return [
            Command("sleep 30", blocking=True),  # Wait for mininet to start
            Command("./vpn/client.sh 10.0.1.3 &", blocking=False),
            Command("sleep 10", blocking=True),  # Wait until VPN is ready
            Command("sudo route del 255.255.255.255", blocking=True),
            Command("sleep 1", blocking=True),
            Command(
                "sudo route add -net 224.0.0.0 -netmask 240.0.0.0 -interface tap0",
                blocking=True,
            ),
            Command("sleep 10", blocking=True),  # Wait until connections have settled
            Command(
                (
                    "./support/bc-proxy-sen-debug.sh &"
                    if self.debug
                    else "./support/bc-proxy-sen.sh &"
                ),
                blocking=False,
            ),
            Command(
                (
                    "./support/bc-proxy-akt-debug.sh &"
                    if self.debug
                    else "./support/bc-proxy-akt.sh &"
                ),
                blocking=False,
            ),
        ]
