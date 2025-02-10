#!/usr/bin/env bash
cd "$(dirname "$0")"

openvpn server.conf
