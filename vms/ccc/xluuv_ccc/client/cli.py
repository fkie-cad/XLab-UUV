#!/usr/bin/env python3
import argparse
import logging
import json
import fileinput
import google.protobuf.message
import time
from xluuv_ccc.utils import flatten_dict, setup_logging
from google.protobuf.empty_pb2 import Empty
from google.protobuf.json_format import MessageToJson, MessageToDict
from .client import RPC, CccClient, Request
from typing import Optional
from xluuv_ccc.messages_pb2 import (
    AggregatedXluuvStatus,
    ApCommand,
    ApMissionJson,
    ApMissionZip,
    ApRouteXML,
    AutopilotCommand,
    DiveProcedureId,
    DiveProcedureJson,
    LoiterPositionId,
    LoiterPositionJson,
    MissionCommand,
    MsCommand,
    RouteId,
)


def _available_rpcs() -> str:
    return ", ".join([rpc.name for rpc in RPC])


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--ccc-grpc-host",
        type=str,
        default="127.0.0.1",
        required=False,
        help="Host to which to send gRPC requests for CCC server (default: 127.0.0.1)",
    )
    parser.add_argument(
        "--ccc-grpc-port",
        type=int,
        default=43001,
        required=False,
        help="Port to which to send gRPC requests for CCC server (default: 43001)",
    )
    parser.add_argument(
        "-p",
        "--poll",
        required=False,
        type=float,
        help="interval with which to poll the server for current status, if ommitted, no polling will occur",
    )
    parser.add_argument(
        "-r",
        "--rpc",
        required=False,
        help=f"RPC to send to the ccc-server, must be one of {_available_rpcs()}",
    )
    parser.add_argument(
        "--rpc-arg",
        required=False,
        help="RPC argument, takes precedence over --rpc-arg-file",
    )
    parser.add_argument(
        "--rpc-arg-file",
        required=False,
        help="file containing the RPC argument, can be '-' for stdin",
    )
    parser.add_argument(
        "--print-as",
        required=False,
        default="text",
        help="Format to use for printing response, can be one of text,json",
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


class CliClient:
    def __init__(
        self,
        grpc_host: str,
        grpc_port: int,
        print_as: str,
        poll: bool,
        poll_interval: float,
    ) -> None:
        callback = self._print_response
        self._awaits = 1
        self._poll = poll
        self._request: Optional[Request] = None
        if print_as == "json":
            callback = self._print_response_as_json
        self._ccc_client = CccClient(
            callback, grpc_host, grpc_port, poll=poll, poll_interval=poll_interval
        )

    def run(self):
        pass
        try:
            self._ccc_client.start()
            if self._request is not None:
                self._ccc_client.enqueue(self._request)
            while True:
                if self._awaits <= 0 and not self._poll:
                    self._ccc_client.terminate()
                    break
                time.sleep(0.3)
        except KeyboardInterrupt:
            self._ccc_client.terminate()

    def set_cmd(self, rpc_str: Optional[str], rpc_arg_raw: bytes) -> None:
        if rpc_str is None or rpc_arg_raw == b"":
            return
        rpc = RPC[rpc_str]
        match rpc:
            case RPC.SEND_MISSION_ZIP:
                self._request = Request(
                    rpc, message=ApMissionZip(mission_zip=rpc_arg_raw)
                )
            case RPC.SEND_MISSION_JSON:
                self._request = Request(
                    rpc,
                    message=ApMissionJson(
                        mission_json=json.loads(rpc_arg_raw.decode())
                    ),
                )
            case RPC.SEND_LP_JSON:
                self._request = Request(
                    rpc,
                    message=LoiterPositionJson(
                        lp_json=json.loads(rpc_arg_raw.decode())
                    ),
                )
            case RPC.SEND_DP_JSON:
                self._request = Request(
                    rpc,
                    message=DiveProcedureJson(dp_json=json.loads(rpc_arg_raw.decode())),
                )
            case RPC.SEND_ROUTE_XML:
                route_obj = json.loads(rpc_arg_raw.decode())
                self._request = Request(
                    rpc,
                    message=ApRouteXML(id=route_obj["id"], route_xml=route_obj["xml"]),
                )
            case RPC.GET_XLUUV_STATUS:
                self._request = Request(rpc, message=Empty())
            case RPC.SEND_AP_COMMAND:
                cmd = AutopilotCommand.Value(rpc_arg_raw)
                self._request = Request(rpc, message=ApCommand(command=cmd))
            case RPC.SEND_MISSION_COMMAND:
                cmd = MissionCommand.Value(rpc_arg_raw)
                self._request = Request(rpc, message=MsCommand(command=cmd))
            case RPC.ACTIVATE_ROUTE:
                self._request = Request(rpc, message=RouteId(id=int(rpc_arg_raw)))
            case RPC.ACTIVATE_LP:
                self._request = Request(
                    rpc, message=LoiterPositionId(id=int(rpc_arg_raw))
                )
            case RPC.ACTIVATE_DP:
                self._request = Request(
                    rpc, message=DiveProcedureId(id=int(rpc_arg_raw))
                )
            case _:
                raise NotImplementedError(
                    f"Handling of RPC {rpc_str} has not been implemented"
                )

    def _print_response(self, rpc_response: google.protobuf.message.Message) -> None:
        self._awaits -= 1
        print(rpc_response)

    def _print_response_as_json(
        self, rpc_response: google.protobuf.message.Message
    ) -> None:
        self._awaits -= 1
        print(
            MessageToJson(
                rpc_response,
                including_default_value_fields=True,
                preserving_proto_field_name=True,
            )
        )


def main() -> None:
    args = parse_args()
    setup_logging(args.log_level)
    poll = False
    poll_interval = 0

    if args.poll is not None:
        poll = True
        poll_interval = args.poll

    rpc_arg = b""
    if args.rpc_arg is not None:
        rpc_arg = bytes(args.rpc_arg, "utf-8")
    elif args.rpc_arg_file is not None:
        with fileinput.input((args.rpc_arg_file), mode="rb") as fd:
            rpc_arg = b"".join(list(fd))

    client = CliClient(
        args.ccc_grpc_host,
        args.ccc_grpc_port,
        args.print_as.lower(),
        poll,
        poll_interval,
    )
    client.set_cmd(args.rpc, rpc_arg)
    client.run()


if __name__ == "__main__":
    main()
