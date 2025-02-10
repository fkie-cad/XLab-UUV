#!/usr/bin/env python3
import socket
import sys
import time


def receive_and_forward(recv_ip, recv_port, send_ip, send_port):
    # Setup sockets
    sock_recv = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock_recv.bind((recv_ip, recv_port))
    sock_send = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    while True:  # Receive and forward
        data, addr = sock_recv.recvfrom(2048)
        print(f"{addr} -> {send_ip}:{send_port} {data}")
        sock_send.sendto(data, (send_ip, send_port))


if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: ./socket-copy.py [sip] [sport] [dip] [dport]")
        sys.exit(-1)

    while True:
        try:
            receive_and_forward(
                sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4])
            )
        except Exception as e:
            print(e)
            time.sleep(0.25)
