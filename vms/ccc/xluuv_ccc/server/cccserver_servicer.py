#!/usr/bin/env python3
from __future__ import annotations

from dataclasses import dataclass
import io
import json
import logging
import xml.etree.ElementTree
import zipfile
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

from google.protobuf.empty_pb2 import Empty
from xluuv_ccc.marmaths import opencpn_string_to_dec

import xluuv_ccc.messages_pb2_grpc
from xluuv_ccc.messages_pb2 import (
    AS_DISABLED,
    JE_NO_ERROR,
    JE_PARSE_ERROR,
    MZE_JSON_ERROR,
    MZE_KEY_ERROR,
    MZE_MISSING_DP,
    MZE_MISSING_MISSION,
    MZE_MISSING_ROUTE,
    MZE_NO_ERROR,
    MZE_XML_ERROR,
    MZE_ZIP_ERROR,
    RXE_INVALID_XML,
    RXE_MISSING_FIELD,
    RXE_NO_ERROR,
    AggregatedXluuvStatus,
    ApCommand,
    ApMission,
    ApMissionJson,
    ApMissionZip,
    ApRoute,
    ApRouteXML,
    AutopilotCommand,
    Coordinates,
    DiveProcedure,
    DiveProcedureId,
    DiveProcedureJson,
    JsonError,
    JsonErrorMessage,
    LoiterPosition,
    LoiterPositionId,
    LoiterPositionJson,
    MissionZipError,
    MissionZipErrorMessage,
    MsCommand,
    RouteId,
    RouteXMLError,
    RouteXMLErrorMessage,
    Waypoint,
)
from xluuv_ccc.server.xluuv_client import XluuvClient
from xluuv_ccc.server.xluuv_state import XluuvState
from xluuv_ccc.utils import Mutex


def _parse_route_xml(
    aproute_xml: ApRouteXML,
) -> Tuple[Optional[ApRoute], RouteXMLError]:
    try:
        xml_root = xml.etree.ElementTree.fromstring(aproute_xml.route_xml)
    except xml.etree.ElementTree.ParseError:
        return (None, RXE_INVALID_XML)
    id = aproute_xml.id
    res = ApRoute(id=id)

    # Assume the namespaces are always the same
    OCPN_NS = "{http://www.opencpn.org}"
    DEFAULT_NS = "{http://www.topografix.com/GPX/1/1}"

    route = xml_root.find(f"{DEFAULT_NS}rte")

    if route is None:
        return (None, RXE_MISSING_FIELD)

    name = route.find(f"{DEFAULT_NS}name")
    if name is None or name.text is None:
        return (None, RXE_MISSING_FIELD)
    res.name = name.text

    extensions = route.find(f"{DEFAULT_NS}extensions")
    if extensions is None:
        return (None, RXE_MISSING_FIELD)
    planned_speed = extensions.find(f"{OCPN_NS}planned_speed")
    if planned_speed is None or planned_speed.text is None:
        return (None, RXE_MISSING_FIELD)
    res.planned_speed = float(planned_speed.text)

    rtepts = route.findall(f"{DEFAULT_NS}rtept")
    if len(rtepts) == 0:
        # don't allow empty routes, since AP will reject them
        return (None, RXE_MISSING_FIELD)

    for i, rtept in enumerate(rtepts):
        wp = Waypoint(
            coords=Coordinates(
                latitude=float(rtept.attrib["lat"]),
                longitude=float(rtept.attrib["lon"]),
            )
        )

        wp_name = rtept.find(f"{DEFAULT_NS}name")
        if wp_name is not None and wp_name.text is not None:
            wp.name = wp_name.text
        else:
            wp.name = f"{i:03}"

        res.waypoints.append(wp)
    return (res, RXE_NO_ERROR)


