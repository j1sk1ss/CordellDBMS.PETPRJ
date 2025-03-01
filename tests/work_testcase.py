import os
import shutil
import time
import glob
import random

from cdbms_api.connection import Connection
from cdbms_api.db_objects.objects.table.table import Table
from cdbms_api.db_objects.objects.database import Database
from cdbms_api.db_objects.objects.manager import DatabaseManager
from cdbms_api.db_objects.objects.table.column import Column, ColumnDataType, ColumnType
from cdbms_api.db_objects.objects.table.table import Expressions, LogicOperator, Statement


def _global_test(
    create_test: bool = True 
) -> None:
    start_test_time: float = time.perf_counter()

    ROWS: int = 10000
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
        uid=Column('uid', ColumnDataType.INT, [ColumnType.NOT_PRIMARY, ColumnType.WHITOUT_AUTO], 8),
        huid=Column('huid', ColumnDataType.INT, [ColumnType.NOT_PRIMARY, ColumnType.WHITOUT_AUTO], 8),
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

    print('\n[Test] Creating database, table and test data...')
    start_time = time.perf_counter()
    table.append_row(uid=0, huid=0, name='FirstTest', weight=1)

    for i in range(ROWS):
        table.append_row(uid=0, huid=i, name='Porosenok', weight=random.randint(100, 250))

    table.append_row(uid=0, huid=ROWS + 1, name='SecondTest', weight=1)

    insert_time = time.perf_counter() - start_time
    print(f'[Time] Test data insert time: {insert_time:.4f} sec.')

    database.sync()

    random_index: int = random.randint(1, ROWS - 2)
    start_time = time.perf_counter()
    row = table.get_row_by_index(random_index)
    retrieve_time = time.perf_counter() - start_time
    print(f'[Time] Get time [by_index {random_index}]: {retrieve_time:.6f} sec.')
    assert row.name == "Porosenok", "Data incorrect"

    start_time: float = time.perf_counter()
    rows: list = _by_exp_str_test(
        expression=[
            Statement(column_name="name", expression=Expressions.STR_NOT_EQUALS, value="Porosenok"),
            LogicOperator.AND,
            Statement(column_name="huid", expression=Expressions.MORE_THEN, value=ROWS - 1),
        ]
    )

    retrieve_time = time.perf_counter() - start_time
    print(f'[Time] Get time [by_exp name != Porosenok and huid > ROWS - 1]: {retrieve_time:.6f} sec. | Count: {len(rows)}')
    for i in rows:
        assert i.name != "Porosenok" and i.huid > ROWS - 1, "Test failed. Data incorrect"

    database.sync()

    # endregion

    # region [READ (GET)]

    print('\n[Test] Getting data by expressions test...')
    start_time: float = time.perf_counter()
    rows: list = _by_exp_str_test(
        expression=[
            Statement(column_name="name", expression=Expressions.STR_NOT_EQUALS, value="Porosenok"),
            LogicOperator.AND,
            Statement(column_name="huid", expression=Expressions.MORE_THEN, value=30),
        ]
    )

    retrieve_time = time.perf_counter() - start_time
    print(f'[Time] Get time [by_exp name != Porosenok and huid > 30]: {retrieve_time:.6f} sec. | Count: {len(rows)}')
    for i in rows:
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
    print(f'[Time] Get time [by_exp weight > 249 or name != Porosenok]: {retrieve_time:.6f} sec. | Count: {len(rows)}')
    for i in rows:
        assert i.weight > 249 or i.name != "Porosenok", "Test failed. Data incorrect"

    # endregion

    # region [UPDATE]

    print('\n[Test] Update row by expression test...')
    start_time: float = time.perf_counter()
    table.insert_row_by_expression(
        expression=[
            Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Porosenok")
        ], 
        uid=1, huid=1, name="Svinya", weight=500
    )

    retrieve_time = time.perf_counter() - start_time
    print(f'[Time] Update time [by_exp name == Porosenok >> Svinya]: {retrieve_time:.6f} sec.')

    rows: list = _by_exp_str_test(
        expression=[
            Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Svinya")
        ],
        limit=50
    )

    for i in rows:
        assert i.name == "Svinya" and i.weight == 500, "Test failed. Data incorrect"


    database.sync()

    # endregion

    # region [DELETE]

    print('\n[Test] Row delete by expression test...')
    start_time: float = time.perf_counter()
    table.delete_row_by_expression(
        expression=[
            Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Svinya")
        ]
    )

    retrieve_time = time.perf_counter() - start_time
    print(f'[Time] Delete time [by_exp name == Svinya]: {retrieve_time:.6f} sec.')
        
    rows: list = _by_exp_str_test(
        expression=[
            Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Svinya")
        ]
    )

    assert len(rows) == 0, "Deleted data exists"
    database.sync()

    # endregion

    # region [APPEND TO FREE REGIONS]

    print('\n[Test] Try to append data into free region (Check 000000.pg for correct result)')
    start_time = time.perf_counter()
    for i in range(50):
        table.append_row(uid=i, huid=i, name='Kitty', weight=random.randint(100, 250))

    insert_time = time.perf_counter() - start_time
    print(f'[Time] Append data time: {insert_time:.4f} sec.')
    database.sync()

    start_time: float = time.perf_counter()
    rows: list = _by_exp_str_test(
        expression=[
            Statement(column_name="name", expression=Expressions.STR_EQUALS, value="Kitty")
        ], limit=55
    )

    retrieve_time = time.perf_counter() - start_time
    assert len(rows) == 50, f"Rows wasn't append: {len(rows)}/50"
    for i in rows:
        assert i.name == "Kitty", "Wrong name for new data"

    database.sync()

    # endregion

    # region [CHECK TEST DATA]

    print('\n[Test] Test data should stay in database (FirstTest and SecondTest)...')
    start_time: float = time.perf_counter()
    rows: list = _by_exp_str_test(
        expression=[
            Statement(column_name="name", expression=Expressions.STR_EQUALS, value="FirstTest"),
            LogicOperator.OR,
            Statement(column_name="name", expression=Expressions.STR_EQUALS, value="SecondTest"),
        ],
        limit=10
    )

    retrieve_time = time.perf_counter() - start_time
    print(f'[Time] Test data get from database time: {retrieve_time:.6f} sec.')

    assert (len(rows)) == 2, f"FirstTest and SecondTest count is not 2 | count: {len(rows)}/2"
    for i in rows:
        assert i.name in [ 'FirstTest', 'SecondTest' ], "Not test value"

    # endregion

    connection.close_connection()
    print('\nTest Complete\nIf you see this message, be sure, all functions in CDBMS works correct!\n')
    retrieve_time = time.perf_counter() - start_test_time
    print(f'[Time] All tests time: {retrieve_time:.6f} sec.')

    print('\n')
    

