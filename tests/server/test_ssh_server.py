#!/usr/bin/env python3
"""Local SSH + SFTP server used for Nyan-SSH integration testing.

Not part of the shipped application.  Requires paramiko (pip install paramiko).

Usage:  python tests/server/test_ssh_server.py [--port 2222]
Credentials: nyan / nyan123
"""
from __future__ import annotations

import argparse
import os
import posixpath
import socket
import stat
import threading
import time
from pathlib import Path

import paramiko

ROOT = Path(__file__).resolve().parent / "sftp_root"
HOST_KEY_PATH = Path(__file__).resolve().parent / "host_ecdsa.key"
CLIENT_KEY_PATH = Path(__file__).resolve().parent / "client_rsa.key"
CLIENT_KEY_PUB = Path(__file__).resolve().parent / "client_rsa.pub"
USER = "nyan"
PASSWORD = "nyan123"
AUTHORIZED_CLIENT_KEY = None


def ensure_client_key() -> paramiko.RSAKey:
    global AUTHORIZED_CLIENT_KEY
    if CLIENT_KEY_PATH.exists():
        key = paramiko.RSAKey(filename=str(CLIENT_KEY_PATH))
    else:
        key = paramiko.RSAKey.generate(2048)
        key.write_private_key_file(str(CLIENT_KEY_PATH))
    CLIENT_KEY_PUB.write_text(
        f"{key.get_name()} {key.get_base64()} nyan-test-client\n", encoding="ascii"
    )
    AUTHORIZED_CLIENT_KEY = key
    return key


def ensure_root() -> None:
    files = {
        "README.md": "# Nyan SSH test server\n\nThis directory is served over SFTP.\n",
        "notes.txt": "remember to feed the cat\n",
        "projects/nyan-web/index.html": "<!doctype html><title>nyan</title><h1>Nyan</h1>\n",
        "projects/nyan-web/app.js": "console.log('nyaa');\n",
        "projects/nyan-core/README.md": "# core\n",
        "logs/app.log": "2026-11-04 boot\n2026-11-04 ready\n",
        "图片/猫咪.txt": "你好，世界\n",
        "empty/.keep": "",
    }
    for rel, content in files.items():
        p = ROOT / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        if not p.exists():
            p.write_text(content, encoding="utf-8")
    # a small PNG so the UI has a real binary file to download
    png = ROOT / "图片" / "cat.png"
    if not png.exists():
        import binascii
        # 1x1 orange pixel
        data = binascii.unhexlify(
            "89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c4"
            "890000000d49444154789c6360f8cfc000000301010018dd8db10000000049454e44ae426082"
        )
        png.write_bytes(data)


def human_mode(mode: int) -> str:
    chars = []
    for perm, ch in (
        (stat.S_IRUSR, "r"), (stat.S_IWUSR, "w"), (stat.S_IXUSR, "x"),
        (stat.S_IRGRP, "r"), (stat.S_IWGRP, "w"), (stat.S_IXGRP, "x"),
        (stat.S_IROTH, "r"), (stat.S_IWOTH, "w"), (stat.S_IXOTH, "x"),
    ):
        chars.append(ch if mode & perm else "-")
    kind = "d" if stat.S_ISDIR(mode) else "l" if stat.S_ISLNK(mode) else "-"
    return kind + "".join(chars)


