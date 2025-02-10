#!/usr/bin/env python3
import argparse
import json
import logging
import os
from typing import List, Optional

import google.protobuf.message
from google.protobuf.empty_pb2 import Empty

from xluuv_ccc.client.formatting import (
    fmt_ap_state,
    fmt_ballast_pump,
    fmt_coords,
    fmt_deg,
    fmt_distance,
    fmt_dp_name,
    fmt_dp_stats,
    fmt_lp_name,
    fmt_lp_stats,
    fmt_ms_name,
    fmt_ms_progress,
    fmt_ms_status,
    fmt_percentage,
    fmt_rot,
    fmt_route_name,
    fmt_route_progress,
    fmt_route_stats,
    fmt_rpm,
    fmt_speed,
    fmt_timestamp,
    fmt_wpt_dist,
)
from xluuv_ccc.messages_pb2 import (
    AC_DIVE_START,
    AC_DIVE_STOP,
    AC_EMERGENCY_STOP,
    AC_LOITER_START,
    AC_LOITER_STOP,
    AC_ROUTE_RESUME,
    AC_ROUTE_START,
    AC_ROUTE_STOP,
    AC_ROUTE_SUSPEND,
    CR_CROSSING,
    CR_HEAD_TO_HEAD,
    CR_OVERTAKEN,
    CR_OVERTAKING,
    JE_NO_ERROR,
    MC_RESUME,
    MC_SKIP_STEP,
    MC_START,
    MC_STOP,
    MC_SUSPEND,
    MZE_NO_ERROR,
    RXE_NO_ERROR,
    AggregatedXluuvStatus,
    ApCommand,
    ApMissionJson,
    ApMissionZip,
    ApRouteXML,
    ColregType,
    DiveProcedureId,
    DiveProcedureJson,
    JsonError,
    JsonErrorMessage,
    LoiterPositionId,
    LoiterPositionJson,
    MissionZipError,
    MissionZipErrorMessage,
    MsCommand,
    RouteId,
    RouteXMLError,
    RouteXMLErrorMessage,
)
from xluuv_ccc.utils import setup_logging

from .client import RPC, CccClient, Request

os.environ["KIVY_NO_ARGS"] = "1"

from kivy.app import App  # noqa
from kivy.clock import mainthread  # noqa
from kivy.core.window import Window  # noqa
from kivy.logger import Logger  # noqa
from kivy.properties import ObjectProperty  # noqa
from kivy.uix.button import Button  # noqa
from kivy.uix.floatlayout import FloatLayout  # noqa
from kivy.uix.popup import Popup  # noqa
from kivy.uix.tabbedpanel import TabbedPanel  # noqa

CMD_MATCH = {
    "ms_skip": {
        "rpc": RPC.SEND_MISSION_COMMAND,
        "msg": MsCommand,
        "command": MC_SKIP_STEP,
    },
    "ms_start": {
        "rpc": RPC.SEND_MISSION_COMMAND,
        "msg": MsCommand,
        "command": MC_START,
    },
    "ms_stop": {
        "rpc": RPC.SEND_MISSION_COMMAND,
        "msg": MsCommand,
        "command": MC_STOP,
    },
    "ms_resume": {
        "rpc": RPC.SEND_MISSION_COMMAND,
        "msg": MsCommand,
        "command": MC_RESUME,
    },
    "ms_suspend": {
        "rpc": RPC.SEND_MISSION_COMMAND,
        "msg": MsCommand,
        "command": MC_SUSPEND,
    },
    "route_start": {
        "rpc": RPC.SEND_AP_COMMAND,
        "msg": ApCommand,
        "command": AC_ROUTE_START,
    },
    "route_stop": {
        "rpc": RPC.SEND_AP_COMMAND,
        "msg": ApCommand,
        "command": AC_ROUTE_STOP,
    },
    "route_suspend": {
        "rpc": RPC.SEND_AP_COMMAND,
        "msg": ApCommand,
        "command": AC_ROUTE_SUSPEND,
    },
    "route_resume": {
        "rpc": RPC.SEND_AP_COMMAND,
        "msg": ApCommand,
        "command": AC_ROUTE_RESUME,
    },
    "lp_start": {
        "rpc": RPC.SEND_AP_COMMAND,
        "msg": ApCommand,
        "command": AC_LOITER_START,
    },
    "lp_stop": {
        "rpc": RPC.SEND_AP_COMMAND,
        "msg": ApCommand,
        "command": AC_LOITER_STOP,
    },
    "dp_start": {
        "rpc": RPC.SEND_AP_COMMAND,
        "msg": ApCommand,
        "command": AC_DIVE_START,
    },
    "dp_stop": {
        "rpc": RPC.SEND_AP_COMMAND,
        "msg": ApCommand,
        "command": AC_DIVE_STOP,
    },
    "emergency_stop": {
        "rpc": RPC.SEND_AP_COMMAND,
        "msg": ApCommand,
        "command": AC_EMERGENCY_STOP,
    },
}