def _delete_files(folder: str, extensions: list):
    if not os.path.exists(folder):
        print(f"Dir {folder} does not exist!")
        return
    
    deleted_files = 0
    for ext in extensions:
        files = glob.glob(os.path.join(folder, f"*.{ext}"))
        for file in files:
            try:
                os.remove(file)
                print(f"Deleted file: {file}")
                deleted_files += 1
            except Exception as e:
                print(f"Delete error {file}: {e}")

    if deleted_files == 0:
        print("Files not found!")
    else:
        print(f"Deleted files count: {deleted_files}")

    deleted_folders = 0
    for root, dirs, _ in os.walk(folder, topdown=False):
        for dir_name in dirs:
            dir_path = os.path.join(root, dir_name)
            try:
                shutil.rmtree(dir_path)
                print(f"Deleted empty folder: {dir_path}")
                deleted_folders += 1
            except Exception as e:
                print(f"Delete error {dir_path}: {e}")

    if deleted_folders == 0:
        print("No empty folders found!")
    else:
        print(f"Deleted empty folders count: {deleted_folders}")


if __name__ == "__main__":
    try:
        while True:
            try:
                _global_test()
            except AssertionError as ex:
                print("Test failed! Text:", str(ex))
            finally:
                _delete_files("/Users/nikolaj/Documents/Repositories/CordellDBMS.EXMPL/builds", ["db", "pg", "dr", "tb"])
            
            input("\nPress any key to continue...")
    except KeyboardInterrupt:
        print("\nTest stopped.")
