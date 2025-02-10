#!/bin/bash
source /opt/opendds/OpenDDS-3.24.1/setenv.sh
cd /opt/dds/
./ccc-proxy -ORBDebugLevel 1 -DCPSConfigFile rtps.ini --grpc-port 42001 --ccc-telemetry-host 192.168.3.100 --ccc-telemetry-port 43002
