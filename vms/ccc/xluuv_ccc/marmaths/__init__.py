import re
from math import asin, atan2, cos, degrees, pi, radians, sin, sqrt
from typing import Tuple

from .constants import *
from .constants import EARTH_RADIUS, KNT_TO_MS, M_TO_NM, MS_TO_KNT, NM_TO_M


def knt_to_ms(knots: float) -> float:
    """
    Converts knots to meters/second.

    Args:
        knots: a velocity in knots

    Returns:
        The velocity in meters per second.
    """
    return knots * KNT_TO_MS


def ms_to_knt(ms: float) -> float:
    """
    Converts meters/second to knots.

    Args:
        ms: a velocity in meters per second

    Returns:
        The velocity in knots.
    """
    return ms * MS_TO_KNT


def nm_to_m(nmiles: float) -> float:
    """
    Converts nautical miles to meters.

    Args:
        nmiles: a distance in nautical miles

    Returns:
        The distance in meters.
    """
    return nmiles * NM_TO_M


def m_to_nm(meters: float) -> float:
    """
    Converts meters to nautical miles.

    Args:
        meters: a distance in meters

    Returns:
        The distance in nautical miles.
    """
    return meters * M_TO_NM


def ddm_to_dd(hours: int, minutes: float, direction: str) -> Tuple[float, bool]:
    """
    Degrees Decimal Minute to Decimal Degrees:
    converts a latitude or longitude in hours, minutes, direction
    format to a single float value.

    Args:
        hours: hours of the latitude or longitude
        minutes: minutes of the latitude or longitude
        direction: the direction (N, S, W, or E)

    Returns:
        A tuple containing respectively the Decimal Degrees equivalent of
        the provided value and a boolean representing
        the direction (True if latitude, False if longitude)
    """
    mod = 1
    latitude = False
    if direction == "S":
        latitude = True
        mod = -1
    elif direction == "W":
        mod = -1
    elif direction == "N":
        latitude = True
    return (mod * (hours + (minutes / 60.0)), latitude)


def dd_to_ddm(position: float, latitude: bool = True) -> Tuple[int, float, str]:
    """
    Degrees Decimal Minute to Decimal Degrees:
    Decimal Degrees to Degrees Decimal Minute:
    converts a latitude or longitude specified as a single float
    to hours, minutes, direction values

    Raises a ``ValueError`` if the input value is not within
    the expected range.

    Args:
        position: a latitude or longitude decimal value, e.g. ``42.301``
        latitude: whether the passed position is a latitude value, if
            false it is instead interpreted as a longitude value

    Returns:
        The Degrees Decimal Minute equivalent of the provided value.
    """
    direction: str
    if latitude:
        if position < -90 or position > 90:
            raise ValueError(
                f"invalid latitude '{position}', must be between -90 and 90"
            )
        if position > 0:
            direction = "N"
        else:
            direction = "S"
    else:
        if position < -180 or position > 180:
            raise ValueError(
                f"invalid longitude '{position}', must be between -180 and 180"
            )
        if position > 0:
            direction = "E"
        else:
            direction = "W"
    hours = int(position)
    minutes = (position - hours) * 60

    return (abs(hours), abs(minutes), direction)


H_REGEXP = re.compile("^([0-9]+)°")
M_REGEXP = re.compile(r" ([0-9]+\.[0-9]+)'")
D_REGEXP = re.compile(" ([A-Z])$")


def opencpn_string_to_dec(position: str) -> Tuple[float, bool]:
    """
    Converts an OpenCPN-formatted latitude or longitude string
    to a decimal float value.

    Raises a ``ValueError`` if the string cannot be parsed.

    Args:
        position: a latitude or longitude string in OpenCPN format,
            e.g. ``"63° 18.3747' S"``

    Returns:
        A tuple containing respectively the Decimal Degrees equivalent of
        the provided string and a boolean representing
        the direction (True if latitude, False if longitude)
    """
    hours_match = H_REGEXP.search(position)
    minutes_match = M_REGEXP.search(position)
    direction_match = D_REGEXP.search(position)

    if hours_match and minutes_match and direction_match:
        return ddm_to_dd(
            int(hours_match.group(1)),
            float(minutes_match.group(1)),
            direction_match.group(1),
        )

    raise ValueError(f"format of '{position}' is not known")


def dec_to_opencpn_string(position: float, latitude: bool = True) -> str:
    """
    Converts latitude or longitude Decimal Degree value float to
    the corresponding OpenCPN-formatted string.

    Raises a ``ValueError`` if the input value is not within
    the expected range.

    Args:
        position: a latitude or longitude decimal value, e.g. ``42.301``
        latitude: whether the passed position is a latitude value, if
            false it is instead interpreted as a longitude value

    Returns:
        The Decimal Degrees equivalent of the provided string
    """

    hours, minutes, direction = dd_to_ddm(position, latitude)
    if latitude:
        return f"{hours:02d}° {minutes:07.4f}' {direction}"
    return f"{hours:03d}° {minutes:07.4f}' {direction}"


def polar_to_cartesian(angle: float, distance: float) -> Tuple[float, float]:
    """
    Convert polar coordinates in a plane to cartesian coordinates.

    Args:
        angle: the angle component of the coordinates (theta)
        distance: the distance component of the coordinates (r)

    Returns:
        The equivalent coordinates in cartesian space.
    """
    angle = radians((angle + 360) % 360)
    x = distance * cos(angle)
    y = distance * sin(angle)
    return (x, y)


