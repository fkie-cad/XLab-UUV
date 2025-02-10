#!/usr/bin/env bash
cd "$(dirname "$0")"

./tunnel.py 192.178.1.2 10112 192.178.1.1 10112
