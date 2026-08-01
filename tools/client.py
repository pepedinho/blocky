import socket
import sys


def encode_resp_command(cmd_str: str) -> bytes:
    """
    Transform string in RESP Protocol.
    Ex: "SET key val" -> *3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$3\r\nval\r\n
    """
    parts = cmd_str.strip().split()
    if not parts:
        return b""
    resp = f"*{len(parts)}\r\n"
    for part in parts:
        resp += f"${len(part)}\r\n{part}\r\n"
    return resp.encode('utf-8')


def main():
    host = "127.0.0.1"
    port = int(sys.argv[1]) or 8080

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((host, port))
        print(f"[+] Conntect to KV server on {host}:{port}")
    except Exception as e:
        print(f"[-] Failed to connect: {e}")
        sys.exit(1)

    try:
        while True:
            cmd = input('blocky> ')
            if not cmd.strip():
                continue
            if cmd.lower() in ["exit", "quit", "q"]:
                break

            payload = encode_resp_command(cmd)
            s.sendall(payload)

            response = s.recv(1024)

            if not response:
                print("[-] Server disconnected.")
                break
            print(f"< {response.decode('utf-8', errors='replace').strip()}")

    except KeyboardInterrupt:
        print("\n[!] Interuption.")
    finally:
        s.close()


if __name__ == "__main__":
    main()
