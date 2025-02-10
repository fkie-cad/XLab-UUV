from secrets import token_hex

from mininet.link import Link
from mininet.net import Mininet
from mininet.node import Node
from mininet.node import Switch


class MACsecLink(Link):
    def __init__(
        self,
        node1,
        node2,
        port1=None,
        port2=None,
        intfName1=None,
        intfName2=None,
        **params,
    ):
        super(MACsecLink, self).__init__(
            node1, node2, port1, port2, intfName1, intfName2, **params
        )

        ip1 = params.get("ip1", None)
        ip2 = params.get("ip2", None)
        encrypted = params.get("encrypted", True)

        key_a = token_hex(16)
        key_b = token_hex(16)
        setup_macsec(
            node=node1,
            iface=self.intf1,
            iface_macsec=self.intf1.name + "-macsec",
            ip=ip1,
            mac_addr=self.intf2.mac,
            tx_key=key_a,
            rx_key=key_b,
            tx_key_id="00",
            rx_key_id="01",
            encrypted=encrypted,
        )
        setup_macsec(
            node=node2,
            iface=self.intf2,
            iface_macsec=self.intf2.name + "-macsec",
            ip=ip2,
            mac_addr=self.intf1.mac,
            tx_key=key_b,
            rx_key=key_a,
            tx_key_id="01",
            rx_key_id="00",
            encrypted=encrypted,
        )


def setup_macsec(
    node: Node,
    iface: str,
    iface_macsec: str,
    mac_addr: str,
    tx_key: str,
    rx_key: str,
    tx_key_id: str,
    rx_key_id: str,
    encrypted: bool,
    ip: str = None,
):
    if encrypted:
        node.cmd(f"ip link add link {iface} {iface_macsec} type macsec encrypt on")
    else:
        node.cmd(f"ip link add link {iface} {iface_macsec} type macsec")

    node.cmd(f"ip macsec add {iface_macsec} rx port 1 address {mac_addr}")
    node.cmd(f"ip macsec add {iface_macsec} tx sa 0 pn 1 on key {tx_key_id} {tx_key}")
    node.cmd(
        f"ip macsec add {iface_macsec} rx port 1 address {mac_addr} sa 0 pn 1 on key {rx_key_id} {rx_key}"
    )
    node.cmd(f"ip link set dev {iface_macsec} up")
    if ip is not None:
        node.cmd(f"ifconfig {iface_macsec} {ip}")


def configure_bridge_for_macsec(net: Mininet, switch_name: str, bridge_ip: str = None):
    switch = net.get(switch_name)
    if switch is None:
        raise ValueError(f'Invalid switch name "{switch_name}"')

    bridge = switch_name + "-macsec"
    switch.cmd(f"ovs-vsctl add-br {bridge}")
    macsec_ports = [
        iface.name for iface in switch.intfList() if link_connects_switches(iface.link)
    ]
    host_ports = [iface.name for iface in switch.intfList() if iface.name != "lo"]

    for port in macsec_ports:
        macsec_port = port + "-macsec"
        switch.cmd(f"ovs-vsctl add-port {bridge} {macsec_port}")

    for port in host_ports:
        switch.cmd(f"ovs-vsctl del-port {switch_name} {port}")
        switch.cmd(f"ovs-vsctl add-port {bridge} {port}")

    switch.cmd(f"ip link set dev {bridge} up")
    if bridge_ip is not None:
        switch.cmd(f"ip addr add {bridge_ip} dev {bridge}")


def link_connects_switches(link: Link) -> bool:
    if link is None:
        return False
    n1 = link.intf1.node
    n2 = link.intf2.node
    return node_is_switch(n1) and node_is_switch(n2)


def node_is_switch(node: Node) -> bool:
    return isinstance(node, Switch)
