# /// script
# dependencies = [
#     "prompt-toolkit>=3.0.0",
#     "rich>=13.0.0",
# ]
# ///

import socket
import sys
import time
import shlex
from datetime import datetime
from prompt_toolkit import PromptSession
from prompt_toolkit.completion import NestedCompleter
from prompt_toolkit.history import FileHistory
from rich.console import Console
from rich.text import Text

console = Console()


class BlockyClient:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.sock = None
        self.known_keys = set()

        self.commands = ["SET", "GET", "DEL", "EXISTS",
                         "EXPIRE", "TTL", "PING", "ECHO", "QUIT", "EXIT"]
        self.session = PromptSession(history=FileHistory(".blocky_history"))

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        console.print(f"[bold green][+][/bold green] Connected to KV server on [bold cyan]{self.host}:{self.port}[/bold cyan]")

    def get_completer(self):
        """Generate dynamic autocompletion"""
        keys_dict = {k: None for k in self.known_keys}
        return NestedCompleter.from_nested_dict({
            "SET": keys_dict,
            "GET": keys_dict,
            "DEL": keys_dict,
            "EXISTS": keys_dict,
            "EXPIRE": keys_dict,
            "TTL": keys_dict,
            "PING": None,
            "ECHO": None,
            "QUIT": None,
            "EXIT": None
        })

    def encode_resp(self, cmd_str: str) -> tuple[bytes, list[str]]:
        try:
            parts = shlex.split(cmd_str.strip())
        except ValueError:
            parts = cmd_str.strip().split()

        if not parts:
            return b"", []

        resp = f"*{len(parts)}\r\n"
        for part in parts:
            resp += f"${len(part)}\r\n{part}\r\n"
        return resp.encode('utf-8'), parts

    def update_key_states(self, parts: list[str], response: str):
        """Updates local key knowledge base based on server response."""
        if not parts:
            return

        cmd = parts[0].upper()

        if cmd == "SET" and len(parts) >= 3 and response.startswith("+OK"):
            self.known_keys.add(parts[1])
        elif cmd == "DEL" and len(parts) >= 2 and response.startswith(":") and not response.startswith(":0"):
            self.known_keys.discard(parts[1])
        elif cmd in ["GET", "TTL", "EXISTS"] and len(parts) >= 2:
            key = parts[1]
            if response.startswith("$-1") or response.startswith(":-2") or (cmd == "EXISTS" and response == ":0"):
                self.known_keys.discard(key)

    def print_formatted_response(self, raw_resp: str, rtt_ms: float):
        """Prints network response with timestamp and syntax highlighting."""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        if raw_resp.startswith("+"):
            styled_resp = Text(raw_resp.strip(), style="bold green")
        elif raw_resp.startswith("-"):
            styled_resp = Text(raw_resp.strip(), style="bold red")
        elif raw_resp.startswith(":"):
            styled_resp = Text(raw_resp.strip(), style="bold magenta")
        elif raw_resp.startswith("$"):
            styled_resp = Text(raw_resp.strip(), style="bold yellow")
        else:
            styled_resp = Text(raw_resp.strip(), style="cyan")

        console.print(f"[dim][{timestamp}][/dim] < ",
                      styled_resp, f" [dim cyan]({rtt_ms:.2f} ms)[/dim cyan]")

    def run(self):
        self.connect()

        try:
            while True:
                prompt_str = f"blocky ({len(self.known_keys)} keys)> "
                cmd = self.session.prompt(
                    prompt_str, completer=self.get_completer())

                if not cmd.strip():
                    continue
                if cmd.lower() in ["exit", "quit", "q"]:
                    break

                payload, parts = self.encode_resp(cmd)
                if not payload:
                    continue

                start_time = time.perf_counter()
                self.sock.sendall(payload)
                response_bytes = self.sock.recv(4096)
                end_time = time.perf_counter()

                if not response_bytes:
                    console.print(
                        "[bold red][-] Server disconnected.[/bold red]")
                    break

                rtt_ms = (end_time - start_time) * 1000
                decoded_resp = response_bytes.decode('utf-8', errors='replace')

                self.update_key_states(parts, decoded_resp)
                self.print_formatted_response(decoded_resp, rtt_ms)

        except (KeyboardInterrupt, EOFError):
            console.print("\n[yellow][!] Session closed.[/yellow]")
        finally:
            if self.sock:
                self.sock.close()


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    client = BlockyClient("127.0.0.1", port)
    client.run()