class ShellSession:
    def __init__(self, chan: paramiko.Channel) -> None:
        self.chan = chan
        self.cwd = "/"
        self.history: list[str] = []
        self.buf = ""
        self.closed = False
        self._setup()

    def _setup(self) -> None:
        home = "/home/nyan"
        try:
            os.makedirs(ROOT / home.lstrip("/"), exist_ok=True)
        except OSError:
            pass
        self.cwd = "/"
        self.write("\x1b[1;38;5;39m")
        self.write("  ╭──────────────────────────────────────────────╮\r\n")
        self.write("  │   Nyan SSH · local test server               │\r\n")
        self.write("  ╰──────────────────────────────────────────────╯\x1b[0m\r\n")
        self.write("  type \x1b[33mhelp\x1b[0m for commands, \x1b[33mexit\x1b[0m to disconnect.\r\n\r\n")
        self.prompt()

    def write(self, text: str) -> None:
        try:
            self.chan.sendall(text.encode("utf-8", "replace"))
        except Exception:
            self.closed = True

    def prompt(self) -> None:
        short = self.cwd if self.cwd != "/" else "~"
        self.write(f"\x1b[1;32mnyan@nyan-server\x1b[0m:\x1b[1;34m{short}\x1b[0m$ ")

    def _visible_prompt_len(self) -> int:
        short = self.cwd if self.cwd != "/" else "~"
        return len(f"nyan@nyan-server:{short}$ ")

    def redraw_line(self) -> None:
        self.write("\r\x1b[K")
        self.prompt()
        self.write(self.buf)

    def _resolve(self, arg: str) -> str:
        if not arg:
            return self.cwd
        p = arg if arg.startswith("/") else posixpath.join(self.cwd, arg)
        return posixpath.normpath(p)

    def _real(self, vpath: str) -> Path:
        return ROOT / vpath.lstrip("/")

    def execute(self, line: str) -> None:
        line = line.strip()
        if not line:
            self.prompt()
            return
        self.history.append(line)
        parts = line.split()
        cmd, args = parts[0], parts[1:]
        if cmd in ("exit", "logout", "quit"):
            self.write("bye~\r\n")
            self.chan.close()
            self.closed = True
            return
        if cmd == "clear":
            self.write("\x1b[2J\x1b[H")
            self.prompt()
            return
        if cmd == "pwd":
            self.write(self.cwd + "\r\n")
        elif cmd == "whoami":
            self.write(USER + "\r\n")
        elif cmd == "uname":
            self.write("Linux nyan-server 6.9.0-nyan #1 SMP x86_64 GNU/Linux\r\n")
        elif cmd == "date":
            self.write(time.strftime("%a %b %d %H:%M:%S %Z %Y") + "\r\n")
        elif cmd == "echo":
            self.write(" ".join(args) + "\r\n")
        elif cmd == "cd":
            target = self._resolve(args[0] if args else "/")
            if self._real(target).is_dir():
                self.cwd = target
            else:
                self.write(f"cd: no such file or directory: {args[0] if args else ''}\r\n")
        elif cmd == "cat":
            if not args:
                self.write("cat: missing operand\r\n")
            else:
                p = self._real(self._resolve(args[0]))
                try:
                    data = p.read_bytes()[:65536]
                    text = data.decode("utf-8", "replace")
                    self.write(text.replace("\n", "\r\n"))
                    if not text.endswith("\n"):
                        self.write("\r\n")
                except OSError as exc:
                    self.write(f"cat: {args[0]}: {exc.strerror}\r\n")
        elif cmd in ("ls", "ll", "dir"):
            target = self._resolve(args[0] if args else ".")
            p = self._real(target)
            try:
                entries = sorted(os.scandir(p), key=lambda e: (not e.is_dir(), e.name.lower()))
                for entry in entries:
                    try:
                        st = entry.stat()
                    except OSError:
                        continue
                    mode = human_mode(st.st_mode)
                    color = "\x1b[1;34m" if entry.is_dir() else "\x1b[0m"
                    self.write(f"{mode} {st.st_size:>8}  {color}{entry.name}\x1b[0m\r\n")
            except OSError as exc:
                self.write(f"ls: cannot access '{target}': {exc.strerror}\r\n")
        elif cmd == "help":
            rows = [
                ("ls [dir]", "list a directory"),
                ("cd [dir]", "change directory"),
                ("pwd", "print working directory"),
                ("cat <file>", "print a file"),
                ("echo <text>", "print text"),
                ("clear", "clear the screen"),
                ("uname / whoami / date", "system info"),
                ("exit", "close the session"),
            ]
            for name, desc in rows:
                self.write(f"  \x1b[36m{name:<22}\x1b[0m {desc}\r\n")
        elif cmd == "nyan":
            self.write("\x1b[1;35m  /\\_/\\\r\n ( o.o )  nyaa~\r\n  > ^ <\x1b[0m\r\n")
        else:
            self.write(f"nyan-sh: command not found: {cmd}\r\n")
        self.prompt()

    def feed(self, data: bytes) -> None:
        text = data.decode("utf-8", "replace")
        i = 0
        while i < len(text) and not self.closed:
            ch = text[i]
            if ch == "\x1b" and i + 2 < len(text) and text[i + 1] == "[":
                seq = text[i:i + 3]
                if seq == "\x1b[A":  # up
                    if self.history:
                        self.buf = self.history[-1]
                        self.redraw_line()
                elif seq == "\x1b[B":  # down
                    self.buf = ""
                    self.redraw_line()
                elif seq == "\x1b[C":
                    pass
                elif seq == "\x1b[D":
                    pass
                i += 3
                continue
            if ch in ("\r", "\n"):
                self.write("\r\n")
                self.execute(self.buf)
                self.buf = ""
            elif ch in ("\x7f", "\b"):
                if self.buf:
                    self.buf = self.buf[:-1]
                    self.write("\b \b")
            elif ch == "\x03":
                self.buf = ""
                self.write("^C\r\n")
                self.prompt()
            elif ch == "\x04":
                if not self.buf:
                    self.chan.close()
                    self.closed = True
                    return
            elif ch >= " ":
                self.buf += ch
                self.write(ch)
            i += 1


