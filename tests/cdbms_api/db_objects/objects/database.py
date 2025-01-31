from cdbms_api.connection import Connection
from cdbms_api.db_objects.db_object import DBobject
from cdbms_api.db_objects.objects.table.table import Table


class Database(DBobject):
    def __init__(self, name: str, connection: Connection | None = None) -> None:
        super(Database, self).__init__(connection=connection)

        self.name: str = name
        self._tables: list[Table] = []

    def add_table(self, table_name: str, access: str, **kwargs) -> Table:
        table: Table = Table(table_name=table_name, access=access, connection=self._connection, **kwargs)
        self._execute_querry(
            f'{self.name} create table {table_name} {access} columns ( {" ".join([x.body for x in table.get_columns()])} )\0'
        )

        table._database = self.name
        return table

    def get_table(self, table_name: str, access: str, **kwargs) -> Table:
        table: Table = Table(table_name=table_name, access=access, connection=self._connection, **kwargs)
        table._database = self.name
        return table

    def remove_table(self, table: Table) -> bytes | int | None:
        return self._execute_querry(f'{self.name} delete table {table.name}\0')

    def sync(self) -> bytes | int | None:
        return self._execute_querry(f'{self.name} sync\0')

    def rollback(self) -> bytes | int | None:
        return self._execute_querry(f'{self.name} rollback\0')
