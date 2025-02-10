#!/usr/bin/env python3
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

from google.protobuf.timestamp_pb2 import Timestamp

from xluuv_ccc.marmaths import distance_harvesine
from xluuv_ccc.messages_pb2 import (
    AutopilotState,
    ColregType,
    Coordinates,
    MissionStatus,
    Waypoint,
)


@dataclass
class XluuvState:
    last_ping: Optional[Timestamp] = None

    ap_state: Optional[AutopilotState] = None
    active_route_id: Optional[int] = None
    active_waypoint: Optional[Waypoint] = None
    ccc_estimated_wpt_dist: Optional[float] = None
    route_len: Optional[int] = None
    route_progress: Optional[int] = None
    route_name: Optional[str] = None
    tgt_speed: Optional[float] = None

    active_lp_id: Optional[int] = None
    lp_dist: Optional[float] = None
    lp_name: Optional[str] = None

    dp_name: Optional[str] = None
    tgt_depth: Optional[float] = None
    ccc_estimated_dp_dist: Optional[float] = None

    mission_name: Optional[str] = None
    mission_status: Optional[MissionStatus] = None
    mission_len: Optional[int] = None
    mission_progress: Optional[int] = None

    sen_gnss_1: Optional[Coordinates] = None
    sen_gnss_2: Optional[Coordinates] = None
    sen_gnss_3: Optional[Coordinates] = None
    gnss_ap: Optional[Coordinates] = None  # position as computed by autopilot

    sen_cog: Optional[float] = None
    sen_heading: Optional[float] = None
    sen_rot: Optional[float] = None
    sen_rpm_port: Optional[float] = None
    sen_rpm_stbd: Optional[float] = None
    sen_rudder_angle: Optional[float] = None
    sen_sog: Optional[float] = None
    sen_speed: Optional[float] = None
    sen_ship_depth: Optional[float] = None
    sen_depth_under_keel: Optional[float] = None
    sen_throttle_port: Optional[float] = None
    sen_throttle_stbd: Optional[float] = None
    sen_buoyancy: Optional[float] = None

    act_rudder_angle: Optional[float] = None
    act_engine_throttle_port: Optional[float] = None
    act_engine_throttle_stbd: Optional[float] = None
    act_thruster_throttle_bow: Optional[float] = None
    act_thruster_throttle_stern: Optional[float] = None
    act_ballast_tank_pump: Optional[float] = None

    colreg_status: Optional[ColregType] = None
    colreg_mmsi: Optional[int] = None

    def update_ping(self) -> None:
        if self.last_ping is None:
            self.last_ping = Timestamp()
        self.last_ping.GetCurrentTime()

    def update_estimated_wpt_dist(self) -> None:
        if self.active_waypoint is None or self.gnss_ap is None:
            return
        tgt = self.active_waypoint.coords
        pos = self.gnss_ap
        if tgt.latitude == 0.0 and tgt.longitude == 0.0:
            # assume we never want to navigate to null island
            return

        self.ccc_estimated_wpt_dist = distance_harvesine(
            pos.latitude, pos.longitude, tgt.latitude, tgt.longitude
        )

    def update_estimated_dp_delta(self) -> None:
        if self.tgt_depth is None or self.sen_ship_depth is None:
            return

        self.ccc_estimated_dp_dist = self.tgt_depth - self.sen_ship_depth
