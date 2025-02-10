from mininet.link import Intf
from mininet.node import Node


class NAT(Node):
    def __init__(self, name, **params):
        super(NAT, self).__init__(name, **params)

        self.cmd("sysctl -w net.ipv4.ip_forward=1")
        self.cmd("sysctl -p")
        self.cmd("nft flush ruleset")
        self.cmd("nft add table nat")
        self.cmd(
            "nft 'add chain nat postrouting { type nat hook postrouting priority 100 ; }'"
        )
        self.cmd(
            "nft 'add chain nat prerouting { type nat hook prerouting priority -100; }'"
        )

    def add_prerouting_rule(
        self, daddr: str, dport: str, dnat: str, proto: str = "tcp"
    ):
        self.cmd(
            f"nft 'add rule nat prerouting ip daddr {daddr} {proto} dport {{ {dport} }} dnat {dnat}'"
        )

    def add_postrouting_masquerade(self):
        self.cmd("sudo nft add rule nat postrouting masquerade")

    def print_ruleset(self):
        self.cmd(f"nft list ruleset")
