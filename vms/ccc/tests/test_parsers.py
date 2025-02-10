import pytest

from xluuv_ccc.messages_pb2 import ApRoute, ApRouteXML, Coordinates, Waypoint
from xluuv_ccc.server.cccserver_servicer import _parse_route_xml


@pytest.mark.parametrize(
    "route_xml_fn,route",
    [
        (
            "tests/snapshots/test-loiter.gpx",
            ApRoute(
                id=0,
                name="SimpleEstuary-XLUUV-loiter",
                planned_speed=8.0,
                waypoints=[
                    Waypoint(
                        coords=Coordinates(
                            latitude=50.031394577, longitude=-9.984622835
                        ),
                        name="001",
                    ),
                    Waypoint(
                        coords=Coordinates(
                            latitude=50.027911736, longitude=-9.984399261
                        ),
                        name="002",
                    ),
                ],
            ),
        ),
        (
            "tests/snapshots/test-loiter-sharedwpviz.gpx",
            ApRoute(
                id=0,
                name="SimpleEstuary-XLUUV-loiter",
                planned_speed=8.0,
                waypoints=[
                    Waypoint(
                        coords=Coordinates(
                            latitude=50.031394577, longitude=-9.984622835
                        ),
                        name="001",
                    ),
                    Waypoint(
                        coords=Coordinates(
                            latitude=50.027911736, longitude=-9.984399261
                        ),
                        name="002",
                    ),
                ],
            ),
        ),
    ],
)
def test_route_xml_parser(route_xml_fn: str, route: ApRoute) -> None:
    route_xml = ApRouteXML()
    with open(route_xml_fn, "rt") as ifile:
        route_xml.route_xml = ifile.read()

    route_xml.id = 0

    parsed_route, _ = _parse_route_xml(route_xml)

    assert parsed_route is not None
    # assert parsed_route == route
    assert parsed_route.name == route.name
    assert parsed_route.id == route.id
    assert parsed_route.planned_speed == route.planned_speed

    assert len(parsed_route.waypoints) == len(route.waypoints)

    for i, wpt in enumerate(parsed_route.waypoints):
        ref = route.waypoints[i]
        assert wpt.coords.latitude == ref.coords.latitude
        assert wpt.coords.longitude == ref.coords.longitude
        assert wpt.name == ref.name
