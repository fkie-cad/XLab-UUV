from mininet.net import Mininet
from mininet.node import Node, Switch


def get_node(net: Mininet, name: str) -> Node:
    node = net.get(name)
    if node is None:
        raise ValueError(f"Node `{name}` not found in Mininet.")
    return node


def get_switch(net: Mininet, name: str) -> Switch:
    switch = get_node(net, name)
    if not isinstance(switch, Switch):
        raise ValueError(f"Node `{name}` is not of type Switch.")
    return switch