def cartesian_to_polar(x: float, y: float) -> Tuple[float, float]:
    """
    Convert cartesian coordinates in a plane to polar coordinates.

    Args:
        x: the x component
        y: the y component

    Returns:
        The equivalent polar coordinates.
    """
    distance = sqrt(x**2 + y**2)
    theta = degrees((atan2(y, x)))
    return ((360 + theta) % 360, distance)


def polar_vector_addition(
    angle_a: float, distance_a: float, angle_b: float, distance_b: float
) -> Tuple[float, float]:
    """
    Add two polar coordinates (or vectors) in a plane, returns the
    normalized (angle, distance) value of the sum.

    Args:
        angle_a: the angle of the first vector (theta_a)
        distance_a: the distance of the first vector (r_a)
        angle_b: the angle of the second vector (theta_b)
        distance_b: the distance of the second vector (r_b)

    Returns:
        The sum of the two vectors.
    """
    angle_a = radians(angle_a)
    angle_b = radians(angle_b)
    distance = sqrt(
        distance_a**2
        + distance_b**2
        + 2 * distance_a * distance_b * cos(angle_a - angle_b)
    )
    angle = angle_a + atan2(
        distance_b * sin(angle_b - angle_a),
        distance_a + distance_b * cos(angle_b - angle_a),
    )
    return (degrees(angle) % 360, distance)


def relative_heading(heading_own: float, heading_other: float) -> float:
    """
    Given the ownship's and another ship's true headings in degrees,
    return the other ship's normalized heading relatively to ownship's.

    Args:
        heading_own: the heading of the vessel from the point of view of which
            the relative heading should be computed
        heading_other: the absolute heading of the target vessel

    Returns:
        The heading of the target vessel relative to our own.
    """
    return (720.0 - heading_own + heading_other) % 360


def distance_polar(
    angle_a: float, distance_a: float, angle_b: float, distance_b: float
) -> float:
    """
    Given a pair of polar coordinates in degrees (0-360, r) in a plane,
    returns the distance between the two points.

    Args:
        angle_a: the angle of the first point (theta_a)
        distance_a: the distance of the first point (r_a)
        angle_b: the angle of the second point (theta_b)
        distance_b: the distance of the second point (r_b)

    Returns:
        The distance between the two points.
    """

    angle_a = radians((angle_a + 360) % 360)
    angle_b = radians((angle_b + 360) % 360)
    return sqrt(
        distance_a**2
        + distance_b**2
        - (2 * distance_a * distance_b * cos(angle_a - angle_b))
    )


def shift_long(lat: float, long: float, distance: float) -> Tuple[float, float]:
    """
    Shift the longitude of a point by ``distance``.
    Distance < 0 indicates to shift westwards.

    Args:
        lat: latitude of the point to shift
        long: longitude of the point to shift
        distance: distance by which the point should be shifted in meters

    Returns:
        The latitude and longitude of the shifted point.
    """
    phi_a = radians(lat)
    lambda_a = radians(long)

    lambda_b = lambda_a + 2 * asin(distance / (2 * EARTH_RADIUS * cos(phi_a)))
    return (lat, degrees(lambda_b))


def shift_lat(lat: float, long: float, distance: float) -> Tuple[float, float]:
    """
    Shift the latitude of a point by ``distance``.
    Distance < 0 indicates to shift northwards.

    Args:
        lat: latitude of the point to shift
        long: longitude of the point to shift
        distance: distance by which the point should be shifted in meters

    Returns:
        The latitude and longitude of the shifted point.
    """
    phi_a = radians(lat)

    phi_b = phi_a + 2 * asin(distance / (2 * EARTH_RADIUS))
    return (degrees(phi_b), long)


def relative_bearing(
    heading: float, lat_own: float, long_own: float, lat_other: float, long_other: float
) -> float:
    """
    Computes the relative bearing of a point in degrees given the current position and heading.

    Args:
        heading: heading of the reference, should be in degrees and between 0 and 360
        lat_own: latitude of the reference point
        long_own: longitude of the reference point
        lat_other: latitude of the target
        long_other: longitude of the target

    Returns:
        The bearing of the target point relative to the given reference point and heading.
    """
    lat_own = radians(lat_own)
    long_own = radians(long_own)
    lat_other = radians(lat_other)
    long_other = radians(long_other)

    delta = long_other - long_own

    # formula for the initial bearing between the two points in radians
    theta = atan2(
        sin(delta) * cos(lat_other),
        cos(lat_own) * sin(lat_other) - sin(lat_own) * cos(lat_other) * cos(delta),
    )

    # mapping to degrees and substraction of the current heading to get the relative bearing in degrees
    return ((theta * 180 / pi) + 360 - heading) % 360


def distance_harvesine(
    lat_a: float, long_a: float, lat_b: float, long_b: float
) -> float:
    """
    Harvesine formula for the distance between two points given their latitude
    and longitude, with the assumption of the Earth being perfectly spherical.

    Args:
        lat_a: latitude of the first point
        long_a: longitude of the first point
        lat_b: latitude of the second point
        long_b: longitude of the second point

    Returns:
        The distance between the two points, taking the curvature of the Earth into account.
    """
    phi_a = radians(lat_a)
    phi_b = radians(lat_b)
    lambda_a = radians(long_a)
    lambda_b = radians(long_b)

    return (
        2
        * EARTH_RADIUS
        * asin(
            sqrt(
                sin((phi_b - phi_a) / 2) ** 2
                + cos(phi_a) * cos(phi_b) * sin((lambda_b - lambda_a) / 2) ** 2
            )
        )
    )
