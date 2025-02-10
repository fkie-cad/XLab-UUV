#!/usr/bin/env python3
import argparse
import re
import socket
import time


def main():
    parser = argparse.ArgumentParser("Network Traffic Generator")
    parser.add_argument(
        "--ip",
        type=str,
        required=True,
        help="Target IPv4 address where the traffic is sent to.",
    )
    parser.add_argument(
        "--port",
        type=int,
        required=True,
        help="Target port for the data`s destination.",
    )
    parser.add_argument(
        "--data-rate",
        required=True,
        type=str,
        help='Data rate, e.g. "100m" for 100 mbps. \
                                Use "k" for kbps, "m" for mbps and "g" for gbps.',
    )
    parser.add_argument(
        "--size", type=int, default=1400, help="Size of each UDP packet"
    )
    args = parser.parse_args()

    data_rate = parse_data_rate(args.data_rate)
    generate_traffic(
        ip=args.ip, port=args.port, data_rate_bps=data_rate, packet_size=args.size
    )


def parse_data_rate(data_rate: str) -> int:
    pattern = re.compile(r"^\d+[kmg]$", re.IGNORECASE)
    if not pattern.match(data_rate):
        raise ValueError("Invalid format for the data rate.")
    unit = data_rate[-1].lower()
    value = int(data_rate[:-1])
    if unit == "k":
        return value * 1e3
    elif unit == "m":
        return value * 1e6
    elif unit == "g":
        return value * 1e9
    else:
        raise ValueError(f"Unknown Unit `{unit}`. Choose from `k`, `m` or `g`.")


def generate_traffic(ip: str, port: int, data_rate_bps: int, packet_size):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    target = (ip, port)
    packet = b"." * packet_size
    packet_size_bits = packet_size / 8
    interval = packet_size_bits / data_rate_bps

    try:
        while True:
            sock.sendto(packet, target)
            time.sleep(interval)
    except KeyboardInterrupt:
        print("Stop sending traffic ...")
    finally:
        sock.close()


if __name__ == "__main__":
    main()
