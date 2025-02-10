# DDS RTPS Participants

Internally, the XLUUV model exchanges sensor amongst a number of participants, in particular the `ccc-proxy`, responsible for translating communications to the remote CCC components, and the `autopilot`, responsible for steering the simulated XLUUV.
All participants are built as part of a single project. By default this is handled by the provisioning scripts, but some details useful for compiling modifying and testing the components locally are given below.

## Installing Dependencies

#### OpenDDS:

- [download latest release](https://opendds.org/downloads.html) and extract archive
- run `./configure` script
- run `make -j $(nproc)`
- source the OpenDDS environment variables: `source setenv.sh`

#### Boost Libraries

BC-Proxy requires the Boost ASIO library and the Boost Serialization library. Under Ubuntu these can be installed through
```
sudo apt install libasio-dev libboost-serialization-dev
```

#### gRPC Dependencies

```
sudo apt install build-essential autoconf libtool pkg-config
```

## Compiling the DDS Participants

**OpenDDS environment variables must be set for CMake to find the library!**, use `source setenv.sh` from the root of the openDDS installation. Then the autopilot can be compiled as follows:

- `cd bin/`
- `cmake ../src -DPULL_GRPC=True`
- `make -j $(nproc)`

This produces a number of binaries:
- `autopilot`, the autopilot component
- `ccc-proxy`, the DDS-to-Protobuf proxy
- `bc-sen-proxy`, the BC-to-DDS proxy
- `bc-act-proxy`, the DDS-to-BC proxy

To skip pulling gRPC dependencies `-DPULL_GRPC=False` can be passed to cmake. This will skip setting up the the `ccc-proxy` build target, but all four other targets can still be built.

## Testing the DDS Participants

Testing the DDS participants requires the CCC and BC to be available, to provide autopilot commands and sensor data respectively.

### With CCC and BC
1. Start the ccc server: `ccc-server --ccc-grpc-port CCC_GRPC_PORT --nmea-send-host NMEA_SEND_HOST --nmea-send-port NMEA_SEND_PORT --xluuv-report-port xluuv_REPORT_PORT --xluuv-grpc-port xluuv_GRPC_PORT --xluuv-grpc-host xluuv_GRPC_HOST`. All arguments can be ommitted
1. Start a ccc client, e.g. ccc GUI: `ccc-gui --ccc-grpc-host CCC_GRPC_HOST --ccc-grpc-port CCC_GRPC_PORT`. All arguments can be ommitted
1. Start the ccc proxy: `./ccc-proxy -ORBDebugLevel 1 -DCPSConfigFile ../rtps.dev.ini --grpc-port GRPC_PORT --ccc-telemetry-host CCC_TELEMETRY_HOST --ccc-telemetry-port CCC_TELEMETRY_PORT`. All `--*` arguments cans be ommitted.
1. Start the autopilot: `./autopilot -ORBDebugLevel 1 -DCPSConfigFile ../rtps.dev.ini`
1. Start the bc proxy: `./bc-proxy -ORBDebugLevel 1 -DCPSConfigFile ../rtps.dev.ini -bc-snd-addr BC_SND_ADDR -bc-snd-port BC_SND_PORT -bc-rcv-port BC_RCV_PORT`. `-bc-*` arguments can be ommitted
1. Start the bc sensor proxy: `./bc-sen-proxy -ORBDebugLevel 1 -DCPSConfigFile ../rtps.ini -senport SEN_PORT, -ais-port AISPORT`
1. Start the bc actuator proxy: `./bc-act-proxy -ORBDebugLevel 1 -DCPSConfigFile ../rtps.ini -bc-snd-addr BC_SND_ADDR -bc-snd-port ACT_PORT`
