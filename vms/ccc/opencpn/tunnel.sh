#!/usr/bin/env bash
cd "$(dirname "$0")"

./tunnel.py 127.0.0.1 10110 10.0.2.2 10110
