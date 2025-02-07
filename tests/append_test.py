# CDMS Test case
# 1) Append test (with speed test) of N rows V
# 2) Index get test V
# 3) Expression get test
#   3.1) By value (equals) V
#   3.2) By value (nequals) V
#   3.3) By integer (equals) V
#   3.4) By integer (nequals) V
#   3.5) By integer (bigger) V
#   3.6) By integer (smaller) V
# 4) Delete test
#   4.1) Row delete by index
#   4.2) Try to get row

import time
import random

from cdbms_api.connection import Connection
from cdbms_api.db_objects.objects.table.table import Table
from cdbms_api.db_objects.objects.database import Database
from cdbms_api.db_objects.objects.manager import DatabaseManager
from cdbms_api.db_objects.objects.table.column import Column, ColumnDataType, ColumnType
from cdbms_api.db_objects.objects.table.table import Expressions, LogicOperator, Statement


ROWS: int = 50
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

# region Insert block

start_time = time.perf_counter()
for i in range(ROWS):
    table.append_row(uid=i, huid=i, name='Kitty', weight=random.randint(100, 250))

insert_time = time.perf_counter() - start_time
print(f'Insert: {insert_time:.4f} sec.')

# endregion

print()

def _by_exp_str_test(expression: list[Statement, LogicOperator]) -> list:
    rows: list | None = table.get_row_by_expression(expression=expression, limit=-1)
    if isinstance(rows, list):
        return rows
    else:
        return []

# region Get by_exp block [neq and >]

start_time: float = time.perf_counter()
rows: list = _by_exp_str_test(
    expression=[
        Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Kitty")
    ]
)

retrieve_time = time.perf_counter() - start_time
assert len(rows) >= ROWS, f"Rows wasn't append: {len(rows)}/{ROWS}"

# endregion


database.sync()
connection.close_connection()
