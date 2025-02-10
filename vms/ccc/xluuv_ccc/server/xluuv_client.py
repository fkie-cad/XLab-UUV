#!/usr/bin/env python3
import logging
import queue
from threading import Lock
import threading
import time
from queue import Queue
from typing import Callable, List, Optional, Union

import grpc
from google.protobuf.empty_pb2 import Empty

from xluuv_ccc.messages_pb2 import (
    ApCommand,
    ApMission,
    ApRoute,
    DiveProcedure,
    DiveProcedureId,
    LoiterPosition,
    LoiterPositionId,
    MsCommand,
    RouteId,
)
from xluuv_ccc.messages_pb2_grpc import XLUUVStub
from xluuv_ccc.utils import Mutex

XluuvRPCArg = Union[
    ApCommand,
    ApRoute,
    ApMission,
    LoiterPosition,
    DiveProcedure,
    Empty,
    RouteId,
    LoiterPositionId,
    DiveProcedureId,
    MsCommand,
]


class XluuvRequestTimeout(Exception):
    def __init__(self, *args: object) -> None:
        super().__init__(*args)


class XluuvRequest:
    def __init__(
        self, name: str, rpc: Callable, max_retries: int, max_backoff: float
    ) -> None:
        self._name = name
        self._backoff_increment = 1.7
        self._rpc = rpc
        self._retries = 0
        self._backoff = max(0, min(max_backoff, 1))
        self._max_backoff = max_backoff
        self._max_retries = max_retries
        self._next_try = 0
        self._rpc_arg: Optional[XluuvRPCArg] = None

    def _reset(self) -> None:
        self._retries = 0
        self._next_try = 0
        self._backoff = max(0, min(self._max_backoff, 1))
        self._rpc_arg = None

    def update_arg(self, rpc_arg: XluuvRPCArg) -> None:
        self._reset()
        self._rpc_arg = rpc_arg

    def send(self) -> bool:
        if self._rpc_arg is None:
            return False
        now = time.time()
        if now < self._next_try:
            return False
        if self._max_retries > 0 and self._retries > self._max_retries:
            err = f"exceeded max retries for {self._name}"
            logging.warning(err)
            self._reset()
            raise XluuvRequestTimeout(err)
        try:
            logging.debug(f"trying RPC '{self._name}'")
            self._rpc(self._rpc_arg)
            # call succeeded, disable this request
            self._reset()
            self.rpc_arg = None
            return True
        except grpc.RpcError as rpc_error:
            warn = f"RPC: request {self._name} failed"
            if isinstance(rpc_error, grpc.Call):
                if rpc_error.code() == grpc.StatusCode.UNAVAILABLE:
                    warn += ", the XLUUV gRPC service is unavailable"
                else:
                    warn += f" with status code {rpc_error.code()}: {rpc_error.details}"
            logging.warning(warn)
        # call failed, set up retry
        self._next_try = now + self._backoff
        logging.warning(f"retrying in {self._next_try - now} seconds")
        self._backoff = min(self._backoff * self._backoff_increment, self._max_backoff)
        self._retries += 1
        return False


