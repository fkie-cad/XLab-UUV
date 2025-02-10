from collections import namedtuple, defaultdict

from mininet.link import Link, Intf
from mininet.net import Mininet
from mininet.node import Switch

from xluuv_net.util import get_switch

VLANEntry = namedtuple("VLANEntry", "host ip vlan_id")


class VLANConfig:
    def __init__(self, net: Mininet):
        self.net = net
        self.entries: list[VLANEntry] = []

    def show_config(self):
        host_vlan_map = defaultdict(list)
        for entry in self.entries:
            host_vlan_map[entry.host].append(entry.vlan_id)

        max_host_length = max(len(host) for host in host_vlan_map.keys()) + 2
        max_vlan_length = (
            max(len(", ".join(map(str, vlans))) for vlans in host_vlan_map.values()) + 2
        )

        host_separator = "-" * max_host_length
        vlan_separator = "-" * max_vlan_length

        print(f"*** VLAN config:")
        print(f"{'Host':<{max_host_length}} | {'VLANs':<{max_vlan_length}}")
        print(f"{host_separator} | {vlan_separator}")

        for host, vlans in host_vlan_map.items():
            print(f"{host:<{max_host_length}} | {', '.join(map(str, vlans))}")

    def add(self, host: str, ip: str, vlan_id: int):
        self.entries.append(VLANEntry(host, ip, vlan_id))

    def apply(self):
        self._set_switch_ports_to_trunk_mode()
        self._set_hosts_ports()

    def _set_switch_ports_to_trunk_mode(self):
        trunks = set(str(entry.vlan_id) for entry in self.entries)
        trunks_str = ",".join(trunks)
        for link in self._get_switch_to_switch_links():
            self._set_interface_to_trunk_mode(link.intf1, trunks_str)
            self._set_interface_to_trunk_mode(link.intf2, trunks_str)

    def _get_switch_to_switch_links(self) -> list[Link]:
        return [
            link for link in self.net.links if self._link_connects_two_switches(link)
        ]

    @staticmethod
    def _link_connects_two_switches(link: Link) -> bool:
        return isinstance(link.intf1.node, Switch) and isinstance(
            link.intf2.node, Switch
        )

    def _set_interface_to_trunk_mode(self, intf: Intf, trunks: str):
        switch = get_switch(self.net, intf.node.name)
        switch.cmd(f"ovs-vsctl set port {intf.name} vlan_mode=trunk")
        switch.cmd(f"ovs-vsctl set port {intf.name} trunks={trunks}")

    def _set_hosts_ports(self):
        host_links_map = self._get_host_links_map()
        for entry in self.entries:
            link = host_links_map[entry.host + entry.ip].pop(0)
            if isinstance(link.intf1.node, Switch):
                self._setup_switch_ports(link.intf1, entry)
            else:
                self._setup_switch_ports(link.intf2, entry)

    def _get_host_links_map(self) -> dict[str, list[Link]]:
        mapping = defaultdict(list)
        for link in self._get_host_to_switch_links():
            if isinstance(link.intf1.node, Switch):
                mapping[link.intf2.node.name + link.intf2.ip].append(link)
            else:
                mapping[link.intf1.node.name + link.intf1.ip].append(link)
        return mapping

    def _get_host_to_switch_links(self) -> list[Link]:
        return [
            link for link in self.net.links if self._link_connects_host_to_switch(link)
        ]

    def _link_connects_host_to_switch(self, link: Link) -> bool:
        return not self._link_connects_two_switches(link) and (
            isinstance(link.intf1.node, Switch) or isinstance(link.intf2.node, Switch)
        )

    def _setup_switch_ports(self, intf: Intf, entry: VLANEntry):
        switch = get_switch(self.net, intf.node.name)
        switch.cmd(f"ovs-vsctl set port {intf.name} vlan_mode=access")
        switch.cmd(f"ovs-vsctl set port {intf.name} tag={entry.vlan_id}")
