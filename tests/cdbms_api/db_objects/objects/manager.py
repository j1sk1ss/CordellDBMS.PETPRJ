import pandas as pd

from cdbms_api.connection import Connection
from cdbms_api.db_objects.db_object import DBobject
from cdbms_api.db_objects.objects.database import Database
from cdbms_api.db_objects.objects.table.table import Table
from cdbms_api.db_objects.objects.table.column import Column, ColumnDataType, ColumnType


class DatabaseManager(DBobject):
    def __init__(self, connection: Connection) -> None:
        super(DatabaseManager, self).__init__(connection=connection)

    def create_database(self, name: str) -> Database:
        database: Database = Database(name)
        self._execute_querry(f'create database {database.name}\0')
        database.set_connection(self._connection)
        return database

    def select_database(self, name: str) -> Database:
        database: Database = Database(name)
        database.set_connection(self._connection)
        return database

    def delete_database(self, name: str) -> None:
        self._execute_querry(f'delete database {name}')

    def from_csv(
        self, path: str, database: Database | None = None, table: Table | None = None, limit: int = 0
    ) -> None:
        dataframe: pd.DataFrame = pd.read_csv(path)
        columns: list = list(dataframe.columns)
        dataframe_name: str = path.split('/')[-1].split('.')[0]

        if database is None:
            database = self.create_database(dataframe_name)

        if table is None:
            columns_body: dict = {}
            for column, dtype in dataframe.dtypes.items():
                if pd.api.types.is_integer_dtype(dtype):
                    column_type: ColumnDataType = ColumnDataType.INT
                elif pd.api.types.is_string_dtype(dtype):
                    column_type: ColumnDataType = ColumnDataType.STR
                else:
                    column_type: ColumnDataType = ColumnDataType.ANY

                column_size: int = int(dataframe[column].map(len).max())
                column_body: Column = Column(
                    name=column, data=column_type,
                    type=[ColumnType.NOT_PRIMARY, ColumnType.WHITOUT_AUTO],
                    size=column_size
                )

                columns_body[column] = column_body

            table = database.add_table(dataframe_name, 'same', **columns_body)

        for _, row in dataframe.iterrows():
            row_data = { column: row[column] for column in columns }
            table.append_row(**row_data)

    def to_csv(self, table: Table, path: str, count: int = 0) -> None:
        pass

    def kill(self):
        if self._connection is None:
            return

        self._connection.close_connection()