def _parse_mission_json(
    mission_json: ApMissionJson,
) -> Tuple[Optional[ApMission], JsonError]:
    res = None
    mission_d: Dict[str, Any]
    try:
        mission_d = json.loads(mission_json.mission_json)
    except json.JSONDecodeError:
        return (res, JE_PARSE_ERROR)

    try:
        mission = mission_d["mission"]
        res = ApMission(id=mission["id"], name=mission["name"])
        for item in mission["items"]:
            ap_item = ApMission.ApMissionItem()
            # item duration
            if "until_completion" in item.keys():
                ap_item.until_completion = True
            else:
                ap_item.until_timeout = int(item["until_timeout"])

            # type of action
            if "activate_route" in item.keys():
                ap_item.act_route_id = int(item["activate_route"])
            elif "activate_loiterpos" in item.keys():
                ap_item.act_loiterpos_id = int(item["activate_loiterpos"])
            elif "activate_diveproc" in item.keys():
                ap_item.act_diveproc_id = int(item["activate_diveproc"])
            else:
                # conversion from int to AutopilotCommand
                ap_item.set_ap_cmd = AutopilotCommand.Value(item["set_ap_cmd"])

            res.ap_mission_items.append(ap_item)

    except (KeyError, ValueError):
        return (None, JE_PARSE_ERROR)

    return (res, JE_NO_ERROR)


def _parse_lp_json(
    lp_json: LoiterPositionJson,
) -> Tuple[Optional[LoiterPosition], JsonError]:
    res = None
    lp_d = Dict[str, Any]
    try:
        lp_d = json.loads(lp_json.lp_json)
    except json.JSONDecodeError:
        return (res, JE_PARSE_ERROR)

    try:
        lat: Union[float, str] = lp_d["lat"]
        lon: Union[float, str] = lp_d["lon"]
        if isinstance(lat, str):
            lat, _ = opencpn_string_to_dec(lat)
        if isinstance(lon, str):
            lon, _ = opencpn_string_to_dec(lon)
        id = int(lp_d["id"])
        name = f"LP-{id}"
        if "name" in lp_d.keys():
            name = lp_d["name"]
        res = LoiterPosition(
            id=id,
            position=Waypoint(
                coords=Coordinates(latitude=float(lat), longitude=float(lon)), name=name
            ),
        )

        res.bearing = float(lp_d["bearing"])

    except (ValueError, KeyError):
        return (None, JE_PARSE_ERROR)

    return (res, JE_NO_ERROR)


def _parse_dp_json(
    dp_json: DiveProcedureJson,
) -> Tuple[Optional[DiveProcedure], JsonError]:
    res = None
    dp_d: Dict[str, Any]
    try:
        dp_d = json.loads(dp_json.dp_json)
    except json.JSONDecodeError:
        return (res, JE_PARSE_ERROR)

    try:
        res = DiveProcedure()
        res.id = int(dp_d["id"])
        res.name = str(dp_d["name"])
        res.depth = float(dp_d["depth"])

    except (ValueError, KeyError):
        return (None, JE_PARSE_ERROR)

    return (res, JE_NO_ERROR)


@dataclass
class _MissionBundle:
    mission: ApMission
    routes: List[ApRoute]
    dive_procedures: List[DiveProcedure]
    loiter_positions: List[LoiterPosition]


