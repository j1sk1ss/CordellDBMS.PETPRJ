from cdbms_api.db_objects.objects.table.column import ColumnDataType


class Row:
    def __init__(self, data: bytes | None = None, string_data: str | None = None, index: int = -1) -> None:
        self._index = index

        if data is not None:
            self._data = data
        elif string_data is not None:
            self._data = string_data.encode('utf-8')
        else:
            self._data = bytes(0)

    def split(self, columns: list | None) -> list | None:
        if columns is None:
            return None

        output: list = []
        lower_bound: int = 0
        upper_bound: int = 0
        for column in columns:
            upper_bound = column
            if len(self._data) <= upper_bound:
                return None

            output.append(self._data[lower_bound:upper_bound])
            lower_bound = column

        return output

    def parse_bytes_to_object(self, columns: list):
        QuerryAnswer = type('QuerryAnswer', (object,), {})
        obj = QuerryAnswer()

        offset = 0
        for column in columns:
            column_data = self._data[offset:offset + column.size]
            offset += column.size

            value = column_data.decode('utf-8').strip()
            if column.data_type == ColumnDataType.INT:
                value = int(column_data.decode('utf-8').strip())
            elif column.data_type == ColumnDataType.FLT:
                value = float(column_data.decode('utf-8').strip())

            setattr(obj, column.name, value)

        return obj

    def __str__(self) -> str:
        return self._data.decode('utf-8')

    @property
    def data(self) -> bytes:
        return self._data

    @property
    def index(self) -> int:
        return self._index
