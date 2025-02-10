from datetime import datetime, timezone
from functools import reduce
from typing import List

from .marmaths import dd_to_ddm


class NmeaSentence:
    def __init__(
        self,
        prefix: str,
        talker: str,
        type_code: str,
        fields: List[str],
    ) -> None:
        self.prefix = prefix
        self.talker = talker
        self.type_code = type_code
        self.valid = False
        self.valid_checksum = False
        self.fields = fields
        self.update_fields()
        self.update_checksum()

    def _compute_checksum(self, data: List[int]) -> int:
        return reduce(lambda a, b: a ^ b, data)

    def update_checksum(self) -> None:
        self.checksum = self._compute_checksum([ord(char) for char in self.raw][1:])

    def update_fields(self) -> None:
        self.raw = f"{self.prefix}{self.talker}{self.type_code},{','.join([str(f) for f in self.fields])}"

    def serialize_to_str(self) -> str:
        self.update_fields()
        self.update_checksum()
        return f"{self.raw}*{self.checksum:02x}\r\n"

    def __str__(self) -> str:
        return self.serialize_to_str().rstrip()


class RpmSentence(NmeaSentence):
    source: str
    engine_or_shaft_nr: int
    rpm: float
    pitch: float
    status: str

    def __init__(
        self,
        prefix: str = "$",
        talker: str = "II",
        type_code: str = "RPM",
        source: str = "S",
        engine_or_shaft_nr: int = 1,
        rpm: float = 0.0,
        pitch: float = 100.0,
        status: str = "A",
    ) -> None:
        self.source = source
        self.engine_or_shaft_nr = engine_or_shaft_nr
        self.rpm = rpm
        self.pitch = pitch
        self.status = status

        super().__init__(
            prefix,
            talker,
            type_code,
            [source, str(engine_or_shaft_nr), f"{rpm:.2f}", f"{pitch:.2f}", status],
        )

    def update_fields(self) -> None:
        self.fields = [
            self.source,
            str(self.engine_or_shaft_nr),
            f"{self.rpm:.2f}",
            f"{self.pitch:.2f}",
            self.status,
        ]
        return super().update_fields()


class RsaSentence(NmeaSentence):
    stbd_angle: float
    stbd_status: str
    port_angle: float
    port_status: str

    def __init__(
        self,
        prefix: str = "$",
        talker: str = "II",
        type_code: str = "RSA",
        stbd_angle: float = 0.0,
        stbd_status: str = "A",
        port_angle: float = 0.0,
        port_status: str = "V",
    ):
        self.stbd_angle = stbd_angle
        self.stbd_status = stbd_status
        self.port_angle = port_angle
        self.port_status = port_status
        super().__init__(
            prefix,
            talker,
            type_code,
            [
                f"{self.stbd_angle:.2f}",
                self.stbd_status,
                f"{self.port_angle:.2f}",
                self.port_status,
            ],
        )

    def update_fields(self) -> None:
        self.fields = [
            f"{self.stbd_angle:.2f}",
            self.stbd_status,
            f"{self.port_angle:.2f}",
            self.port_status,
        ]
        return super().update_fields()


class GllSentence(NmeaSentence):
    latitude: float
    longitude: float
    timestamp: datetime
    status: str
    faa_mode: str

    def __init__(
        self,
        prefix: str = "$",
        talker: str = "GP",
        type_code: str = "GLL",
        latitude: float = 0.0,
        longitude: float = 0.0,
        timestamp: datetime = datetime.now(timezone.utc),
        status: str = "A",
        faa_mode: str = "A",
    ):
        self.latitude = latitude
        self.longitude = longitude
        self.timestamp = timestamp
        self.status = status
        self.faa_mode = faa_mode

        lat_h, lat_m, lat_d = dd_to_ddm(self.latitude)
        lon_h, lon_m, lon_d = dd_to_ddm(self.longitude, latitude=False)
        super().__init__(
            prefix,
            talker,
            type_code,
            [
                f"{lat_h:02d}{lat_m:07.4f}",
                lat_d,
                f"{lon_h:03d}{lon_m:07.4f}",
                lon_d,
                f"{self.timestamp.strftime('%H%M%S%f')[:8]}",
                self.status,
                self.faa_mode,
            ],
        )

    def update_fields(self) -> None:
        lat_h, lat_m, lat_d = dd_to_ddm(self.latitude)
        lon_h, lon_m, lon_d = dd_to_ddm(self.longitude, latitude=False)
        self.fields = [
            f"{lat_h:02d}{lat_m:07.4f}",
            lat_d,
            f"{lon_h:03d}{lon_m:07.4f}",
            lon_d,
            f"{self.timestamp.strftime('%H%M%S%f')[:8]}",
            self.status,
            self.faa_mode,
        ]
        return super().update_fields()