class XluuvClient:
    def __init__(
        self, xluuv_host: str, xluuv_port: str, check_interval: float = 0.3
    ) -> None:
        self._grpc_channel = grpc.insecure_channel(f"{xluuv_host}:{xluuv_port}")
        self._xluuv_server = XLUUVStub(self._grpc_channel)
        self._check_interval = check_interval
        self._call_queue = Queue()

        # requests that are currently being tried
        self._mixed_request_queue: List[
            Union[Mutex[XluuvRequest], Mutex[List[XluuvRequest]]]
        ] = []

        # single requests are overwritten by newer ones as they come in
        self._ap_command_req = Mutex(
            XluuvRequest(
                "SendApCommand",
                self._xluuv_server.SendApCommand,
                max_retries=10,
                max_backoff=30,
            )
        )
        self._mission_info_req = Mutex(
            XluuvRequest(
                "SendMission",
                self._xluuv_server.SendMission,
                max_retries=20,
                max_backoff=120,
            )
        )
        self._ms_command_req = Mutex(
            XluuvRequest(
                "SendMissionCommand",
                self._xluuv_server.SendMissionCommand,
                max_retries=10,
                max_backoff=30,
            )
        )
        self._activate_route_req = Mutex(
            XluuvRequest(
                "ActivateRoute",
                self._xluuv_server.ActivateRoute,
                max_retries=10,
                max_backoff=30,
            )
        )
        self._activate_lp_req = Mutex(
            XluuvRequest(
                "ActivateLoiterPosition",
                self._xluuv_server.ActivateLoiterPosition,
                max_retries=10,
                max_backoff=30,
            )
        )
        self._activate_dp_req = Mutex(
            XluuvRequest(
                "ActivateDiveProcedure",
                self._xluuv_server.ActivateDiveProcedure,
                max_retries=10,
                max_backoff=30,
            )
        )

        # for some request types we allow multiple requests to be queued
        # (e.g. transmitting multiple route information requests)
        # the queue is stricly FIFO: a request is only tried when all others
        # that came before it timed out or succeeded
        self._route_info_reqs: Mutex[List[XluuvRequest]] = Mutex([])
        self._lp_info_reqs: Mutex[List[XluuvRequest]] = Mutex([])
        self._dp_info_reqs: Mutex[List[XluuvRequest]] = Mutex([])

        # queue of all request types
        self._mixed_request_queue = [
            self._ap_command_req,
            self._ms_command_req,
            self._activate_route_req,
            self._activate_lp_req,
            self._activate_dp_req,
            self._mission_info_req,
            self._route_info_reqs,
            self._lp_info_reqs,
            self._dp_info_reqs,
        ]

        self._stop = False
        self._stop_lock = Lock()

    def start(self) -> None:
        logging.debug("starting xluuv client")
        with self._stop_lock:
            self._stop = False
        req_thread = threading.Thread(target=self._execute_requests)
        req_thread.daemon = True
        req_thread.start()

    def stop(self) -> None:
        with self._stop_lock:
            self._stop = True

    def _execute_requests(self) -> None:
        while True:
            with self._stop_lock:
                if self._stop:
                    break

            time.sleep(self._check_interval)
            # send all request types
            for i in range(len(self._mixed_request_queue)):
                # execute the latest calls to ensure we're working on up-to-date requests
                while True:
                    try:
                        call, arg = self._call_queue.get(block=False)
                        call(arg)
                    except queue.Empty:
                        break
                with self._mixed_request_queue[i].lock() as req:
                    if isinstance(req, list):
                        # queue of RPCs
                        while len(req) > 0:
                            try:
                                result = req[0].send()
                                if not result:
                                    # call is still blocking others
                                    break
                                else:
                                    # call suceeded, remove from req
                                    req.pop(0)
                            except XluuvRequestTimeout:
                                req.pop(0)
                    else:
                        # single RPC overwritten by newer ones
                        try:
                            req.send()
                        except XluuvRequestTimeout:
                            pass

    def _handle_queue(self, queue: List[XluuvRequest]):
        if len(queue) == 0:
            return
        try:
            result = queue[0].send()
            if result:
                # call suceeded, remove from queue
                queue.pop(0)
        except XluuvRequestTimeout:
            queue.pop(0)

    def send_mission(self, ap_mission: ApMission) -> None:
        self._call_queue.put((self._send_mission, ap_mission))

    def _send_mission(self, ap_mission: ApMission) -> None:
        with self._mission_info_req.lock() as request:
            request.update_arg(ap_mission)

    def send_loiter_position(self, loiter_position: LoiterPosition) -> None:
        self._call_queue.put((self._send_loiter_position, loiter_position))

    def _send_loiter_position(self, loiter_position: LoiterPosition) -> None:
        logging.debug("add send loiter position to request queue")
        request = XluuvRequest(
            "SendLoiterPosition",
            self._xluuv_server.SendLoiterPosition,
            max_retries=20,
            max_backoff=120,
        )
        request.update_arg(loiter_position)
        with self._lp_info_reqs.lock() as request_list:
            request_list.append(request)

    def send_route(self, ap_route: ApRoute) -> None:
        self._call_queue.put((self._send_route, ap_route))

    def _send_route(self, ap_route: ApRoute) -> None:
        request = XluuvRequest(
            "SendRoute",
            self._xluuv_server.SendRoute,
            max_retries=20,
            max_backoff=120,
        )
        request.update_arg(ap_route)
        with self._route_info_reqs.lock() as request_list:
            request_list.append(request)

    def send_dive_procedure(self, dive_procedure: DiveProcedure) -> None:
        self._call_queue.put((self._send_dive_procedure, dive_procedure))

    def _send_dive_procedure(self, dive_procedure: DiveProcedure) -> None:
        request = XluuvRequest(
            "SendDiveProcedure",
            self._xluuv_server.SendDiveProcedure,
            max_retries=20,
            max_backoff=120,
        )
        request.update_arg(dive_procedure)
        with self._dp_info_reqs.lock() as request_list:
            request_list.append(request)

    def send_ms_command(self, ms_command: MsCommand) -> None:
        self._call_queue.put((self._send_ms_command, ms_command))

    def _send_ms_command(self, ms_command: MsCommand) -> None:
        with self._ms_command_req.lock() as request:
            request.update_arg(ms_command)

    def send_ap_command(self, ap_command: ApCommand) -> None:
        self._call_queue.put((self._send_ap_command, ap_command))

    def _send_ap_command(self, ap_command: ApCommand) -> None:
        with self._ap_command_req.lock() as request:
            request.update_arg(ap_command)

    def activate_route(self, route_id: RouteId) -> None:
        self._call_queue.put((self._activate_route, route_id))

    def _activate_route(self, route_id: RouteId) -> None:
        with self._activate_route_req.lock() as request:
            request.update_arg(route_id)

    def activate_loiter_position(self, loiter_position_id: LoiterPositionId) -> None:
        self._call_queue.put((self._activate_loiter_position, loiter_position_id))

    def _activate_loiter_position(self, loiter_position_id: LoiterPositionId) -> None:
        with self._activate_lp_req.lock() as request:
            request.update_arg(loiter_position_id)

    def activate_dive_procedure(self, dive_procedure_id: DiveProcedureId) -> None:
        self._call_queue.put((self._activate_dive_procedure, dive_procedure_id))

    def _activate_dive_procedure(self, dive_procedure_id: DiveProcedureId) -> None:
        with self._activate_dp_req.lock() as request:
            request.update_arg(dive_procedure_id)
