import logging
import queue
import threading
import time
from dataclasses import dataclass
from enum import Enum
from queue import Queue
from typing import Callable, Optional

import google.protobuf.message
import grpc
from google.protobuf.empty_pb2 import Empty

from xluuv_ccc.messages_pb2_grpc import CCCServerStub


class RPC(Enum):
    SEND_MISSION_ZIP = 0
    SEND_MISSION_JSON = 1
    SEND_LP_JSON = 2
    SEND_DP_JSON = 3
    SEND_ROUTE_XML = 4
    GET_XLUUV_STATUS = 5
    SEND_AP_COMMAND = 6
    SEND_MISSION_COMMAND = 7
    ACTIVATE_ROUTE = 8
    ACTIVATE_LP = 9
    ACTIVATE_DP = 10


@dataclass
class Request:
    rpc: RPC
    message: google.protobuf.message.Message


class CmdWorker:
    def __init__(
        self,
        cmd_queue: Queue,
        server_addr: str,
        server_port: int,
        callback: Callable,
    ) -> None:
        self._cmd_queue = cmd_queue
        self._exit = False
        self._exit_lock = threading.Lock()
        self._grpc_channel = grpc.insecure_channel(f"{server_addr}:{server_port}")
        self._ccc_server = CCCServerStub(self._grpc_channel)
        self._callback = callback

    def run(self):
        while True:
            with self._exit_lock:
                if self._exit:
                    break
            try:
                request: Request = self._cmd_queue.get(block=True, timeout=0.5)
                response = None
                try:
                    match request.rpc:
                        case RPC.SEND_MISSION_ZIP:
                            response = self._ccc_server.SendMissionZip(request.message)
                        case RPC.SEND_MISSION_JSON:
                            response = self._ccc_server.SendMissionJson(request.message)
                        case RPC.SEND_LP_JSON:
                            response = self._ccc_server.SendLoiterPositionJson(
                                request.message
                            )
                        case RPC.SEND_DP_JSON:
                            response = self._ccc_server.SendDiveProcedureJson(
                                request.message
                            )
                        case RPC.SEND_ROUTE_XML:
                            response = self._ccc_server.SendRouteXml(request.message)
                        case RPC.GET_XLUUV_STATUS:
                            response = self._ccc_server.GetXluuvStatus(request.message)
                        case RPC.SEND_AP_COMMAND:
                            response = self._ccc_server.SendApCommand(request.message)
                        case RPC.SEND_MISSION_COMMAND:
                            response = self._ccc_server.SendMissionCommand(
                                request.message
                            )
                        case RPC.ACTIVATE_ROUTE:
                            response = self._ccc_server.ActivateRoute(request.message)
                        case RPC.ACTIVATE_LP:
                            response = self._ccc_server.ActivateLoiterPosition(
                                request.message
                            )
                        case RPC.ACTIVATE_DP:
                            response = self._ccc_server.ActivateDiveProcedure(
                                request.message
                            )
                    if response is None:
                        logging.warning("RPC: request did not match any RPC")
                except grpc.RpcError as rpc_error:
                    warn = "RPC: request failed"
                    if isinstance(rpc_error, grpc.Call):
                        if rpc_error.code() == grpc.StatusCode.UNAVAILABLE:
                            warn += ", the CCC gRPC service is unavailable"
                        else:
                            warn += f" with status code {rpc_error.code()}: {rpc_error.details}"

                    logging.warning(warn)

                else:
                    self._callback(response)
            except queue.Empty:
                pass

    def terminate(self):
        with self._exit_lock:
            self._exit = True


class PollWorker:
    def __init__(
        self,
        cmd_queue: Queue,
        interval: float,
    ) -> None:
        self._interval = interval
        self._cmd_queue = cmd_queue
        self._exit = False
        self._exit_lock = threading.Lock()

    def run(self):
        while True:
            time.sleep(self._interval)
            with self._exit_lock:
                if self._exit:
                    break
            request = Request(rpc=RPC.GET_XLUUV_STATUS, message=Empty())
            self._cmd_queue.put(request, block=True, timeout=None)

    def terminate(self):
        with self._exit_lock:
            self._exit = True


class CccClient:
    def __init__(
        self,
        cmd_callback: Callable,
        server_addr: str,
        server_port: int,
        poll: bool,
        poll_interval: float = 1,
    ) -> None:
        self._poll = poll
        self._poll_interval = poll_interval
        self._cmd_queue = Queue()

        # create workers
        self._cmd_worker = CmdWorker(
            self._cmd_queue, server_addr, server_port, cmd_callback
        )
        self._poll_worker: Optional[PollWorker] = None
        if poll:
            self._poll_worker = PollWorker(self._cmd_queue, poll_interval)

    def start(self) -> None:
        # start workers
        cmd_thread = threading.Thread(target=self._cmd_worker.run)
        cmd_thread.daemon = True
        cmd_thread.start()
        if self._poll_worker is not None:
            poll_thread = threading.Thread(target=self._poll_worker.run)
            poll_thread.daemon = True
            poll_thread.start()

    def terminate(self) -> None:
        self._cmd_worker.terminate()
        if self._poll_worker is not None:
            self._poll_worker.terminate()

    def enqueue(self, request: Request) -> None:
        self._cmd_queue.put(request)
