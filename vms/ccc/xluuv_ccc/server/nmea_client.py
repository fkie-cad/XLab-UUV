#!/usr/bin/env python3
import logging
import socket
from datetime import datetime, timezone
from typing import List

from xluuv_ccc.marmaths.constants import MS_TO_KNT
from xluuv_ccc.messages_pb2 import (
    APReport,
    ColregStatus,
    ColregType,
    Coordinates,
    SensorReport,
    WrappedNmea,
)
from xluuv_ccc.nmea import (
    DptSentence,
    GllSentence,
    HdtSentence,
    NmeaSentence,
    RMCSentence,
    RpmSentence,
    RsaSentence,
    TllSentence,
)


class NmeaClient:
    def __init__(self, host: str, port: int) -> None:
        self._dst = (host, port)

        self._socket = socket.socket(
            socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP
        )

        self._gll = GllSentence()
        self._hdt = HdtSentence()
        self._rpm_port = RpmSentence()
        self._rpm_stbd = RpmSentence()
        self._rsa = RsaSentence()
        self._rmc = RMCSentence()
        self._dpt = DptSentence()
        self._gnss_ap = Coordinates()
        self._gnss_ap.latitude = 0.0
        self._gnss_ap.longitude = 0.0

        self._colreg_tgt_id = 0
        self._colreg_tgt_active = False
        self._colreg_tll = TllSentence(tgt_name="COLREG TGT", reference="")

    def handle_ap_report(self, ap_report: APReport) -> None:
        # buffer AP gnss, which we send over alongside the
        # more frequent sensor updates
        self._gnss_ap = ap_report.gnss_ap

    def handle_sensors(self, sensor_report: SensorReport) -> None:
        # Note: we use the gnss position computed by the autopilot
        #       which might differ from actual sensor values
        self._gll.latitude = self._gnss_ap.latitude
        self._gll.longitude = self._gnss_ap.longitude
        self._gll.timestamp = datetime.now(timezone.utc)

        self._hdt.heading = sensor_report.heading

        self._rpm_port.rpm = sensor_report.rpm_port
        self._rpm_port.engine_or_shaft_nr = 1
        self._rpm_stbd.rpm = sensor_report.rpm_stbd
        self._rpm_stbd.engine_or_shaft_nr = 1

        self._rmc.latitude = self._gnss_ap.latitude
        self._rmc.longitude = self._gnss_ap.longitude
        self._rmc.timestamp = datetime.now(timezone.utc)
        self._rmc.track_made_good = sensor_report.cog
        self._rmc.sog = sensor_report.sog * MS_TO_KNT

        self._rsa.stbd_angle = sensor_report.rudder_angle

        self._dpt.depth = sensor_report.depth_under_keel
        self._dpt.offset = 0.0

        sentences: List[NmeaSentence] = [
            self._rmc,
            self._gll,
            self._hdt,
            self._rpm_port,
            self._rpm_stbd,
            self._rsa,
            self._dpt,
        ]
        for sentence in sentences:
            try:
                self._socket.sendto(sentence.serialize_to_str().encode(), self._dst)
            except ValueError as e:
                logging.info(f"Could not serialize NMEA sentence: {e}")

    # nmea messages forwarded by XLUUV, e.g. AIS
    def handle_wrapped_nmea(self, nmea_report: WrappedNmea):
        self._socket.sendto(nmea_report.msg.encode(), self._dst)

    # colreg information, which we forward as fake ARPA targets
    def handle_colreg_report(self, colreg_report: ColregStatus):
        serialized_sentences: List[str] = []

        if self._colreg_tgt_active:
            # clear previous target by indicating it has been lost
            self._colreg_tll.tgt_number = 90 + self._colreg_tgt_id
            self._colreg_tll.status = "L"  # lost target
            self._colreg_tll.timestamp = datetime.now(timezone.utc)

            try:
                serialized_sentences.append(self._colreg_tll.serialize_to_str())
            except ValueError as e:
                logging.info(f"Could not serialize NMEA sentence: {e}")

        # switch target number
        self._colreg_tgt_id = 1 - self._colreg_tgt_id

        status = colreg_report.type
        if status != ColregType.CR_INACTIVE:
            self._colreg_tgt_active = True
            # update position by painting a new target
            self._colreg_tll.tgt_number = 90 + self._colreg_tgt_id
            self._colreg_tll.status = "Q"  # acquiring target, marked orange in ocpn
            self._colreg_tll.latitude = colreg_report.tgt_pos.latitude
            self._colreg_tll.longitude = colreg_report.tgt_pos.longitude
            self._colreg_tll.timestamp = datetime.now(timezone.utc)

            try:
                serialized_sentences.append(self._colreg_tll.serialize_to_str())
            except ValueError as e:
                logging.info(f"Could not serialize NMEA sentence: {e}")
        else:
            self._colreg_tgt_active = False

        for sentence in serialized_sentences:
            self._socket.sendto(sentence.encode(), self._dst)
