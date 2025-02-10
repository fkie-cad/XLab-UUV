from collections import defaultdict

from mininet.net import Mininet
from mininet.node import Switch

from xluuv_net.util import get_switch


def configure_hsr_switch(net: Mininet) -> None:
    target_interfaces = get_links_between_switches(net)
    for name, interfaces in target_interfaces.items():
        switch = get_switch(net, name)
        switch.cmd("sysctl net.ipv4.ip_forward=1")
        for intf in interfaces:
            switch.cmd(
                f"ovs-vsctl set Interface {intf} type=hsr options:supervision-interval=45"
            )


def get_links_between_switches(net: Mininet) -> dict[str, list[str]]:
    target_interfaces = defaultdict(list)
    for link in net.links:
        node1 = link.intf1.node
        node2 = link.intf2.node
        if isinstance(node1, Switch) and isinstance(node2, Switch):
            target_interfaces[node1.name].append(link.intf1.name)
            target_interfaces[node2.name].append(link.intf2.name)
    return target_interfaces
