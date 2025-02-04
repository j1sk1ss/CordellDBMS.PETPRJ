import time

from cdbms_api.connection import Connection
from cdbms_api.db_objects.objects.table.table import Table
from cdbms_api.db_objects.objects.database import Database
from cdbms_api.db_objects.objects.manager import DatabaseManager
from cdbms_api.db_objects.objects.table.column import Column, ColumnDataType, ColumnType
from cdbms_api.db_objects.objects.table.table import Expressions, LogicOperator, Statement


connection: Connection = Connection(
    base_addr='0.0.0.0',
    port=7777,
    username='root',
    password='root'
)


manager: DatabaseManager = DatabaseManager(connection=connection)
database: Database = manager.select_database('dbtest')
table: Table = database.get_table(
    table_name='pigs', access='same',
    uid=Column('uid', ColumnDataType.INT, [ColumnType.NOT_PRIMARY, ColumnType.WHITOUT_AUTO], 4),
    huid=Column('huid', ColumnDataType.INT, [ColumnType.NOT_PRIMARY, ColumnType.WHITOUT_AUTO], 4),
    name=Column('name', ColumnDataType.STR, [ColumnType.NOT_PRIMARY, ColumnType.WHITOUT_AUTO], 16),
    weight=Column('weight', ColumnDataType.INT, [ColumnType.NOT_PRIMARY, ColumnType.WHITOUT_AUTO], 4)
)


# region [Update]

start_time: float = time.perf_counter()
table.delete_row_by_expression(
    expression=[
        Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Svinya")
    ]
)

retrieve_time = time.perf_counter() - start_time
print(f'Update [by_exp eq]: {retrieve_time:.6f} sec.')

# endregion

# region [Get]

def _by_exp_str_test(expression: list[Statement, LogicOperator]) -> list:
    rows: list | None = table.get_row_by_expression(expression=expression, limit=5)
    if isinstance(rows, list):
        return rows
    else:
        return []
    

rows: list = _by_exp_str_test(
    expression=[
        Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Svinya")
    ]
)

assert len(rows) == 0, "Deleted data exists"

database.sync()
connection.close_connection()
# endregion