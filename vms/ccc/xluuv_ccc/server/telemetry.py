from socketserver import (
    BaseRequestHandler,
    DatagramRequestHandler,
    UDPServer,
)
from typing import Any, Callable, Tuple

import logging

from xluuv_ccc.messages_pb2 import TelemetryReport
from xluuv_ccc.server.xluuv_state import XluuvState
from xluuv_ccc.server.nmea_client import NmeaClient
from xluuv_ccc.utils import Mutex


class TelemetryServer(UDPServer):
    def __init__(
        self,
        server_address: Tuple[str, int],
        RequestHandlerClass: Callable[[Any, Any, Any], BaseRequestHandler],
        xluuv_state: Mutex[XluuvState],
        nmea_client: NmeaClient,
        bind_and_activate: bool = True,
    ) -> None:
        self.xluuv_state = xluuv_state
        self.nmea_client = nmea_client
        super().__init__(server_address, RequestHandlerClass, bind_and_activate)


class TelemetryHandler(DatagramRequestHandler):
    server: TelemetryServer

    def __init__(
        self,
        request,
        client_address,
        server: TelemetryServer,
    ) -> None:
        super().__init__(request, client_address, server)

    def handle(self) -> None:
        datagram = self.request[0]
        report_wrapper = TelemetryReport()
        report_wrapper.ParseFromString(datagram)

        match report_wrapper.WhichOneof("report"):
            case "sensor_report":
                report = report_wrapper.sensor_report
                logging.debug(f"got sensor report {report}")
                with self.server.xluuv_state.lock() as xluuv_state:
                    xluuv_state.update_ping()
                    xluuv_state.sen_gnss_1 = report.gnss_1
                    xluuv_state.sen_gnss_2 = report.gnss_2
                    xluuv_state.sen_gnss_3 = report.gnss_3
                    xluuv_state.sen_cog = report.cog
                    xluuv_state.sen_heading = report.heading
                    xluuv_state.sen_rot = report.rot
                    xluuv_state.sen_rpm_port = report.rpm_port
                    xluuv_state.sen_rpm_stbd = report.rpm_stbd
                    xluuv_state.sen_rudder_angle = report.rudder_angle
                    xluuv_state.sen_sog = report.sog
                    xluuv_state.sen_speed = report.speed
                    xluuv_state.sen_ship_depth = report.ship_depth
                    xluuv_state.sen_depth_under_keel = report.depth_under_keel
                    xluuv_state.sen_throttle_port = report.throttle_port
                    xluuv_state.sen_throttle_stbd = report.throttle_stbd
                    xluuv_state.sen_buoyancy = report.buoyancy
                    xluuv_state.update_estimated_wpt_dist()
                    xluuv_state.update_estimated_dp_delta()

                self.server.nmea_client.handle_sensors(report)

            case "act_cmd_report":
                report = report_wrapper.act_cmd_report
                with self.server.xluuv_state.lock() as xluuv_state:
                    xluuv_state.update_ping()
                    xluuv_state.act_rudder_angle = report.rudder_angle
                    xluuv_state.act_ballast_tank_pump = report.ballast_tank_pump
                    xluuv_state.act_engine_throttle_port = report.engine_throttle_port
                    xluuv_state.act_engine_throttle_stbd = report.engine_throttle_stbd
                    xluuv_state.act_thruster_throttle_bow = report.thruster_throttle_bow
                    xluuv_state.act_thruster_throttle_stern = (
                        report.thruster_throttle_stern
                    )

            case "ap_report":
                report = report_wrapper.ap_report
                with self.server.xluuv_state.lock() as xluuv_state:
                    xluuv_state.update_ping()
                    xluuv_state.ap_state = report.state
                    xluuv_state.active_waypoint = report.active_waypoint
                    xluuv_state.active_route_id = report.active_route_id
                    xluuv_state.route_progress = report.route_progress
                    xluuv_state.route_len = report.route_len
                    xluuv_state.route_name = report.route_name
                    xluuv_state.tgt_speed = report.tgt_speed
                    xluuv_state.lp_name = report.lp_name
                    xluuv_state.lp_dist = report.lp_dist
                    xluuv_state.active_lp_id = report.active_lp_id
                    xluuv_state.dp_name = report.dp_name
                    xluuv_state.tgt_depth = report.tgt_depth
                    xluuv_state.gnss_ap = report.gnss_ap
                    xluuv_state.update_estimated_wpt_dist()
                    xluuv_state.update_estimated_dp_delta()

                self.server.nmea_client.handle_ap_report(report)

            case "colreg_report":
                report = report_wrapper.colreg_report
                with self.server.xluuv_state.lock() as xluuv_state:
                    xluuv_state.update_ping()
                    xluuv_state.colreg_mmsi = report.tgt_mmsi
                    xluuv_state.colreg_status = report.type

                self.server.nmea_client.handle_colreg_report(report)

            case "mission_report":
                report = report_wrapper.mission_report
                with self.server.xluuv_state.lock() as xluuv_state:
                    xluuv_state.update_ping()
                    xluuv_state.mission_name = report.name
                    xluuv_state.mission_status = report.status
                    xluuv_state.mission_len = report.length
                    xluuv_state.mission_progress = report.progress

            case "nmea_report":
                report = report_wrapper.nmea_report
                self.server.nmea_client.handle_wrapped_nmea(report)

            case None:
                pass
