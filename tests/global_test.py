import time
import random

from cdbms_api.connection import Connection
from cdbms_api.db_objects.objects.table.table import Table
from cdbms_api.db_objects.objects.database import Database
from cdbms_api.db_objects.objects.manager import DatabaseManager
from cdbms_api.db_objects.objects.table.column import Column, ColumnDataType, ColumnType
from cdbms_api.db_objects.objects.table.table import Expressions, LogicOperator, Statement


start_test_time: float = time.perf_counter()


ROWS: int = 100
connection: Connection = Connection(
    base_addr='0.0.0.0',
    port=7777,
    username='root',
    password='root'
)


manager: DatabaseManager = DatabaseManager(connection=connection)
database: Database = manager.create_database('dbtest')
table: Table = database.add_table(
    table_name='pigs', access='same',
    uid=Column('uid', ColumnDataType.INT, [ColumnType.NOT_PRIMARY, ColumnType.WHITOUT_AUTO], 4),
    huid=Column('huid', ColumnDataType.INT, [ColumnType.NOT_PRIMARY, ColumnType.WHITOUT_AUTO], 4),
    name=Column('name', ColumnDataType.STR, [ColumnType.NOT_PRIMARY, ColumnType.WHITOUT_AUTO], 16),
    weight=Column('weight', ColumnDataType.INT, [ColumnType.NOT_PRIMARY, ColumnType.WHITOUT_AUTO], 4)
)

def _by_exp_str_test(expression: list[Statement, LogicOperator], limit: int = 5) -> list:
    rows: list | None = table.get_row_by_expression(expression=expression, limit=limit)
    if isinstance(rows, list):
        return rows
    else:
        return []
    

# region [CREATE]

print('Creating database, table and test data...')
start_time = time.perf_counter()
table.append_row(uid=0, huid=0, name='FirstTest', weight=1)

for i in range(ROWS):
    table.append_row(uid=0, huid=i, name='Porosenok', weight=random.randint(100, 250))

table.append_row(uid=0, huid=ROWS + 1, name='SecondTest', weight=1)

insert_time = time.perf_counter() - start_time
print(f'Test data insert time: {insert_time:.4f} sec.')

print('Getting test data...')

random_index: int = random.randint(10, ROWS - 1)
start_time = time.perf_counter()
row = table.get_row_by_index(random_index)
retrieve_time = time.perf_counter() - start_time
print(f'Get time [by_index {random_index}]: {retrieve_time:.6f} sec.')
print(f'Name: {row.name}, huid: {row.huid}, uid: {row.uid}, weight: {row.weight}')

start_time: float = time.perf_counter()
rows: list = _by_exp_str_test(
    expression=[
        Statement(column_name="name", expression=Expressions.STR_NOT_EQUALS, value="Porosenok"),
        LogicOperator.AND,
        Statement(column_name="huid", expression=Expressions.MORE_THEN, value=ROWS - 1),
    ]
)

retrieve_time = time.perf_counter() - start_time
print(f'Get time [by_exp name != Porosenok and huid > ROWS - 1]: {retrieve_time:.6f} sec. | Count: {len(rows)}')
for i in rows:
    print(f'Name: {i.name}, huid: {i.huid}, uid: {i.uid}, weight: {i.weight}')
    assert i.name != "Porosenok" and i.huid > ROWS - 1, "Test failed. Data incorrect"

database.sync()

# endregion

# region [READ (GET)]

print('Getting data by expressions test...')
start_time: float = time.perf_counter()
rows: list = _by_exp_str_test(
    expression=[
        Statement(column_name="name", expression=Expressions.STR_NOT_EQUALS, value="Porosenok"),
        LogicOperator.AND,
        Statement(column_name="huid", expression=Expressions.MORE_THEN, value=30),
    ]
)

retrieve_time = time.perf_counter() - start_time
print(f'Get time [by_exp name != Porosenok and huid > 30]: {retrieve_time:.6f} sec. | Count: {len(rows)}')
for i in rows:
    print(f'Name: {i.name}, huid: {i.huid}, uid: {i.uid}, weight: {i.weight}')
    assert i.name != "Porosenok" and i.huid > 30, "Test failed. Data incorrect"

