**About this project:**
-----------------------------------------------------
I just want to create alternative of SQL like DBMS. For example my own PostgreSQL, but simpler and lighter. I want use this DBMS on embended systems like STM32F103C6T8 or Arduino NANO.</br>
Also this project can be launched on Windows, MacOS (ARM / Intel), and Linux (Ubuntu, Debiand and Fedora tested).</br>

**Testing:**
-----------------------------------------------------
### Size of executable file

| OS                       | Size (bytes)|
|--------------------------|-------------|
| Ubuntu 24.04.1           | 34488       |
| Fedora 40                | 35536       |
| Windows 10 22H2          | 37376       |
| MacOSX 10.15.7           | 87064       |

### Insert and get speed:

| Rows count     | Insert time (sec.) | Get time (sec.) |
|----------------|--------------------|-----------------|
| 1000           | 1.8628             | 0.002097        |
| 5000           | 8.9934             | 0.001761        |
| 10000          | 18.4760            | 0.001736        |


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
create database db
db create table table_1 000 columns ( col1 10 str is_primary na col2 10 any np na )
db create table table_1 000 columns ( col1 10 calc=col2*10,mpre is_primary na col2 10 any np na )
db create table table_1 000 columns ( uid 5 int p a name 8 str np na password 8 "hash=password 8,mpre" np na )
```

----------------
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
----------------
*GET* </br>
Get function template:
```
<db_name> get row <tb_name> by_index <index>
<db_name> get row <tb_name> by_exp column <col_name> <expression (</>/!=/=/eq/neq)> <value> <and/or> ... limit <count>
```
Get function example: 
```
db get row table_1 by_index 0
db get row table_1 by_exp column col2 != 100
db get row table_1 by_exp column col2 eq "hello world" and column col1 > 200 limit 15
```
P.S. *eq* and *neq* will compare strings, instead converting data to int. </br>
P.P.S. Limit is optional. Providing -1 to limit will return all entries. </br>

----------------
*UPDATE* </br>
Update function template:
```
<db_name> update row <tb_name> <new_data> by_index <index>
<db_name> update row <tb_name> <new_data> by_exp column <col_name> <expression (</>/!=/=/eq/neq)> <value> <and/or> ... limit <count>
```
Update function example: 
```
db update row table_1 "goodbye   hello  bye" by_index 0
db update row table_1 "goodbye   hello  bye" by_exp column col1 = 10 and col2 eq "Hello there!"
```

----------------
*DELETE* </br>
Delete function template:
```
<db_name> delete database
<db_name> delete table <tb_name>
<db_name> delete row <tb_name> by_index <index>
<db_name> delete row <tb_name> by_exp column <col_name> <expression (</>/!=/=/eq/neq)> <value> <and/or> ... limit <count>
```
Delete function example:
```
delete database db
db delete table table_1
db delete row table_1 by_index 0
db delete row table_1 by_exp column col2 "eq" "hello world"
```

----------------
*MIGRATE* </br>
Migrate function template:
```
<db_name> migrate <src_table_name> <dst_table_name> nav ( ... )
```
Migrate function example:
```
db migrate table_1 table_2 nav ( 0 1 1 3 4 2 )
```
P.S. In breckets placed navigation of columns. This means next:
- 0-index columns will transfer data to 1-index column from destination table.
- 1-index columns will transfer data to 3-index column from destination table.
- 4-index columns will transfer data to 2-index column from destination table.

----------------
*SYNC* </br>
Sync function template:
```
<db_name> sync
```
Sync function example:
```
db sync
```

----------------
*ROLLBACK* </br>
Rollback function template:
```
<db_name> rollback
```
Rollback function example:
```
db rollback
```
