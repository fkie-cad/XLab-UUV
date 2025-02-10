#!/usr/bin/env bash
source /opt/opendds/OpenDDS-3.24.1/setenv.sh
cd /opt/dds/
./bc-act-proxy -ORBDebugLevel 1 -DCPSConfigFile rtps.ini -bc-snd-addr 192.178.1.2 -bc-snd-port 10113
