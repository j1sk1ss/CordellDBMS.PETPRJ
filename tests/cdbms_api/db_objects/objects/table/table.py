from __future__ import annotations

from cdbms_api.connection import Connection
from cdbms_api.db_objects.db_object import DBobject
from cdbms_api.db_objects.objects.table.row import Row
from cdbms_api.db_objects.objects.table.column import Column


class Table(DBobject):
    def __init__(
        self, table_name: str, access: str, connection: Connection | None = None, **kwargs
    ) -> None:
        super(Table, self).__init__(connection=connection)

        self.name = table_name
        self._access = access
        self._columns = []
        self._database = ''

        if isinstance(kwargs, dict):
            for column in kwargs.keys():
                self._columns.append(kwargs[column])

# region [Rows]

    def append_row(self, **kwargs) -> int:
        querry: str = ''
        for column in self._columns:
            value = kwargs.get(column.name, " " * column.size)
            if not isinstance(value, str):
                querry += ("0" * (column.size - len(str(value)))) + str(value)
            else:
                querry += (" " * (column.size - len(value))) + value

        return_code = self._execute_querry(f'{self._database} append row {self.name} values "{querry}"\0')
        if isinstance(return_code, int):
            if return_code == -20:
                raise ValueError("Primary check failed!")
            elif return_code == -14:
                raise ValueError("Table type error!")
            elif return_code == -12:
                raise ValueError("Table wrong signture error! Expected int!")
            elif return_code == -4:
                raise ValueError("Table not found!")
            elif return_code == -3:
                raise ValueError("Access denied!")

            return 1
        else:
            return -1

    def insert_row_by_index(self, row: Row, index: int) -> bytes | int | None:
        return self._execute_querry(f'{self._database} update row {self.name} by_index {index} "{row.data}"\0')

    def insert_row_by_value(self, row: Row, value: str, column: Column) -> bytes | int | None:
        column_name: str = column.name if isinstance(column, Column) else column
        return self._execute_querry(f'{self._database} update row {self.name} by_value column {column_name} value "{value}" values "{row.data}"\0')

    def get_row_by_index(self, index: int):
        row_body: bytes | int | None = self._execute_querry(querry=f'{self._database} get row {self.name} by_index {index}\0', is_code=False)
        if not isinstance(row_body, bytes):
            print('[ERROR] row_body should be bytes')
            return None

        row: Row = Row(data=row_body)
        return row.parse_bytes_to_object(self._columns)

    def get_row_by_expression(self, exp: str, value: str, column: Column | str, limit: int = -1) -> list | None:
        column_name: str = column.name if isinstance(column, Column) else column
        row_body: bytes | int | None = self._execute_querry(
            querry=f'{self._database} get row {self.name} by_exp column {column_name} "{exp}" "{value}" limit {limit}\0', is_code=False
        )

        if not isinstance(row_body, bytes):
            return None

        column_sizes: list | None = self._split_row_params()
        if column_sizes is None:
            return None

        row_size: int = sum(column_sizes)
        rows: int = len(row_body) // row_size

        output: list = []
        for x in range(rows):
            row: Row = Row(data=row_body[x * row_size:(x + 1) * row_size])
            output.append(row.parse_bytes_to_object(self._columns))

        return output

    def delete_row_by_index(self, index: int) -> bytes | int | None:
        return self._execute_querry(f'{self._database} delete row {self.name} by_index {index}\0')

    def delete_row_by_value(self, value: str, column: Column | str) -> bytes | int | None:
        column_name: str = column.name if isinstance(column, Column) else column
        return self._execute_querry(f'{self._database} delete row {self.name} by_value column {column_name} value {value}\0')

    def _split_row_params(self) -> list | None:
        column_sizes: list = []
        for column in self._columns:
            column_sizes.append(column.size)

        return column_sizes

# endregion

    def link_to_table(self, master_column: str, slave_table: Table, slave_column: str, args: list[str]) -> bytes | int | None:
        return self._execute_querry(
            f'{self._database} link master {self.name} {master_column} to_slave {slave_table.name} {slave_column} ( {" ".join(args)} )\0'
        )

    def get_columns(self) -> list:
        return self._columns
