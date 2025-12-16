# PA4-Semantic-Analyzer-
编译原理第四次作业
COOL语义分析器实现
项目概述
本项目实现了一个COOL（Classroom Object-Oriented Language）语言的语义分析器。语义分析器是编译器的重要组成部分，负责对COOL程序进行静态语义检查，确保程序在类型、继承、作用域等方面符合语言规范。

功能特性
核心功能
类型检查：验证所有表达式的类型正确性

继承关系检查：确保类继承关系的合法性

方法重写检查：验证方法重写符合规则

变量作用域检查：确保变量在正确的作用域内使用

表达式类型推断：推断表达式的静态类型

特殊功能
SELF_TYPE支持：正确处理COOL语言中的SELF_TYPE特殊类型

静态类型检查：编译时检测类型错误

错误报告：提供详细的错误信息和位置

实现细节
关键算法
类型推断算法：基于继承关系的类型推导

LUB算法：计算最小上界（Least Upper Bound）

方法查找算法：支持继承链上的方法查找

符号表管理：实现多层作用域的变量管理

数据结构
ClassTable：类表，存储所有类信息

SymbolTable：符号表，支持嵌套作用域

继承图：类继承关系的有向无环图

文件结构
text
.
├── semant.cc              # 语义分析器主实现文件
├── semant.h              # 语义分析器头文件
├── cool-tree.h           # AST节点定义
├── symtab.h              # 符号表实现
├── stack.cl              # 测试用例：栈实现
├── good.cl               # 测试用例：正确程序
├── bad.cl                # 测试用例：错误程序
├── Makefile              # 构建文件
└── README.md             # 项目说明文档
使用方法
环境要求
Unix/Linux系统

C++编译器（g++）

make工具

COOL编译器工具链

编译和运行
链接必要工具：

bash
ln -s /usr/class/bin/lexer .
ln -s /usr/class/bin/parser .
ln -s /usr/class/bin/cgen .
编译语义分析器：

bash
make clean
make semant
运行测试：

bash
# 运行单个测试
./lexer good.cl | ./parser good.cl 2>&1 | ./semant good.cl

# 或者使用mysemant脚本
./mysemant good.cl
测试用例
项目包含多个测试用例：

good.cl：正确的COOL程序

bad.cl：包含语义错误的COOL程序

stack.cl：栈数据结构的实现

关键实现
SELF_TYPE处理
cpp
// SELF_TYPE的特殊规则
if (child == SELF_TYPE) {
    return true;  // SELF_TYPE可以赋值给任何类型
}
if (parent == SELF_TYPE) {
    return false;  // 任何类型不能赋值给SELF_TYPE
}
方法查找
cpp
// 在继承链中查找方法
method_class* ClassTable::find_method(Symbol class_name, Symbol method_name) {
    while (class_name != No_class) {
        Class_ c = get_class(class_name);
        // 在当前类中查找方法
        // 如果找不到，在父类中继续查找
    }
}
类型检查
cpp
// 类型检查主函数
Symbol ClassTable::type_check_expression(Expression expr, 
                                         Symbol current_class,
                                         SymbolTable<Symbol, Symbol>* object_env) {
    // 处理各种表达式类型
    // 设置表达式类型
    expr->set_type(result_type);
    return result_type;
}
设计决策
内存管理
使用动态内存分配存储类和方法信息，避免悬空指针问题。

符号表设计
使用嵌套作用域的符号表，支持变量作用域规则。

错误处理
提供详细的错误信息，包括错误位置和类型信息。

测试结果
语义分析器能够正确：

检测类型不匹配错误

检测未定义变量错误

检测继承循环错误

正确推断表达式类型

处理SELF_TYPE特殊情况

构建说明
项目使用Makefile构建，包含以下目标：

make semant：编译语义分析器

make clean：清理编译文件

make dotest：运行测试

注意事项
不要修改以下文件：

Makefile

cool-tree.aps

tree.{cc|h}

ast-lex.cc, ast-parse.cc

semant-phase.cc

cool-tree.cc

symtab.h

可以修改的文件：

semant.cc

semant.h

cool-tree.h

cool-tree.handcode.h

必须确保输出格式与官方实现完全一致。

参考资料
COOL语言规范

斯坦福CS143课程材料

编译器设计原理

许可证
本项目用于教学目的，遵循课程相关许可协议。

作者：[你的名字]
完成日期：[当前日期]
