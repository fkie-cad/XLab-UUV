#!/usr/bin/env bash

if [ -z "$1" ]; then
    echo "Usage: ./client.sh [IP in VPN network, e.g. 10.0.0.x]"
    exit
fi

function create_tun() {
    echo "tun0 not found, creating it"
    sudo ifconfig tun create
}
ifconfig tun0 || create_tun

echo "Establishing connection to the XLUUV-ring as IP $1"
cd "$(dirname "$0")"
sed "s/REPLACE/$1/g" client.conf > client-$1.conf
sudo openvpn client-$1.conf
cd -