class Server(paramiko.ServerInterface):
    def __init__(self) -> None:
        self.shell = None
        self.exec_commands: dict[int, str] = {}
        self.subsystems: dict[int, str] = {}
        self.shell_channels: dict[int, bool] = {}
        self.event = threading.Event()

    def check_auth_password(self, username: str, password: str) -> int:
        if username == USER and password == PASSWORD:
            return paramiko.AUTH_SUCCESSFUL
        return paramiko.AUTH_FAILED

    def check_auth_publickey(self, username: str, key) -> int:
        if (
            username == USER
            and AUTHORIZED_CLIENT_KEY is not None
            and key.get_base64() == AUTHORIZED_CLIENT_KEY.get_base64()
        ):
            return paramiko.AUTH_SUCCESSFUL
        return paramiko.AUTH_FAILED

    def get_allowed_auths(self, username: str) -> str:
        return "password,publickey"

    def check_channel_request(self, kind: str, chanid: int) -> int:
        if kind == "session":
            return paramiko.OPEN_SUCCEEDED
        return paramiko.OPEN_FAILED_ADMINISTRATIVELY_PROHIBITED

    def check_channel_shell_request(self, channel) -> bool:
        self.shell_channels[id(channel)] = True
        self.event.set()
        return True

    def check_channel_pty_request(self, channel, term, width, height, pixelwidth, pixelheight, modes) -> bool:
        return True

    def check_channel_window_change_request(self, channel, width, height, pixelwidth, pixelheight) -> bool:
        return True

    def check_channel_exec_request(self, channel, command) -> bool:
        self.event.set()
        text = command.decode("utf-8", "replace") if isinstance(command, bytes) else str(command)
        self.exec_commands[id(channel)] = text
        return True

    def exec_payload(self, text: str) -> str:
        if text.startswith("echo "):
            return text[5:] + "\n"
        if text == "uname -a":
            return "Linux nyan-server 6.9.0-nyan #1 SMP x86_64 GNU/Linux\n"
        if text == "pwd":
            return "/\n"
        return f"executed: {text}\n"

    def check_channel_subsystem_request(self, channel, name) -> bool:
        # let paramiko's subsystem handler registered on the transport do its job
        ok = paramiko.ServerInterface.check_channel_subsystem_request(self, channel, name)
        if ok:
            self.subsystems[id(channel)] = name
        return ok