class CccTab(TabbedPanel):
    st_ping_label = ObjectProperty(None)
    st_mission_name = ObjectProperty(None)
    st_mission_stats = ObjectProperty(None)
    st_ms_status = ObjectProperty(None)
    st_ap_status = ObjectProperty(None)
    st_route_name = ObjectProperty(None)
    st_route_stats = ObjectProperty(None)
    st_route_progress = ObjectProperty(None)
    st_wpt_dist = ObjectProperty(None)
    st_lp_name = ObjectProperty(None)
    st_lp_stats = ObjectProperty(None)
    st_dp_name = ObjectProperty(None)
    st_dp_stats = ObjectProperty(None)
    st_gnss_ap = ObjectProperty(None)
    st_gnss_1 = ObjectProperty(None)
    st_gnss_2 = ObjectProperty(None)
    st_gnss_3 = ObjectProperty(None)
    st_cog = ObjectProperty(None)
    st_heading = ObjectProperty(None)
    st_sen_rudder = ObjectProperty(None)
    st_rot = ObjectProperty(None)
    st_sog = ObjectProperty(None)
    st_speed = ObjectProperty(None)
    st_ship_depth = ObjectProperty(None)
    st_depth_under_keel = ObjectProperty(None)
    st_rpm_port = ObjectProperty(None)
    st_rpm_stbd = ObjectProperty(None)
    st_act_rudder = ObjectProperty(None)
    st_act_ballast_pump = ObjectProperty(None)
    st_act_engine_throttle_port = ObjectProperty(None)
    st_act_engine_throttle_stbd = ObjectProperty(None)
    st_thruster_throttle_bow = ObjectProperty(None)
    st_thruster_throttle_stern = ObjectProperty(None)
    alert_container = ObjectProperty(None)
    st_rov_video = ObjectProperty(None)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        Window.size = (420, 1100)

        self.st_rov_video.source = (
            f"{os.path.dirname(os.path.abspath(__file__))}/video-camera.png"
        )


class LoadRouteDialog(FloatLayout):
    load = ObjectProperty(None)
    cancel = ObjectProperty(None)


class LoadMissionZipDialog(FloatLayout):
    load = ObjectProperty(None)
    cancel = ObjectProperty(None)


class LoadMissionJsonDialog(FloatLayout):
    load = ObjectProperty(None)
    cancel = ObjectProperty(None)


class LoadLpDialog(FloatLayout):
    load = ObjectProperty(None)
    cancel = ObjectProperty(None)


class LoadDpDialog(FloatLayout):
    load = ObjectProperty(None)
    cancel = ObjectProperty(None)


class MessageEditor(FloatLayout):
    msg_type: str
    textinput = ObjectProperty(None)


