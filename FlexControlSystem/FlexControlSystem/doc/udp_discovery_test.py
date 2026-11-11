# -*- coding: utf-8 -*-
"""
W5500 UDP 发现通道独立验证脚本(不依赖 Qt 上位机)。

用法:
    python udp_discovery_test.py               # 只广播(扫描本机各网段的定向广播)
    python udp_discovery_test.py 192.168.2.190 # 先单播指定 IP, 再广播

判断依据:
    - 单播通、广播不通 -> 设备 UDP 服务正常, 广播被交换机/网卡过滤
    - 单播广播都不通   -> 设备侧问题(未烧新固件 / UDP 命令未生效)
    - 脚本收到应答而 Qt 收不到 -> Windows 防火墙拦了 Qt 程序的入站 UDP

协议: 55 AA | 11 | 01 | 01 | chkH chkL | AA 55
应答: 55 AA | 12 | 04 | IP1..IP4 | chkH chkL | AA 55
"""
import socket
import struct
import sys
import time

DISCOVERY_PORT = 5000


def build_request():
    body = bytes([0x11, 0x01, 0x01])
    chk = (~sum(body) + 1) & 0xFFFF
    return bytes([0x55, 0xAA]) + body + bytes([chk >> 8, chk & 0xFF]) + bytes([0xAA, 0x55])


def parse_respond(data):
    """校验应答帧, 成功返回 IP 字符串, 否则返回 None"""
    if len(data) != 12:
        return None
    if data[0] != 0x55 or data[1] != 0xAA or data[2] != 0x12 or data[3] != 0x04:
        return None
    if data[10] != 0xAA or data[11] != 0x55:
        return None
    chk = (~sum(data[2:8]) + 1) & 0xFFFF
    if chk != ((data[8] << 8) | data[9]):
        return None
    return "%d.%d.%d.%d" % (data[4], data[5], data[6], data[7])


def local_broadcasts():
    """按本机各网卡 IPv4 地址算定向广播地址(W5500 收不到 255.255.255.255)"""
    out = set()
    try:
        import subprocess
        txt = subprocess.check_output(["ipconfig"], shell=True).decode("gbk", "ignore")
    except Exception:
        return ["255.255.255.255"]

    ip = mask = None
    for line in txt.splitlines():
        line = line.strip()
        if line.lower().startswith(("ipv4", "ipv4 ")) or "IPv4" in line:
            ip = line.split(":")[-1].strip()
        elif "子网掩码" in line or "Subnet Mask" in line:
            mask = line.split(":")[-1].strip()
            if ip and mask:
                try:
                    ipn = struct.unpack("!I", socket.inet_aton(ip))[0]
                    mkn = struct.unpack("!I", socket.inet_aton(mask))[0]
                    out.add(socket.inet_ntoa(struct.pack("!I", ipn | (~mkn & 0xFFFFFFFF))))
                except Exception:
                    pass
            ip = mask = None
    out.add("255.255.255.255")
    return sorted(out)


def probe(targets, label, wait=2.0):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind(("", 0))
    sock.settimeout(0.2)
    frame = build_request()

    for t in targets:
        try:
            sock.sendto(frame, (t, DISCOVERY_PORT))
            print("[%s] -> %s" % (label, t))
        except Exception as e:
            print("[%s] -> %s FAILED: %s" % (label, t, e))

    got = False
    deadline = time.time() + wait
    while time.time() < deadline:
        try:
            data, addr = sock.recvfrom(128)
        except socket.timeout:
            continue
        ip = parse_respond(data)
        print("[%s] RECV from %s: %s -> %s" % (label, addr[0], data.hex(), ip))
        got = got or (ip is not None)
    if not got:
        print("[%s] 无应答" % label)
    sock.close()
    return got


if __name__ == "__main__":
    if len(sys.argv) > 1:
        ok = probe([sys.argv[1]], "UNICAST")
        if ok:
            sys.exit(0)
    if not probe(local_broadcasts(), "BROADCAST"):
        print("\n结论: 设备未应答。请确认已烧录含 UDP 发现的固件, 且与上位机同网段。")
        sys.exit(1)