class RMCSentence(NmeaSentence):
    timestamp: datetime
    status: str
    latitude: float
    longitude: float
    sog: float
    track_made_good: float
    magnetic_var: float
    faa_mode: str
    nav_status: str

    def __init__(
        self,
        prefix: str = "$",
        talker: str = "GP",
        type_code: str = "RMC",
        timestamp=datetime.now(timezone.utc),
        status: str = "A",
        latitude: float = 0.0,
        longitude: float = 0.0,
        sog: float = 0.0,  # knots
        cog: float = 0.0,  # degrees
        magnetic_var: float = 0.0,
        faa_mode: str = "A",
        nav_status: str = "A",
    ):
        self.timestamp = timestamp
        self.status = status
        self.latitude = latitude
        self.longitude = longitude
        self.sog = sog
        self.track_made_good = cog
        self.magnetic_var = magnetic_var
        self.faa_mode = faa_mode
        self.nav_status = nav_status

        lat_h, lat_m, lat_d = dd_to_ddm(self.latitude)
        lon_h, lon_m, lon_d = dd_to_ddm(self.longitude, latitude=False)

        super().__init__(
            prefix,
            talker,
            type_code,
            [
                f"{self.timestamp.strftime('%H%M%S%f')[:8]}",
                self.status,
                f"{lat_h:02d}{lat_m:07.4f}",
                lat_d,
                f"{lon_h:03d}{lon_m:07.4f}",
                lon_d,
                f"{self.sog:.3f}",
                f"{self.track_made_good:.3f}",
                self.timestamp.strftime("%d%m%y"),
                "",  # ignoring magnetic variation
                "",
                self.faa_mode,
                self.nav_status,
            ],
        )

    def update_fields(self) -> None:
        lat_h, lat_m, lat_d = dd_to_ddm(self.latitude)
        lon_h, lon_m, lon_d = dd_to_ddm(self.longitude, latitude=False)
        self.fields = [
            f"{self.timestamp.strftime('%H%M%S%f')[:8]}",
            self.status,
            f"{lat_h:02d}{lat_m:07.4f}",
            lat_d,
            f"{lon_h:03d}{lon_m:07.4f}",
            lon_d,
            f"{self.sog:.3f}",
            f"{self.track_made_good:.3f}",
            self.timestamp.strftime("%d%m%y"),
            "",
            "",
            self.faa_mode,
            self.nav_status,
        ]
        return super().update_fields()


class HdtSentence(NmeaSentence):
    heading: float
    heading_type: str

    def __init__(
        self,
        prefix: str = "$",
        talker: str = "GP",
        type_code: str = "HDT",
        heading: float = 0.0,  # degrees
        heading_type: str = "T",
    ):
        self.heading = heading
        self.heading_type = heading_type
        super().__init__(
            prefix, talker, type_code, [f"{self.heading:.2f}", self.heading_type]
        )

    def update_fields(self) -> None:
        self.fields = [f"{self.heading:.2f}", self.heading_type]
        return super().update_fields()


class TllSentence(NmeaSentence):
    tgt_number: int
    latitude: float
    longitude: float
    tgt_name: str
    timestamp: datetime
    status: str
    reference: str

    def __init__(
        self,
        prefix: str = "$",
        talker: str = "RA",
        type_code: str = "TLL",
        tgt_number: int = 1,
        latitude: float = 0.0,
        longitude: float = 0.0,
        tgt_name: str = "TGT 01",
        timestamp: datetime = datetime.now(timezone.utc),
        status: str = "T",
        reference: str = "",
    ):
        self.tgt_number = tgt_number
        self.latitude = latitude
        self.longitude = longitude
        self.tgt_name = tgt_name
        self.timestamp = timestamp
        self.status = status
        self.reference = reference

        lat_h, lat_m, lat_d = dd_to_ddm(self.latitude)
        lon_h, lon_m, lon_d = dd_to_ddm(self.longitude, latitude=False)

        super().__init__(
            prefix,
            talker,
            type_code,
            [
                f"{self.tgt_number:02d}",
                f"{lat_h:02d}{lat_m:07.4f}",
                lat_d,
                f"{lon_h:03d}{lon_m:07.4f}",
                lon_d,
                self.tgt_name,
                f"{self.timestamp.strftime('%H%M%S%f')[:8]}",
                self.status,
                self.reference,
            ],
        )

    def update_fields(self) -> None:
        lat_h, lat_m, lat_d = dd_to_ddm(self.latitude)
        lon_h, lon_m, lon_d = dd_to_ddm(self.longitude, latitude=False)

        self.fields = [
            f"{self.tgt_number:02d}",
            f"{lat_h:02d}{lat_m:07.4f}",
            lat_d,
            f"{lon_h:03d}{lon_m:07.4f}",
            lon_d,
            self.tgt_name,
            f"{self.timestamp.strftime('%H%M%S%f')[:8]}",
            self.status,
            self.reference,
        ]

        super().update_fields()


class DptSentence(NmeaSentence):
    """
    Depth Sentance

        Parameters:
            depth (float): Depth in meters
            offset (float): Offset from transducer:
                Positive - distance from transducer to water line, or
                Negative - distance from transducer to keel
    """

    depth: float
    offset: float

    def __init__(
        self,
        prefix: str = "$",
        talker: str = "GP",
        type_code: str = "DPT",
        depth: float = 0.0,
        offset: float = 0.0,
    ):
        self.depth = depth
        self.offset = offset
        super().__init__(
            prefix, talker, type_code, [f"{self.depth:.1f}", f"{self.offset:.1f}"]
        )

    def update_fields(self) -> None:
        self.fields = [f"{self.depth:.1f}", f"{self.offset:.1f}"]
        return super().update_fields()