class CccGuiApp(App):
    root: CccTab

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.last_route_path = os.getcwd()
        self.last_mission_zip_path = os.getcwd()
        self.last_mission_json_path = os.getcwd()
        self.last_lp_path = os.getcwd()
        self.last_dp_path = os.getcwd()

        self.route_template = {"id": 1, "xml": ""}
        self.mission_template = {
            "mission": {
                "id": 1,
                "name": "Example Mission",
                "items": [
                    {"until_completion": True, "activate_route": 1},
                    {"until_completion": True, "activate_loiterpos": 1},
                    {"until_timeout": 120, "set_ap_cmd": "AC_ROUTE_START"},
                    {"until_completion": True, "set_ap_cmd": "AC_ROUTE_SUSPEND"},
                    {"until_timeout": 300, "set_ap_cmd": "AC_LOITER_START"},
                    {"until_completion": True, "set_ap_cmd": "AC_LOITER_STOP"},
                    {"until_completion": True, "set_ap_cmd": "AC_ROUTE_RESUME"},
                ],
            }
        }
        self.loiter_pos_template = {
            "id": 1,
            "name": "Example Loiter Pos.",
            "lat": 1.3012,
            "lon": "001° 18.0720' W",
            "bearing": 42.305,
        }
        self.dive_proc_template = {"id": 1, "name": "Example Dive Proc.", "depth": 10.0}

        self.route_str = json.dumps(self.route_template, indent=2)
        self.lp_str = json.dumps(self.loiter_pos_template, indent=2)
        self.dp_str = json.dumps(self.dive_proc_template, indent=2)
        self.mission_json_str = json.dumps(self.mission_template, indent=2)
        self.mission_zip_obj: Optional[bytes] = None

        self.ccc_client: Optional[CccClient] = None

        self.colreg_alert_mmsi: Optional[int] = None
        self.colreg_alert_type: Optional[ColregType] = None

    def build(self):
        return CccTab()

    def show_load(self, load_what: str):
        match load_what:
            case "missionjson":
                content = LoadMissionJsonDialog(
                    load=self.load, cancel=self.dismiss_popup
                )
                self._popup = Popup(
                    title="Load Mission JSON", content=content, size_hint=(0.9, 0.9)
                )
                self._popup.open()
            case "missionzip":
                content = LoadMissionZipDialog(
                    load=self.load, cancel=self.dismiss_popup
                )
                self._popup = Popup(
                    title="Load Mission ZIP", content=content, size_hint=(0.9, 0.9)
                )
                self._popup.open()
            case "route":
                content = LoadRouteDialog(load=self.load, cancel=self.dismiss_popup)
                self._popup = Popup(
                    title="Load Route XML", content=content, size_hint=(0.9, 0.9)
                )
                self._popup.open()
            case "lp":
                content = LoadLpDialog(load=self.load, cancel=self.dismiss_popup)
                self._popup = Popup(
                    title="Load Loiter Position JSON",
                    content=content,
                    size_hint=(0.9, 0.9),
                )
                self._popup.open()
            case "dp":
                content = LoadDpDialog(load=self.load, cancel=self.dismiss_popup)
                self._popup = Popup(
                    title="Load Dive Procedure JSON",
                    content=content,
                    size_hint=(0.9, 0.9),
                )
                self._popup.open()

    def show_editor(self, msg_type: str):
        content = MessageEditor()
        content.msg_type = msg_type
        match msg_type:
            case "route":
                content.textinput.text = self.route_str
            case "lp":
                content.textinput.text = self.lp_str
            case "dp":
                content.textinput.text = self.dp_str
            case "missionjson":
                content.textinput.text = self.mission_json_str
        self._popup = Popup(title="Edit Message", content=content, size_hint=(0.9, 0.9))
        self._popup.open()

    def dismiss_popup(self):
        self._popup.dismiss()

    def cancel(self):
        self.dismiss_popup()

    def editor_save(self, text: str, msg_type: str):
        match msg_type:
            case "route":
                self.route_str = text
            case "lp":
                self.lp_str = text
            case "dp":
                self.dp_str = text
            case "missionjson":
                self.mission_json_str = text
        self.dismiss_popup()

    def send_command(self, command: str):
        if self.ccc_client is None:
            return

        if command not in CMD_MATCH.keys():
            logging.error(f"invalid mission or autopilot command '{command}'")
            return

        req = CMD_MATCH[command]
        self.ccc_client.enqueue(
            Request(rpc=req["rpc"], message=req["msg"](command=req["command"]))
        )

    def rov_deploy_retrieve(self, command: str):
        logging.info(f"ROV: {command}")
        if command == "deploy":
            self.root.st_rov_video.source = "rtp://127.0.0.1:5010"
            self.root.st_rov_video.play = True
        else:
            self.root.st_rov_video.play = False
            self.root.st_rov_video.source = (
                f"{os.path.dirname(os.path.abspath(__file__))}/video-camera.png"
            )

    def rov_video_ended(self):
        logging.info("ROV: video ended")
        self.root.st_rov_video.play = False
        self.root.st_rov_video.source = "rtp://127.0.0.1:5010"
        self.root.st_rov_video.play = True

    def activate_procedure(self, proc_id_str: str, msg_type: str):
        if self.ccc_client is None:
            return
        try:
            proc_id = int(proc_id_str)
        except ValueError:
            logging.warning(
                f"invalid {msg_type} id '{proc_id_str}', must be an integer"
            )
            return

        match msg_type:
            case "route":
                self.ccc_client.enqueue(
                    Request(rpc=RPC.ACTIVATE_ROUTE, message=RouteId(id=proc_id))
                )
            case "lp":
                self.ccc_client.enqueue(
                    Request(rpc=RPC.ACTIVATE_LP, message=LoiterPositionId(id=proc_id))
                )
            case "dp":
                self.ccc_client.enqueue(
                    Request(rpc=RPC.ACTIVATE_DP, message=DiveProcedureId(id=proc_id))
                )

    def send_procedure(self, msg_type: str):
        if self.ccc_client is None:
            return
        match msg_type:
            case "route":
                try:
                    r_json = json.loads(self.route_str)
                    route = ApRouteXML(id=r_json["id"], route_xml=r_json["xml"])
                    self.ccc_client.enqueue(
                        Request(rpc=RPC.SEND_ROUTE_XML, message=route)
                    )
                except json.JSONDecodeError:
                    logging.warning(
                        f"Procedure sender: invalid JSON '{self.route_str}'"
                    )
                except KeyError:
                    logging.warning("Procedure sender: route JSON missing key(s)")
            case "missionzip":
                mission = ApMissionZip(mission_zip=self.mission_zip_obj)
                self.ccc_client.enqueue(
                    Request(rpc=RPC.SEND_MISSION_ZIP, message=mission)
                )
            case "missionjson":
                mission = ApMissionJson(mission_json=self.mission_json_str)
                self.ccc_client.enqueue(
                    Request(rpc=RPC.SEND_MISSION_JSON, message=mission)
                )
            case "lp":
                lp = LoiterPositionJson(lp_json=self.lp_str)
                self.ccc_client.enqueue(Request(rpc=RPC.SEND_LP_JSON, message=lp))
            case "dp":
                dp = DiveProcedureJson(dp_json=self.dp_str)
                self.ccc_client.enqueue(Request(rpc=RPC.SEND_DP_JSON, message=dp))

    def load(self, path: str, filename: List[str], what: str):
        self.dismiss_popup()
        if what == "mission_zip":
            with open(filename[0], "rb") as ifile:
                self.last_mission_zip_path = path
                self.mission_zip_obj = ifile.read()
            return

        with open(filename[0], "rt") as ifile:
            match what:
                case "route":
                    self.last_route_path = path
                    self.route_str = json.dumps(
                        {"id": 1, "xml": ifile.read()}, indent=4
                    )
                case "lp":
                    self.last_lp_path = path
                    self.lp_str = ifile.read()
                case "dp":
                    self.last_lp_path = path
                    self.dp_str = ifile.read()
                case "mission_json":
                    self.last_mission_json_path = path
                    self.mission_json_str = ifile.read()

    def show_alert_popup(self, text: str):
        self.root.alert_container.text = text
        self.root.alert_container.visible = True

    def dismiss_alert_popup(self):
        self.root.alert_container.visible = False

    def handle_colreg(self, status: ColregType, mmsi: int):
        action = ""
        if status == CR_CROSSING:
            action = "crossing path of"
        elif status == CR_OVERTAKING:
            action = "overtaking"
        elif status == CR_OVERTAKEN:
            action = "overtaken by"
        elif status == CR_HEAD_TO_HEAD:
            # this actually isn't always the case, see AP execute_colreg()
            action = "head to head with"

        if len(action) == 0:
            self.dismiss_alert_popup()
            self.colreg_alert_mmsi = None
        elif self.colreg_alert_mmsi != mmsi or self.colreg_alert_type != status:
            # only raise alert if it hasn't already been raised before
            self.colreg_alert_mmsi = mmsi
            self.colreg_alert_type = status
            self.show_alert_popup(f"COLREG: {action} MMSI {mmsi}")

    @mainthread
    def render(self, rpc_response: google.protobuf.message.Message) -> None:
        match rpc_response:
            case Empty():
                # we don't need to update the GUI
                logging.info("RPC: Empty Response")
            case RouteXMLErrorMessage():
                if rpc_response.error != RXE_NO_ERROR:
                    logging.warning(
                        f"RPC: sending route failed: {RouteXMLError.Name(rpc_response.error)}"
                    )

            case JsonErrorMessage():
                if rpc_response.error != JE_NO_ERROR:
                    logging.warning(
                        f"RPC: operation failed: {JsonError.Name(rpc_response.error)}"
                    )

            case MissionZipErrorMessage():
                if rpc_response.error != MZE_NO_ERROR:
                    logging.warning(
                        f"RPC: invalid mission zip format: {MissionZipError.Name(rpc_response.error)}"
                    )

            case AggregatedXluuvStatus():
                # update last ping field
                self.root.st_ping_label.text = fmt_timestamp(rpc_response.last_ping)

                # AP reports
                self.root.st_ms_status.text = fmt_ms_status(rpc_response.ms_status)
                self.root.st_mission_name.text = fmt_ms_name(rpc_response.ms_name)
                self.root.st_mission_stats.text = fmt_ms_progress(
                    rpc_response.ms_progress, rpc_response.ms_len
                )
                self.root.st_ap_status.text = fmt_ap_state(rpc_response.ap_state)
                self.root.st_route_name.text = fmt_route_name(
                    rpc_response.ap_route_name
                )
                self.root.st_route_stats.text = fmt_route_stats(rpc_response.tgt_speed)
                self.root.st_route_progress.text = fmt_route_progress(
                    rpc_response.ap_route_progress, rpc_response.ap_route_len
                )
                self.root.st_wpt_dist.text = fmt_wpt_dist(
                    rpc_response.ccc_estimated_wpt_dist
                )
                self.root.st_lp_name.text = fmt_lp_name(rpc_response.lp_name)
                self.root.st_lp_stats.text = fmt_lp_stats(rpc_response.lp_dist)
                self.root.st_dp_name.text = fmt_dp_name(rpc_response.dp_name)
                self.root.st_dp_stats.text = fmt_dp_stats(
                    rpc_response.tgt_depth, rpc_response.ccc_estimated_dp_dist
                )

                self.root.st_gnss_ap.text = fmt_coords(rpc_response.gnss_ap)

                # sensors
                self.root.st_gnss_1.text = fmt_coords(rpc_response.sensor_gnss_1)
                self.root.st_gnss_2.text = fmt_coords(rpc_response.sensor_gnss_2)
                self.root.st_gnss_3.text = fmt_coords(rpc_response.sensor_gnss_3)
                self.root.st_cog.text = fmt_deg(rpc_response.sensor_cog)
                self.root.st_heading.text = fmt_deg(rpc_response.sensor_heading)
                self.root.st_sen_rudder.text = fmt_deg(rpc_response.sensor_rudder_angle)
                self.root.st_rot.text = fmt_rot(rpc_response.sensor_rot)
                self.root.st_sog.text = fmt_speed(rpc_response.sensor_sog)
                self.root.st_speed.text = fmt_speed(rpc_response.sensor_speed)
                self.root.st_ship_depth.text = fmt_distance(
                    rpc_response.sensor_ship_depth
                )
                self.root.st_depth_under_keel.text = fmt_distance(
                    rpc_response.sensor_depth_under_keel
                )
                self.root.st_rpm_port.text = fmt_rpm(rpc_response.sensor_rpm_port)
                self.root.st_rpm_stbd.text = fmt_rpm(rpc_response.sensor_rpm_stbd)

                # actuators
                self.root.st_act_rudder.text = fmt_deg(rpc_response.act_rudder_angle)
                self.root.st_act_ballast_pump.text = fmt_ballast_pump(
                    rpc_response.act_ballast_tank_pump
                )
                self.root.st_act_engine_throttle_port.text = fmt_percentage(
                    rpc_response.act_engine_throttle_port
                )
                self.root.st_act_engine_throttle_stbd.text = fmt_percentage(
                    rpc_response.act_engine_throttle_stbd
                )
                self.root.st_thruster_throttle_bow.text = fmt_percentage(
                    rpc_response.act_thruster_throttle_bow
                )
                self.root.st_thruster_throttle_stern.text = fmt_percentage(
                    rpc_response.act_thruster_throttle_stern
                )

                # COLREG
                self.handle_colreg(rpc_response.colreg_status, rpc_response.colreg_mmsi)


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
    Logger.setLevel(args.log_level)
    app = CccGuiApp()
    app.ccc_client = CccClient(
        app.render, args.ccc_grpc_host, args.ccc_grpc_port, poll=True
    )
    try:
        app.ccc_client.start()
        app.run()

    except KeyboardInterrupt:
        app.ccc_client.terminate()
        app.stop()


if __name__ == "__main__":
    main()