def _parse_mission_zip(
    mission_zip: bytes,
) -> Tuple[Optional[_MissionBundle], MissionZipError]:
    res = _MissionBundle(ApMission(), [], [], [])

    try:
        zf = zipfile.ZipFile(io.BytesIO(mission_zip), "r")
    except zipfile.BadZipFile:
        return (None, MZE_ZIP_ERROR)

    zf_content = zf.namelist()

    if "mission.json" not in zf_content:
        logging.info("could not find mission.json in the mission zip")
        return (None, MZE_MISSING_MISSION)

    mission_text = zipfile.Path(zf, at="mission.json").read_text()
    mission_json = ApMissionJson()
    mission_json.mission_json = mission_text
    mission, _ = _parse_mission_json(mission_json)
    if mission is None:
        logging.info("something went wrong parsing mission.json")
        return (None, MZE_JSON_ERROR)

    res.mission = mission

    mission_obj = json.loads(mission_text)

    try:
        routes = mission_obj["procedures"]["routes"]
        dive_procs = mission_obj["procedures"]["dive_procedures"]
        loiter_positions = mission_obj["procedures"]["loiter_positions"]
    except KeyError:
        return (None, MZE_KEY_ERROR)

    for route in routes:
        try:
            id = route["id"]
            given_path = str(Path(route["path"]))
        except KeyError:
            return (None, MZE_KEY_ERROR)

        if given_path not in zf_content:
            return (None, MZE_MISSING_ROUTE)

        route_xml = ApRouteXML()
        path = zipfile.Path(zf, at=given_path)
        route_xml.route_xml = path.read_text()
        route_xml.id = id
        route, _ = _parse_route_xml(route_xml)
        if route is None:
            return (None, MZE_XML_ERROR)
        res.routes.append(route)

    for dive_proc in dive_procs:
        try:
            id = dive_proc["id"]
            given_path = str(Path(dive_proc["path"]))
        except KeyError:
            return (None, MZE_KEY_ERROR)

        if given_path not in zf_content:
            return (None, MZE_MISSING_DP)

        dp_json = DiveProcedureJson()
        path = zipfile.Path(zf, at=given_path)
        dp_json.dp_json = path.read_text()
        dp, _ = _parse_dp_json(dp_json)
        if dp is None:
            logging.info(f"something went wrong parsing {given_path}")
            return (None, MZE_JSON_ERROR)
        # override id
        dp.id = id
        res.dive_procedures.append(dp)

    for loiter_position in loiter_positions:
        try:
            id = loiter_position["id"]
            given_path = str(Path(loiter_position["path"]))
        except KeyError:
            return (None, MZE_KEY_ERROR)

        if given_path not in zf_content:
            return (None, MZE_MISSING_DP)

        lp_json = LoiterPositionJson()
        path = zipfile.Path(zf, at=given_path)
        lp_json.lp_json = path.read_text()
        lp, _ = _parse_lp_json(lp_json)
        if lp is None:
            logging.info(f"something went wrong parsing {given_path}")
            return (None, MZE_JSON_ERROR)
        # override id
        lp.id = id
        res.loiter_positions.append(lp)

    return (res, MZE_NO_ERROR)


