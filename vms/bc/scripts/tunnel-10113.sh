#!/usr/bin/env bash
cd "$(dirname "$0")"

./tunnel.py 192.178.1.2 10113 10.0.2.2 10113
