**About this project:**
-----------------------------------------------------
I just want to create alternative of SQL like DBMS. For example my own PostgreSQL, but simpler and lighter. I want use this DBMS on embended systems like STM32F103C6T8 or Arduino NANO.

**ToDo list:**
-----------------------------------------------------
1) STM32 brunch (Complete) </br>
2) Reach 40 KB size (Complete) </br>
3) Test on hardware </br>

**Docs:**
-----------------------------------------------------

*CREATE* </br>
Create function template:
```
create database <db_name>
<db_name> create table <tb_name> <rwd> columns ( <col_name> <size> <str/int/any/"<module_name>=args,<mpre/mpost/both>"> <p/np> <a/na> ... )
```
Create function examples:
```
create database db1
db create table table_1 000 columns ( col1 10 str is_primary na col2 10 any np na )
db create table table_1 000 columns ( col1 10 calc=col2*10,mpre is_primary na col2 10 any np na )
db create table table_1 000 columns ( uid 5 int p a name 8 str np na password 8 "hash=password 8,mpre" np na )
```

*APPEND* </br>
Append function template: </br>
Note: This function take solid string without any separators for columns.
```
<db_name> append row <tb_name> values <data>
```
Append function example:
```
db append row table_1 values "hello     second col"
```

*UPDATE* </br>
Update function template:
```
<db_name> update row <tb_name> by_index <index> values <new_data>
<db_name> update row <tb_name> by_value column <col_name> value <value> values <new_data>
```
Update function example: 
```
db update row table_1 by_index 0 "goodbye   hello  bye"
db update row table_1 by_value column col1 value "goodbye" values "goodbye   hello  bye"
```

*GET* </br>
Get function template:
```
<db_name> get row <tb_name> by_index <index>
<db_name> get row <tb_name> by_value column <col_name> value <value>
<db_name> get row <tb_name> by_exp column <col_name> <expression (</>/!=)> <value>
```
Get function example: 
```
db get row table_1 by_index 0
db get row table_1 by_value column col2 value "value"
db get row table_1 by_exp column col2 "!=" "100"
```

*DELETE* </br>
Delete function template:
```
delete database <db_name>
<db_name> delete table <tb_name>
<db_name> delete row <tb_name> by_index <index>
<db_name> delete row <tb_name> by_value column <col_name> value <value>
```
Delete function example:
```
delete database db
db delete table table_1
db delete row table_1 by_index 0
db delete row table_1 by_value column col1 value 'data'
```

*SYNC* </br>
Sync function template:
```
<db_name> sync
```
Sync function example:
```
db sync
```

*ROLLBACK* </br>
Rollback function template:
```
<db_name> rollback
```
Rollback function example:
```
db rollback
```
