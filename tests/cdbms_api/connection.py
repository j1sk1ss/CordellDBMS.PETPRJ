from __future__ import annotations

import socket


class Connection:
    def __init__(self, base_addr: str, port: int, username: str, password: str) -> None:
        self._body_data: bytes | None = bytes(0)
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._addr = base_addr
        self._port = port
        self._code = f'{username}:{password}\0'

        self._is_opened: bool = False

    def open_connection(self) -> Connection:
        if self._is_opened:
            return self
        else:
            self._is_opened = True

        self._socket.connect((self._addr, self._port))
        self._socket.sendall(self._code.encode('utf-8'))
        return self

    def send_data(self, data: str) -> Connection:
        try:
            self._clear_buffer()
            self._socket.sendall(data.encode('utf-8'))
            self._body_data = self._socket.recv(4096)

            if not self._body_data:
                print("Connection closed by the server")

            return self
        except Exception as ex:
            print(f"Error occurred: {ex}")
            self._body_data = None

        return self

    def close_connection(self) -> Connection:
        self._socket.close()
        self._is_opened = False
        return self

    def get_data(self) -> bytes | None:
        return self._body_data

    def _clear_buffer(self):
        try:
            self._socket.settimeout(0.00005)
            while True:
                self._socket.recv(4096)
        except BlockingIOError:
            pass
        except Exception as ex:
            pass
        finally:
            self._socket.settimeout(None)