start_time = time.perf_counter()
rows: list = _by_exp_str_test(
    expression=[
        Statement(column_name="weight", expression=Expressions.MORE_THEN, value=249),
        LogicOperator.OR,
        Statement(column_name="name", expression=Expressions.STR_NOT_EQUALS, value="Porosenok")
    ]
)

retrieve_time = time.perf_counter() - start_time
print(f'Get time [by_exp weight > 249 or name != Porosenok]: {retrieve_time:.6f} sec. | Count: {len(rows)}')
for i in rows:
    print(f'Name: {i.name}, huid: {i.huid}, uid: {i.uid}, weight: {i.weight}')
    assert i.weight > 249 or i.name != "Porosenok", "Test failed. Data incorrect"

# endregion

# region [UPDATE]

print('Update row by expression test...')
start_time: float = time.perf_counter()
table.insert_row_by_expression(
    expression=[
        Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Porosenok")
    ], 
    uid=1, huid=1, name="Svinya", weight=500
)

retrieve_time = time.perf_counter() - start_time
print(f'Update time [by_exp name == Porosenok >> Svinya]: {retrieve_time:.6f} sec.')

rows: list = _by_exp_str_test(
    expression=[
        Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Svinya")
    ]
)

print('Check update values:')
for i in rows:
    print(f'Name: {i.name}, huid: {i.huid}, uid: {i.uid}, weight: {i.weight}')
    assert i.weight == 500, "Test failed. Data incorrect"


database.sync()

# endregion

# region [DELETE]

print('Row delete by expression test...')
start_time: float = time.perf_counter()
table.delete_row_by_expression(
    expression=[
        Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Svinya")
    ]
)

retrieve_time = time.perf_counter() - start_time
print(f'Delete time [by_exp name == Svinya]: {retrieve_time:.6f} sec.')
    
rows: list = _by_exp_str_test(
    expression=[
        Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Svinya")
    ]
)

assert len(rows) == 0, "Deleted data exists"
database.sync()

# endregion

# region [APPEND TO FREE REGIONS]

print('Try to append data into free region (Check 000000.pg for correct result)')
start_time = time.perf_counter()
for i in range(ROWS):
    table.append_row(uid=i, huid=i, name='Kitty', weight=random.randint(100, 250))

insert_time = time.perf_counter() - start_time
print(f'Append data time: {insert_time:.4f} sec.')

start_time: float = time.perf_counter()
rows: list = _by_exp_str_test(
    expression=[
        Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Kitty")
    ], limit=ROWS
)

retrieve_time = time.perf_counter() - start_time
assert len(rows) == ROWS, f"Rows wasn't append: {len(rows)}/{ROWS}"

print('New rows:')
for i in rows:
    print(f'Name: {i.name}, huid: {i.huid}, uid: {i.uid}, weight: {i.weight}')
    assert i.name == "Kitty", "Wrong name for new data"

database.sync()

# endregion

# region [CHECK TEST DATA]

print('Test data should stay in database (FirstTest and SecondTest)...')
start_time: float = time.perf_counter()
rows: list = _by_exp_str_test(
    expression=[
        Statement(column_name="name", expression=Expressions.STR_NOT_EQUALS, value="Porosenok"),
        LogicOperator.AND,
        Statement(column_name="name", expression=Expressions.STR_NOT_EQUALS, value="Kitty"),
    ]
)

retrieve_time = time.perf_counter() - start_time
print(f'Test data get from database time: {retrieve_time:.6f} sec.')

assert (len(rows)) == 2, "FirstTest and SecondTest not exists"
for i in rows:
    print(f'Name: {i.name}, huid: {i.huid}, uid: {i.uid}, weight: {i.weight}')
    assert i.name in [ 'FirstTest', 'SecondTest' ]

# endregion

print('TEST COMPLETE')
retrieve_time = time.perf_counter() - start_test_time
print(f'All tests time: {retrieve_time:.6f} sec.')
