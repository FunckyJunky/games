#!/usr/bin/env python3
"""XREAL One / One Pro head tracking for Leviathan Voyage (error.blue/voyage).

The glasses send their motion sensors (gyroscope, accelerometer) over their USB-C network link, which
a web page cannot reach. This little program reads them and passes them on to the game in your
browser, on this computer only (ws://localhost:8765/ws). Nothing goes anywhere else. It speaks as the
XREAL bridge Mistwalkers uses (Start XREAL VR.bat): run either one, not both (they share the port).

  1. Plug the glasses into this computer (USB-C). In the glasses' menu set them to Follow mode
     (stabilisation off): the sensors only stream then.
  2. Run it:  python xreal-bridge.py      (Windows: double-click xreal-bridge.bat)
     Leave the window open while you play.
  3. In the game: XREAL · VR in the top bar > VR mode: On, then Calibrate.

Needs Python 3.8 or newer and nothing else (python.org). Stop it with Ctrl+C.
Only the game's own page (error.blue, or a copy opened from this computer) may connect;
python xreal-bridge.py --any-site lets any page in; --glasses=HOST:PORT reads glasses elsewhere.
"""
import base64
import hashlib
import socket
import struct
import sys
import threading
import time

GLASSES = [("169.254.2.1", 52998), ("169.254.1.1", 52998)]   # the glasses' end of their USB-C link
HOST, PORT = "127.0.0.1", 8765                                # where the game connects (ws://localhost:8765/ws): this computer only
HEADER = bytes((0x28, 0x36, 0x00, 0x00, 0x00, 0x80))          # each sensor record starts so
SENSOR = bytes((0x00, 0x40, 0x1F, 0x00, 0x00, 0x40))          # ...and has this in it
MSG = 84                                                      # the part of a record we read
WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
SITES = ("error.blue", "localhost", "127.0.0.1", "[::1]")    # the pages that may connect
ANY_SITE = "--any-site" in sys.argv
for _a in sys.argv[1:]:                                       # --glasses HOST:PORT (another address)
    if _a.startswith("--glasses="):
        _h, _, _p = _a.split("=", 1)[1].rpartition(":")
        GLASSES = [(_h, int(_p))]

clients = []
lock = threading.Lock()
status = {"text": "looking for the glasses"}


def ws_frame(payload, opcode):
    n = len(payload)
    if n < 126:
        head = struct.pack("!BB", 0x80 | opcode, n)
    elif n < 65536:
        head = struct.pack("!BBH", 0x80 | opcode, 126, n)
    else:
        head = struct.pack("!BBQ", 0x80 | opcode, 127, n)
    return head + payload


def broadcast(payload, opcode=2):
    frame = ws_frame(payload, opcode)
    with lock:
        for c in list(clients):
            try:
                c.sendall(frame)
            except OSError:
                clients.remove(c)
                try:
                    c.close()
                except OSError:
                    pass


def say(text):
    if status["text"] != text:
        print(text, flush=True)
    status["text"] = text
    broadcast(('{"status":"%s"}' % text).encode(), 1)


def site_ok(origin):
    """The page asking: the game's site, a copy opened from this computer (file://: no origin), or none."""
    if ANY_SITE or not origin or origin == "null" or origin.startswith("file:"):
        return True
    host = origin.split("://", 1)[-1].split("/", 1)[0].rsplit(":", 1)[0].lower() if "]" not in origin else \
        origin.split("://", 1)[-1].split("]", 1)[0] + "]"
    return any(host == s or host.endswith("." + s) for s in SITES)