class SFTPHandle(paramiko.SFTPHandle):
    def stat(self):
        try:
            return paramiko.SFTPAttributes.from_stat(os.fstat(self.readfile.fileno()))
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)

    def chattr(self, attr):
        try:
            if attr.st_mode is not None:
                os.fchmod(self.readfile.fileno(), attr.st_mode)
            if attr.st_uid is not None or attr.st_gid is not None:
                pass
            return paramiko.SFTP_OK
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)


class SFTPServerInterface(paramiko.SFTPServerInterface):
    root = ROOT

    def _real(self, path: str) -> str:
        normalized = posixpath.normpath("/" + (path or "/").lstrip("/"))
        return str(self.root / normalized.lstrip("/"))

    def _rel(self, path: str) -> str:
        normalized = posixpath.normpath("/" + (path or "/").lstrip("/"))
        return normalized

    def list_folder(self, path):
        real = self._real(path)
        try:
            out = []
            for entry in sorted(os.scandir(real), key=lambda e: (not e.is_dir(), e.name.lower())):
                attr = paramiko.SFTPAttributes.from_stat(entry.stat(follow_symlinks=False))
                attr.filename = entry.name
                out.append(attr)
            return out
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)

    def stat(self, path):
        try:
            return paramiko.SFTPAttributes.from_stat(os.stat(self._real(path)))
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)

    def lstat(self, path):
        try:
            return paramiko.SFTPAttributes.from_stat(os.lstat(self._real(path)))
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)

    def open(self, path, flags, attr):
        real = self._real(path)
        try:
            binary_flag = getattr(os, "O_BINARY", 0)
            flags |= binary_flag
            mode = getattr(attr, "st_mode", None) or 0o666
            fd = os.open(real, flags, mode)
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)
        if (flags & os.O_WRONLY) or (flags & os.O_RDWR):
            fstr = "wb"
        else:
            fstr = "rb"
        f = os.fdopen(fd, fstr, buffering=0)
        handle = SFTPHandle(flags)
        handle.filename = real
        handle.readfile = f
        handle.writefile = f
        return handle

    def remove(self, path):
        try:
            os.remove(self._real(path))
            return paramiko.SFTP_OK
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)

    def rename(self, oldpath, newpath):
        try:
            os.rename(self._real(oldpath), self._real(newpath))
            return paramiko.SFTP_OK
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)

    def posix_rename(self, oldpath, newpath):
        return self.rename(oldpath, newpath)

    def mkdir(self, path, attr):
        try:
            os.mkdir(self._real(path), (getattr(attr, "st_mode", None) or 0o777))
            return paramiko.SFTP_OK
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)

    def rmdir(self, path):
        try:
            os.rmdir(self._real(path))
            return paramiko.SFTP_OK
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)

    def chattr(self, path, attr):
        real = self._real(path)
        try:
            if attr.st_mode is not None:
                os.chmod(real, attr.st_mode)
            if attr.st_uid is not None or attr.st_gid is not None:
                pass
            if attr.st_atime is not None or attr.st_mtime is not None:
                st = os.stat(real)
                os.utime(real, (attr.st_atime or st.st_atime, attr.st_mtime or st.st_mtime))
            return paramiko.SFTP_OK
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)

    def canonicalize(self, path):
        return self._rel(posixpath.join(self._rel("."), path) if not path.startswith("/") else path)

    def readlink(self, path):
        try:
            return os.readlink(self._real(path))
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)

    def symlink(self, target_path, path):
        try:
            os.symlink(target_path, self._real(path))
            return paramiko.SFTP_OK
        except OSError as exc:
            return paramiko.SFTPServer.convert_errno(exc.errno)


