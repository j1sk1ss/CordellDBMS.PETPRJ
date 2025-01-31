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


ROWS: int = 1000
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

# region Get by_index block

random_index: int = random.randint(10, ROWS - 1)
start_time = time.perf_counter()
row = table.get_row_by_index(random_index)
retrieve_time = time.perf_counter() - start_time
print(f'Get [by_index {random_index}]: {retrieve_time:.6f} sec.')
print(f'Name: {row.name}, huid: {row.huid}, uid: {row.uid}, weight: {row.weight}')

#endregion

print()

def _by_exp_str_test(value: str, exp: str, col: str) -> list:
    rows: list | None = table.get_row_by_expression(exp=exp, value=value, column=col, limit=5)
    if isinstance(rows, list):
        return rows
    else:
        return []

# region Get by_exp block [eq]

start_time: float = time.perf_counter()
rows: list = _by_exp_str_test('Porosenok', 'neq', 'name')
retrieve_time = time.perf_counter() - start_time
print(f'Get [by_exp neq]: {retrieve_time:.6f} sec. | Count: {len(rows)}')
for i in rows:
    print(f'Name: {i.name}, huid: {i.huid}, uid: {i.uid}, weight: {i.weight}')

# endregion

print()

# region Get by_exp block [>]

start_time = time.perf_counter()
rows = _by_exp_str_test('249', '>', 'weight')
retrieve_time = time.perf_counter() - start_time
print(f'Get [by_exp >]: {retrieve_time:.6f} sec. | Count: {len(rows)}')
for i in rows:
    print(f'Name: {i.name}, huid: {i.huid}, uid: {i.uid}, weight: {i.weight}')

# endregion

connection.close_connection()