def serve_client(conn):
    try:
        conn.settimeout(5)
        req = b""
        while b"\r\n\r\n" not in req:
            d = conn.recv(4096)
            if not d:
                return
            req += d
            if len(req) > 16384:
                return
        key, origin = None, None
        for line in req.decode("latin-1").split("\r\n"):
            low = line.lower()
            if low.startswith("sec-websocket-key:"):
                key = line.split(":", 1)[1].strip()
            elif low.startswith("origin:"):
                origin = line.split(":", 1)[1].strip()
        if not site_ok(origin):
            print("refused a page from %s (use --any-site to let it in)" % origin, flush=True)
            conn.sendall(b"HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n")
            return
        if not key:   # (opened in a browser tab: just say how it is)
            conn.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nAccess-Control-Allow-Origin: *\r\n\r\n"
                         b"xreal-bridge: " + status["text"].encode() + b"\n")
            return
        accept = base64.b64encode(hashlib.sha1((key + WS_GUID).encode()).digest()).decode()
        conn.sendall(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                      "Sec-WebSocket-Accept: %s\r\n\r\n" % accept).encode())
        conn.settimeout(None)
        conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        conn.sendall(ws_frame(('{"status":"%s"}' % status["text"]).encode(), 1))
        with lock:
            clients.append(conn)
        print("the game connected", flush=True)
        while True:   # (what the page sends is not needed: read it until it closes)
            d = conn.recv(4096)
            if not d or (d[0] & 0x0F) == 8:
                break
    except OSError:
        pass
    finally:
        with lock:
            if conn in clients:
                clients.remove(conn)
        try:
            conn.close()
        except OSError:
            pass


def server():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.bind((HOST, PORT))
    except OSError:
        print("Port %d is busy: is a bridge already running (this one, or Start XREAL VR.bat)? One is enough." % PORT, flush=True)
        sys.exit(1)
    s.listen(4)
    print("Waiting for the game on ws://%s:%d" % (HOST, PORT), flush=True)
    while True:
        conn, _ = s.accept()
        threading.Thread(target=serve_client, args=(conn,), daemon=True).start()


def decode(m):
    """One sensor record: (time in seconds, gyroscope rad/s x3, accelerometer m/s2 x3), or None."""
    ts = struct.unpack_from("<Q", m, 14)[0]
    v = struct.unpack_from("<6f", m, 34)
    for x in v:
        if x != x or abs(x) > 1000.0:
            return None
    a2 = v[3] * v[3] + v[4] * v[4] + v[5] * v[5]
    if not 25.0 < a2 < 225.0:   # (gravity, give or take)
        return None
    return ts / 1e9, v


def glasses():
    while True:
        sock = None
        for addr in GLASSES:
            try:
                sock = socket.create_connection(addr, timeout=2)
                break
            except OSError:
                sock = None
        if sock is None:
            say("no glasses: plug them in (USB-C), set Follow mode")
            time.sleep(2)
            continue
        say("glasses connected")
        sock.settimeout(3)
        buf = b""
        out = []
        prev = None
        last = time.perf_counter()
        n, n_t = 0, last
        try:
            while True:
                d = sock.recv(16384)
                if not d:
                    break
                buf += d
                while True:
                    i = buf.find(HEADER)
                    if i < 0:
                        buf = buf[-(len(HEADER) - 1):]
                        break
                    if i:
                        buf = buf[i:]
                    if len(buf) < MSG:
                        break
                    m = buf[:MSG]
                    r = decode(m) if SENSOR in m else None
                    if r is None:
                        buf = buf[len(HEADER):]
                        continue
                    buf = buf[MSG:]
                    dt = r[0] - prev if prev is not None else 0.001
                    prev = r[0]
                    if not 0.0002 < dt < 0.05:
                        dt = 0.001
                    out.append(struct.pack("<7f", dt, *r[1]))   # 28 bytes: dt (s), gyro (rad/s) x3, accel (m/s2) x3
                    n += 1
                now = time.perf_counter()
                if out and now - last > 0.004:   # (a batch every few milliseconds)
                    broadcast(b"".join(out))
                    out = []
                    last = now
                if now - n_t > 5:
                    print("  %d readings a second" % int(n / (now - n_t)), flush=True)
                    n, n_t = 0, now
        except OSError:
            pass
        try:
            sock.close()
        except OSError:
            pass
        say("glasses lost: trying again")
        time.sleep(1)


if __name__ == "__main__":
    print(__doc__.split("\n\n")[0], flush=True)
    threading.Thread(target=server, daemon=True).start()
    try:
        glasses()
    except KeyboardInterrupt:
        pass
