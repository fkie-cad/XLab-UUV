import logging
from typing import Optional

from google.protobuf.timestamp_pb2 import Timestamp

from xluuv_ccc.marmaths import dec_to_opencpn_string
from xluuv_ccc.marmaths.constants import MS_TO_KNT, RAD_TO_DEG
from xluuv_ccc.messages_pb2 import (
    AS_UNKNOWN,
    MS_UNKNOWN,
    AutopilotState,
    Coordinates,
    MissionStatus,
)

_unavailable = "N/A"


def _fmt_float(v: float, pres: float = 2) -> str:
    if not isinstance(v, float):
        raise TypeError(f"'{v}' passed to _fm_float is not a float")
    return f"{v:.{pres}f}"


def fmt_timestamp(timestamp: Timestamp) -> str:
    if timestamp.ToSeconds() == 0:
        return _unavailable
    return timestamp.ToDatetime().astimezone().strftime("%H:%M:%S.%f")[:-4]


def fmt_ms_status(status: MissionStatus) -> str:
    if status == MS_UNKNOWN:
        return _unavailable
    return MissionStatus.Name(status)


def fmt_ms_name(name: Optional[str]) -> str:
    if name is None or name == "":
        name = _unavailable
    return f"Mission '{name}'"


def fmt_ms_progress(prog: Optional[int], length: Optional[int]) -> str:
    if length is None or length == 0:
        return f"Progress {_unavailable}"
    if prog is None:
        prog = 0
    return f"Step {prog}/{length}"


def fmt_ap_state(state: AutopilotState) -> str:
    if state == AS_UNKNOWN:
        return _unavailable
    return AutopilotState.Name(state)


def fmt_route_name(name: Optional[str]) -> str:
    if name is None or name == "":
        name = _unavailable
    return f"Route '{name}'"


def fmt_route_progress(prog: Optional[int], length: Optional[int]) -> str:
    if length is None or length == 0:
        return f"Progress {_unavailable}"
    if prog is None:
        prog = 0
    return f"Waypoint {prog}/{length}"


def fmt_route_stats(tgt_speed: Optional[float]) -> str:
    if tgt_speed is None:
        return f"Tgt. Speed {_unavailable}"
    return f"Tgt. Speed {fmt_speed(tgt_speed)}"


def fmt_lp_name(name: Optional[str]) -> str:
    if name is None or name == "":
        name = _unavailable
    return f"Loiter Pos. '{name}'"


def fmt_dp_name(name: Optional[str]) -> str:
    if name is None or name == "":
        name = _unavailable
    return f"Dive Proc. '{name}'"


def fmt_wpt_dist(dist: Optional[float]) -> str:
    return fmt_lp_stats(dist)


def fmt_distance(dist: Optional[float]) -> str:
    if dist is None:
        return _unavailable
    return f"{_fmt_float(dist, pres=1)}m"


def fmt_dp_stats(target: Optional[float], dist: Optional[float]) -> str:
    return f"Tgt: {fmt_distance(target)}, Delta: {fmt_distance(dist)}"


def fmt_lp_stats(dist: Optional[float]) -> str:
    return f"Distance {fmt_distance(dist)}"


def fmt_coords(pos: Optional[Coordinates]) -> str:
    if pos is None or (pos.latitude == 0.0 and pos.longitude == 0.0):
        return _unavailable
    try:
        return f"{dec_to_opencpn_string(pos.latitude, True)}\n{dec_to_opencpn_string(pos.longitude, False)}"
    except ValueError:
        logging.warning(
            f"tried to format invalid coordinates {pos.latitude}/{pos.longitude}"
        )
        return "ERROR"


def fmt_deg(deg: Optional[float]) -> str:
    if deg is None:
        return _unavailable
    return f"{_fmt_float(deg)}°"


def fmt_rot(rot: Optional[float]) -> str:
    if rot is None:
        return _unavailable
    return f"{fmt_deg(rot * RAD_TO_DEG)}/s"


def fmt_speed(speed: Optional[float]) -> str:
    if speed is None:
        return _unavailable
    # return f"{_fm_float(speed)}m/s"
    return f"{_fmt_float(speed * MS_TO_KNT)}kn"


def fmt_rpm(rpm: Optional[float]) -> str:
    if rpm is None:
        return _unavailable
    return f"{_fmt_float(rpm)}"


def fmt_ballast_pump(pump: Optional[float]) -> str:
    if pump is None:
        return _unavailable
    dir = "IN"
    if pump < 0.0:
        dir = "OUT"

    return f"{dir} {fmt_percentage(abs(pump))}"


def fmt_percentage(v: Optional[float]) -> str:
    if v is None:
        return _unavailable
    return f"{_fmt_float(v * 100.0)}%"
