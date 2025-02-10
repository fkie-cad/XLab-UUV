#!/usr/bin/env python3
import argparse
from concurrent import futures
import threading
import logging

import grpc

import xluuv_ccc.messages_pb2_grpc
from xluuv_ccc.server.nmea_client import NmeaClient
from xluuv_ccc.server.telemetry import TelemetryHandler, TelemetryServer
from xluuv_ccc.utils import Mutex, setup_logging
from .xluuv_client import XluuvClient
from .cccserver_servicer import CCCServerServicer
from .xluuv_state import XluuvState


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--ccc-grpc-port",
        type=int,
        default=43001,
        required=False,
        help="Port on which to listen for gRPC requests from CCC clients (default: 43001)",
    )
    parser.add_argument(
        "--nmea-send-host",
        type=str,
        default="127.0.0.1",
        required=False,
        help="Host to which to send NMEA-over-UDP (default: 127.0.0.1)",
    )
    parser.add_argument(
        "--nmea-send-port",
        type=int,
        default=10110,
        required=False,
        help="Port to which to send NMEA-over-UDP (default: 10110)",
    )
    parser.add_argument(
        "--xluuv-report-port",
        type=int,
        default=43002,
        required=False,
        help="Port on which to listen for XLUUV UDP reports (default: 43002)",
    )
    parser.add_argument(
        "--xluuv-grpc-port",
        type=int,
        default=42001,
        required=False,
        help="Port to which to send gRPCs to XLUUV (default: 42001)",
    )
    parser.add_argument(
        "--xluuv-grpc-host",
        type=str,
        default="127.0.0.1",
        required=False,
        help="Host to which to send gRPCs to XLUUV (default: 127.0.0.1)",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Set logging to verbose, prints INFO messages",
    )
    parser.add_argument(
        "-vv",
        "--debug",
        action="store_true",
        help="Set logging to debug, prints DEBUG messages",
    )
    args = parser.parse_args()
    args.log_level = logging.WARNING
    if args.verbose:
        args.log_level = logging.INFO
    if args.debug:
        args.log_level = logging.DEBUG
    return args


def main() -> None:
    args = parse_args()
    setup_logging(args.log_level)

    # shared xluuv state
    xluuv_state = Mutex(XluuvState())

    # xluuv client for grpc calls to XLUUV
    xluuv_client = XluuvClient(args.xluuv_grpc_host, args.xluuv_grpc_port)

    # nmea client for OpenCPN/Chart plotter
    nmea_client = NmeaClient(args.nmea_send_host, args.nmea_send_port)

    # protobuf server for XLUUV telemetry
    telemetry_server = TelemetryServer(
        ("0.0.0.0", args.xluuv_report_port), TelemetryHandler, xluuv_state, nmea_client
    )
    telemetry_thread = threading.Thread(target=telemetry_server.serve_forever)
    telemetry_thread.daemon = True
    telemetry_thread.start()

    # grpc service for CCC clients
    grpc_server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    xluuv_ccc.messages_pb2_grpc.add_CCCServerServicer_to_server(
        CCCServerServicer(
            xluuv_client=xluuv_client,
            xluuv_state=xluuv_state,
        ),
        grpc_server,
    )
    grpc_server.add_insecure_port(f"0.0.0.0:{args.ccc_grpc_port}")
    grpc_server.start()

    try:
        grpc_server.wait_for_termination()
    except KeyboardInterrupt:
        grpc_server.stop(grace=None)


if __name__ == "__main__":
    main()