def serve_shell(channel: paramiko.Channel, addr) -> None:
    shell = ShellSession(channel)
    try:
        channel.settimeout(1.0)
        while not shell.closed:
            try:
                data = channel.recv(4096)
            except socket.timeout:
                if not channel.active:
                    break
                continue
            except EOFError:
                break
            if not data:
                break
            shell.feed(data)
    except Exception:
        pass
    finally:
        try:
            channel.close()
        except Exception:
            pass
        print(f"[server] shell closed {addr}", flush=True)


def handle_client(client: socket.socket, host_key: paramiko.PKey, addr) -> None:
    transport = paramiko.Transport(client)
    transport.add_server_key(host_key)
    transport.set_subsystem_handler("sftp", paramiko.SFTPServer, SFTPServerInterface)
    server = Server()
    try:
        transport.start_server(server=server)
    except Exception as exc:  # noqa: BLE001
        print(f"[server] handshake failed from {addr}: {exc}", flush=True)
        transport.close()
        return
    print(f"[server] auth ok from {addr}", flush=True)
    try:
        while transport.is_active():
            chan = transport.accept(1.0)
            if chan is None:
                continue
            key = id(chan)
            if key in server.subsystems:
                # paramiko's SFTPServer handler owns the channel
                continue
            command = server.exec_commands.pop(key, None)
            if command is not None:
                try:
                    chan.sendall(server.exec_payload(command))
                    chan.send_exit_status(0)
                finally:
                    chan.close()
                print(f"[server] exec {command!r} from {addr}", flush=True)
                continue

            # The channel request and transport.accept() race each other.  Wait
            # until the request type is known; a subsystem channel is owned by
            # paramiko, while a shell channel is ours to serve.
            deadline = time.time() + 2.0
            while (
                time.time() < deadline
                and key not in server.subsystems
                and key not in server.exec_commands
                and key not in server.shell_channels
                and transport.is_active()
            ):
                time.sleep(0.01)

            if key in server.subsystems:
                continue
            command = server.exec_commands.pop(key, None)
            if command is not None:
                try:
                    chan.sendall(server.exec_payload(command))
                    chan.send_exit_status(0)
                finally:
                    chan.close()
                print(f"[server] exec {command!r} from {addr}", flush=True)
                continue
            if key in server.shell_channels:
                threading.Thread(target=serve_shell, args=(chan, addr), daemon=True).start()
            else:
                try:
                    chan.close()
                except Exception:
                    pass
    except Exception:
        pass
    finally:
        transport.close()
        print(f"[server] disconnected {addr}", flush=True)

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=2222)
    ap.add_argument("--host", default="127.0.0.1")
    args = ap.parse_args()
    ensure_root()
    ensure_client_key()

    if HOST_KEY_PATH.exists():
        host_key = paramiko.ECDSAKey(filename=str(HOST_KEY_PATH))
    else:
        host_key = paramiko.ECDSAKey.generate(bits=256)
        host_key.write_private_key_file(str(HOST_KEY_PATH))
    print(f"[server] host key {host_key.get_name()} fingerprint {host_key.get_fingerprint().hex()}")
    import base64 as _b64
    import hashlib as _hashlib
    _sha256 = _b64.b64encode(_hashlib.sha256(host_key.asbytes()).digest()).decode("ascii").rstrip("=")
    print(f"[server] host key sha256 SHA256:{_sha256}")
    print(f"[server] authorized client key: {CLIENT_KEY_PATH}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((args.host, args.port))
    sock.listen(20)
    print(f"[server] listening on {args.host}:{args.port} (user={USER} password={PASSWORD})")
    print(f"[server] sftp root: {ROOT}")
    while True:
        client, addr = sock.accept()
        threading.Thread(target=handle_client, args=(client, host_key, addr), daemon=True).start()


if __name__ == "__main__":
    main()
