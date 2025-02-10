from dataclasses import dataclass
from typing import Union

from mininet.net import Mininet
from mininet.node import Node

from xluuv_net.util import get_node


@dataclass
class LinkQuality:
    rate: Union[int, None]
    delay_avg: Union[int, None]
    delay_var: Union[int, None] = None
    delay_dist: Union[str, None] = None
    loss: Union[float, None] = None
    rate_unit: str = "mbit"


@dataclass
class LinkConfig:
    upstream: LinkQuality
    downstream: LinkQuality

    @classmethod
    def direct(cls):
        upstream = LinkQuality(rate=None, delay_avg=None)
        downstream = LinkQuality(rate=None, delay_avg=None)
        return cls(upstream, downstream)

    @classmethod
    def dsl(cls):
        upstream = LinkQuality(rate=10, delay_avg=16)
        downstream = LinkQuality(rate=50, delay_avg=16)
        return cls(upstream, downstream)

    @classmethod
    def lte(cls):
        upstream = LinkQuality(rate=5, delay_avg=55)
        downstream = LinkQuality(rate=24, delay_avg=55)
        return cls(upstream, downstream)

    @classmethod
    def starlink(cls):
        upstream = LinkQuality(
            rate=17, delay_avg=25, delay_var=12, delay_dist="normal", loss=1.75
        )
        downstream = LinkQuality(
            rate=178, delay_avg=25, delay_var=12, delay_dist="normal", loss=0.425
        )
        return cls(upstream, downstream)


def get_link_config(name: str) -> LinkConfig:
    factory = {
        "dsl": LinkConfig.dsl(),
        "lte": LinkConfig.lte(),
        "starlink": LinkConfig.starlink(),
    }
    config = factory.get(name)
    if config is None:
        raise ValueError(
            f'Unknown link config `{name}`. Choose from {", ".join(factory.keys())}.'
        )
    return config


def configure_ccc_link(net: Mininet, link_type: str = "lte", name: str = "ccc"):
    ccc = get_node(net, name)
    link_config = get_link_config(link_type)
    for link in net.links:
        node1 = link.intf1.node
        node2 = link.intf2.node
        if node1 == ccc:
            configure_tc_link(node1, link.intf1.name, link_config.upstream)
            configure_tc_link(node2, link.intf2.name, link_config.downstream)
        elif node2 == ccc:
            configure_tc_link(node1, link.intf1.name, link_config.downstream)
            configure_tc_link(node2, link.intf2.name, link_config.upstream)


def configure_tc_link(node: Node, interface: str, quality: LinkQuality):
    if quality.rate is not None and quality.delay_avg is not None:
        qdisc = f"tc qdisc add dev {interface} root handle 1:0 netem rate {quality.rate}{quality.rate_unit}"
        qdisc += f" delay {quality.delay_avg}ms"
        if quality.delay_var is not None:
            qdisc += f" {quality.delay_var}ms"
        if quality.delay_dist is not None:
            qdisc += f" distribution {quality.delay_dist}"
        if quality.loss is not None:
            qdisc += f" loss random {quality.loss}%"
        output = node.cmd(qdisc)
        print(output)
