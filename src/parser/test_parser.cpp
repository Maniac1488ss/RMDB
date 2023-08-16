/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */
#undef NDEBUG

#include <cassert>

#include "parser.h"

int main() {
    std::vector<std::string> sqls = {
//        "show tables;",
//        "desc tb;",
//        "create table tb (a int, b float, c char(4));",
//        "drop table tb;",
//        "create index tb(a);",
//        "create index tb(a, b, c);",
//        "drop index tb(a, b, c);",
//        "drop index tb(b);",
//        "insert into tb values (1, 3.14, 'pi');",
//        "delete from tb where a = 1;",
//        "update tb set a = 1, b = 2.2, c = 'xyz' where x = 2 and y < 1.1 and z > 'abc';",
//        "select * from tb;",
//        "select * from tb where x <> 2 and y >= 3. and z <= '123' and b < tb.a;",
//        "select x.a, y.b from x, y where x.a = y.b and c = d;",
//        "select x.a, y.b from x join y where x.a = y.b and c = d;",
//        "exit;",
//        "help;",
//        "INSERT INTO t VALUES(372036854775807,233421);",
//        "select SUM(id) as sum_id from aggregate;",
//        "select MAX(id) as max_id from aggregate;",
//        "select COUNT(name) as count_name from aggregate where val = 2.0;",
//        "select COUNT(id) as count_id from aggregate;",
//        "select COUNT(*) as max_name from t1;",
//        "show index from warehouse;",
//        "select * from warehouse where w_id <> 10;",
//        "SELECT company, order_number FROM orders ORDER BY order_number;",
//        "SELECT company, order_number FROM orders ORDER BY company, order_number;",
//        "SELECT company, order_number FROM orders ORDER BY company DESC, order_number ASC;",
//        "SELECT company, order_number FROM orders ORDER BY order_number ASC LIMIT 2;",
//        "load file_name into table_name;",
//        "set output_file off",
        "update student set score=score-5.5 where id<3;",

        "",
    };
    for (auto &sql : sqls) {
        std::cout << sql << std::endl;
        YY_BUFFER_STATE buf = yy_scan_string(sql.c_str());
        assert(yyparse() == 0);
        if (ast::parse_tree != nullptr) {
            ast::TreePrinter::print(ast::parse_tree);
            yy_delete_buffer(buf);
            std::cout << std::endl;
        } else {
            std::cout << "exit/EOF" << std::endl;
        }
    }
    ast::parse_tree.reset();
    return 0;
}