class CCCServerServicer(xluuv_ccc.messages_pb2_grpc.CCCServerServicer):
    def __init__(
        self, xluuv_client: XluuvClient, xluuv_state: Mutex[XluuvState]
    ) -> None:
        self._xluuv_client = xluuv_client
        self._xluuv_client.start()
        self._xluuv_state = xluuv_state
        super().__init__()

    def SendMissionZip(self, request: ApMissionZip, context) -> MissionZipErrorMessage:
        logging.info("received mission zip")
        mission_bundle, error = _parse_mission_zip(request.mission_zip)
        if mission_bundle is None:
            logging.info("mission zip is invalid, notifying client")
            return MissionZipErrorMessage(error=error)

        # send every item of the bundle
        self._xluuv_client.send_mission(mission_bundle.mission)
        for route in mission_bundle.routes:
            self._xluuv_client.send_route(route)
        for lp in mission_bundle.loiter_positions:
            self._xluuv_client.send_loiter_position(lp)
        for dp in mission_bundle.dive_procedures:
            self._xluuv_client.send_dive_procedure(dp)

        return MissionZipErrorMessage(error=MZE_NO_ERROR)

    def SendMissionJson(self, request: ApMissionJson, context) -> JsonErrorMessage:
        logging.info("received mission json")
        ap_mission, error = _parse_mission_json(request)
        if ap_mission is None:
            logging.info("mission json is invalid, notifying client")
            return JsonErrorMessage(error=error)

        self._xluuv_client.send_mission(ap_mission)
        return JsonErrorMessage(error=JE_NO_ERROR)

    def SendLoiterPositionJson(
        self, request: LoiterPositionJson, context
    ) -> JsonErrorMessage:
        logging.info("received loiter position json")
        lp, error = _parse_lp_json(request)
        if lp is None:
            logging.info("loiter position json is invalid, notifying client")
            return JsonErrorMessage(error=error)

        self._xluuv_client.send_loiter_position(lp)
        return JsonErrorMessage(error=JE_NO_ERROR)

    def SendDiveProcedureJson(
        self, request: DiveProcedureJson, context
    ) -> JsonErrorMessage:
        logging.info("received dive procedure json")
        dp, error = _parse_dp_json(request)
        if dp is None:
            logging.info("dive procedure json is invalid, notifying client")
            return JsonErrorMessage(error=error)

        self._xluuv_client.send_dive_procedure(dp)
        return JsonErrorMessage(error=JE_NO_ERROR)

    def SendRouteXml(self, request: ApRouteXML, context) -> RouteXMLErrorMessage:
        logging.info("received route xml")
        route, error = _parse_route_xml(request)
        if route is None:
            logging.info("route xml is invalid, notifying client")
            return RouteXMLErrorMessage(error=error)

        self._xluuv_client.send_route(route)
        return RouteXMLErrorMessage(error=RXE_NO_ERROR)

    def ActivateRoute(self, request: RouteId, context) -> Empty:
        logging.info("received route activation request")
        # transparently forward request to XLUUV
        self._xluuv_client.activate_route(request)
        return Empty()

    def ActivateLoiterPosition(self, request: LoiterPositionId, context) -> Empty:
        logging.info("recevied loiter position activation request")
        # transparently forward request to XLUUV
        self._xluuv_client.activate_loiter_position(request)
        return Empty()

    def ActivateDiveProcedure(self, request: DiveProcedureId, context) -> Empty:
        logging.info("received dive procedure activation request")
        # transparently forward request to XLUUV
        self._xluuv_client.activate_dive_procedure(request)
        return Empty()

    def GetXluuvStatus(self, request: Empty, context) -> AggregatedXluuvStatus:
        logging.debug("received xluuv status poll")
        with self._xluuv_state.lock() as xluuv_state:
            return AggregatedXluuvStatus(
                last_ping=xluuv_state.last_ping,
                ap_state=xluuv_state.ap_state,
                ap_active_route=xluuv_state.active_route_id,
                ap_active_waypoint=xluuv_state.active_waypoint,
                ccc_estimated_wpt_dist=xluuv_state.ccc_estimated_wpt_dist,
                ap_route_progress=xluuv_state.route_progress,
                ap_route_len=xluuv_state.route_len,
                ap_route_name=xluuv_state.route_name,
                active_lp_id=xluuv_state.active_lp_id,
                lp_dist=xluuv_state.lp_dist,
                lp_name=xluuv_state.lp_name,
                ccc_estimated_dp_dist=xluuv_state.ccc_estimated_dp_dist,
                tgt_speed=xluuv_state.tgt_speed,
                dp_name=xluuv_state.dp_name,
                tgt_depth=xluuv_state.tgt_depth,
                gnss_ap=xluuv_state.gnss_ap,
                ms_name=xluuv_state.mission_name,
                ms_status=xluuv_state.mission_status,
                ms_len=xluuv_state.mission_len,
                ms_progress=xluuv_state.mission_progress,
                sensor_gnss_1=xluuv_state.sen_gnss_1,
                sensor_gnss_2=xluuv_state.sen_gnss_2,
                sensor_gnss_3=xluuv_state.sen_gnss_3,
                sensor_cog=xluuv_state.sen_cog,
                sensor_heading=xluuv_state.sen_heading,
                sensor_rot=xluuv_state.sen_rot,
                sensor_rpm_port=xluuv_state.sen_rpm_port,
                sensor_rpm_stbd=xluuv_state.sen_rpm_stbd,
                sensor_rudder_angle=xluuv_state.sen_rudder_angle,
                sensor_sog=xluuv_state.sen_sog,
                sensor_speed=xluuv_state.sen_speed,
                sensor_depth_under_keel=xluuv_state.sen_depth_under_keel,
                sensor_ship_depth=xluuv_state.sen_ship_depth,
                sensor_throttle_port=xluuv_state.sen_throttle_port,
                sensor_throttle_stbd=xluuv_state.sen_throttle_stbd,
                act_rudder_angle=xluuv_state.act_rudder_angle,
                act_engine_throttle_port=xluuv_state.act_engine_throttle_port,
                act_engine_throttle_stbd=xluuv_state.act_engine_throttle_stbd,
                act_thruster_throttle_bow=xluuv_state.act_thruster_throttle_bow,
                act_thruster_throttle_stern=xluuv_state.act_thruster_throttle_stern,
                act_ballast_tank_pump=xluuv_state.act_ballast_tank_pump,
                colreg_status=xluuv_state.colreg_status,
                colreg_mmsi=xluuv_state.colreg_mmsi,
            )

    def SendApCommand(self, request: ApCommand, context) -> Empty:
        logging.info("received ap command")
        # transparently forward request to XLUUV
        self._xluuv_client.send_ap_command(request)
        return Empty()

    def SendMissionCommand(self, request: MsCommand, context) -> Empty:
        logging.info("received mission command")
        self._xluuv_client.send_ms_command(request)
        return Empty()
