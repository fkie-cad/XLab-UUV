#!/usr/bin/python3
from __future__ import print_function

import argparse
import time

from mininet.clean import cleanup
from mininet.cli import CLI
from mininet.link import Intf
from mininet.log import setLogLevel
from mininet.net import Mininet
from mininet.topo import Topo

from xluuv_net.macsec import MACsecLink, configure_bridge_for_macsec
from xluuv_net.nat import NAT
from xluuv_net.tclink import configure_ccc_link
from xluuv_net.vlan import VLANConfig


class NetworkTopo(Topo):
    # Builds network topology with a loop
    def __init__(self, *args, **params):
        self.macsec = params.pop("macsec", False)
        super().__init__(*args, **params)

    def build(self, **_opts):
        # Adding legacy switches
        s1, s2, s3, s4, s5 = [
            self.addSwitch(s, failMode="standalone")
            for s in ("s1", "s2", "s3", "s4", "s5")
        ]

        # Creating links
        link_options = {"cls": MACsecLink} if self.macsec else {}
        self.addLink(s1, s2, **link_options)
        self.addLink(s2, s3, **link_options)
        self.addLink(s3, s4, **link_options)
        self.addLink(s4, s5, **link_options)

        # Adding hosts
        ccc = self.addHost("ccc", cls=NAT, ip="10.0.1.1/24")
        self.addLink(ccc, s5)
        self.addLink(ccc, s5, params1={"ip": "10.0.2.1/24"})

        ap = self.addHost("ap", ip="10.0.1.2/16")
        self.addLink(ap, s3)

        vpn = self.addHost("vpn", ip="10.0.1.99/16")
        self.addLink(vpn, s2)

        rov = self.addHost("rov", ip="10.0.2.2/16")
        self.addLink(rov, s1)

    def configure_after_start(self, net):
        for switch in ["s1", "s2", "s3", "s4", "s5"]:
            configure_bridge_for_macsec(net, switch)

        # Setups the following VLANs:
        # Node                  | VLANs
        # --------------------- | -----------
        # Autopilot (AP)        | 100
        # Sensor/Aktuator (vpn) | 100
        # CCC                   | 100, 200
        # ROV                   | 200

        vlan = VLANConfig(net)
        vlan.add("ccc", "10.0.1.1", 100)
        vlan.add("ap", "10.0.1.2", 100)
        vlan.add("vpn", "10.0.1.99", 100)

        vlan.add("ccc", "10.0.2.1", 200)
        vlan.add("rov", "10.0.2.2", 200)

        vlan.apply()
        vlan.show_config()

        # Attach external VMs
        Intf("enp0s8", node=net.get("vpn"))
        net.get("vpn").cmd(f"ip addr add 192.168.1.1 dev enp0s8")
        net.get("vpn").cmd(
            f"route add -net 192.168.1.0 netmask 255.255.255.0 dev enp0s8"
        )
        Intf("enp0s9", node=net.get("ccc"))
        net.get("ccc").cmd(f"ip addr add 192.168.3.1 dev enp0s9")
        net.get("ccc").cmd(
            f"route add -net 192.168.3.0 netmask 255.255.255.0 dev enp0s9"
        )

        # Forward ROV stream to CCC
        net.get("ccc").add_prerouting_rule(
            daddr="10.0.2.1", dport="5010", dnat="192.168.3.100:5010", proto="tcp"
        )
        net.get("ccc").add_prerouting_rule(
            daddr="10.0.2.1", dport="5010", dnat="192.168.3.100:5010", proto="udp"
        )

        # Configure Multicast Routes
        for host in ["ccc", "vpn", "ap", "rov"]:
            net.get(host).cmdPrint(
                f"route add -net 224.0.0.0 netmask 240.0.0.0 dev {host}-eth0"
            )
            net.get(host).cmdPrint(f"ifconfig {host}-eth0 multicast")

        # configure bandwidth and loss rate of ccc link
        # use the default LTE config
        # available options are direct (no loss / bandwidth restriction), dsl, lte and starlink
        configure_ccc_link(net, link_type="lte")


def run(args: argparse.Namespace):
    cleanup()

    topo_options = {"macsec": True} if args.macsec else {}
    topo = NetworkTopo(**topo_options)

    net = Mininet(topo=topo, controller=None)

    net.start()
    topo.configure_after_start(net)

    # Start Autopilot
    if args.debug:
        net.get("ap").cmd(f"./scripts/autopilot-debug.sh &> autopilot.log &")
    else:
        net.get("ap").cmd(f"./scripts/autopilot.sh &> autopilot.log &")

    # Start ROV Stream
    net.get("rov").cmd(
        f"su vagrant -c '/opt/src/rov/video-sender.sh 10.0.2.1 &> rov.log &'"
    )

    # Start CCC Proxy
    if args.debug:
        net.get("ccc").cmd(f"./scripts/ccc-debug.sh &> ccc.log &")
    else:
        net.get("ccc").cmd(f"./scripts/ccc.sh &> ccc.log &")

    # Start VPN
    net.get("vpn").cmd(f"./vpn/bridge-start &> vpn.log &")
    time.sleep(1)
    net.get("vpn").cmd(f"./vpn/server.sh &>> vpn.log &")

    CLI(net)
    net.stop()


if __name__ == "__main__":
    parser = argparse.ArgumentParser("XLUUV Mininet")
    parser.add_argument(
        "--macsec",
        action="store_true",
        default=False,
        help="Enable MACsec encryption between switches",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        default=False,
        help="Set debug flags in components, i.e. ORBDebugLevel 1",
    )
    _args = parser.parse_args()
    setLogLevel("info")
    run(_args)
