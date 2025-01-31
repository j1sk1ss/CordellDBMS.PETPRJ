from cdbms_api.connection import Connection


class DBobject:
    def __init__(self, connection: Connection | None) -> None:
        self._connection = connection
        if self._connection is not None:
            self._connection.open_connection()

    def get_connection(self) -> Connection | None:
        return self._connection

    def set_connection(self, connection: Connection | None) -> None:
        self._connection = connection

    def _execute_querry(self, querry: str, is_code: bool = True) -> bytes | int | None:
        if self._connection is None:
            return None

        answer_code: bytes | None = (
            self._connection
            .send_data(querry)
            .get_data()
        )

        if answer_code is None:
            return None

        code = answer_code[0] if is_code else answer_code
        if code == 5:
            raise ValueError("Incorrect querry provided!")

        return code
