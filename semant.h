/*
 * semant.h - COOL语义分析器头文件
 */

#ifndef SEMANT_H_
#define SEMANT_H_

#include <iostream>
#include <set>
#include <vector>
#include <string>
#include "cool-tree.h"
#include "symtab.h"

// 错误报告函数声明
void semant_error();
void semant_error(Class_ c);
void semant_error(Symbol filename, tree_node *t);

//////////////////////////////////////////////////////////////////////
// ClassTable类 - 语义分析器的核心数据结构
//////////////////////////////////////////////////////////////////////

class ClassTable {
private:
    int semant_errors;                     // 错误计数器
    ostream& error_stream;                 // 错误输出流
    
    // 类表：存储所有类的符号表
    SymbolTable<Symbol, Class_> *class_table;
    
    // 基本类的成员变量（避免悬空指针）
    Class_ Object_class;
    Class_ IO_class;
    Class_ Int_class;
    Class_ Bool_class;
    Class_ String_class;
    
    // 私有方法
    void install_basic_classes();          // 安装基本类
    void build_inheritance_graph(Classes classes); // 构建继承图
    void check_inheritance();              // 检查继承关系
    
    // 类型检查方法
    void type_check_class(Class_ c);       // 检查单个类
    Symbol type_check_expression(Expression expr, 
                                 Symbol current_class,
                                 SymbolTable<Symbol, Symbol>* object_env,
                                 const char* filename);
    
    // 辅助方法
    bool is_subtype(Symbol child, Symbol parent);  // 检查子类型关系
    Symbol lub(Symbol type1, Symbol type2);        // 计算最小上界
    method_class* find_method(Symbol class_name, Symbol method_name); // 查找方法
    
public:
    // 构造函数
    ClassTable(Classes classes);
    
    // 公共方法
    void type_check();                     // 执行类型检查
    int errors() { return semant_errors; } // 获取错误数量
    
    // 迭代器支持
    typedef SymbolTable<Symbol, Class_>::iterator iterator;
    iterator begin() { return class_table->begin(); }
    iterator end() { return class_table->end(); }
    
    // 类查找
    Class_* get_class(Symbol name) {
        return class_table->lookup(name);
    }
    
    // 获取基本类的方法
    Class_ get_object_class() { return Object_class; }
    Class_ get_io_class() { return IO_class; }
    Class_ get_int_class() { return Int_class; }
    Class_ get_bool_class() { return Bool_class; }
    Class_ get_string_class() { return String_class; }
};

#endif /* SEMANT_H_ */
