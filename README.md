I just want to create alternative of SQL like DBMS

##Docs:
-----------------------------------------------------
Examples:

*CREATE*:
```
Example: db create database db1
Example: db create table table_1 000 columns ( col1 10 str is_primary na col2 10 any np na )
```

*APPEND*:
```
Example: db append row table_1 values "hello     second col"
```

*UPDATE*:
```
Example: db update row table_1 by_index 0 "goodbye   hello  bye"
Example: db update row table_1 by_value column col1 value "goodbye   hello  bye"
```

*SYNC*:
```
Example: db sync
```

*ROLLBACK*:
```
Example: db rollback
```

*GET*:
```
Example: db get row table_1 by_value column col2 value "value"
Example: db get row table_1 by_index
```

*DELETE*:
```
Example: db delete table table_1
Example: db delete row table_1 by_index 0
Example: db delete row table_1 by_value column col1 value 'data'
```

*LINK*:
```
Example: db link table1 col1 table2 col1 ( capp cdel )
```
