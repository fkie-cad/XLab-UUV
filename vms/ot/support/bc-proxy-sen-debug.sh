#!/usr/bin/env bash
source /opt/opendds/OpenDDS-3.24.1/setenv.sh
cd /opt/dds/
./bc-sen-proxy -ORBDebugLevel 1 -DCPSConfigFile rtps.ini -sen-port 10112 -ais-port 10114
