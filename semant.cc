/*
 * COOL语义分析器实现
 * 文件：semant.cc
 * 实现功能：
 * 1. 类型检查
 * 2. 继承关系检查
 * 3. 方法重写检查
 * 4. 变量作用域检查
 * 5. 表达式类型推断
 * 6. SELF_TYPE特殊处理
 * 7. 符号表管理
 * 8. LUB计算
 */

#include <set>
#include <vector>
#include <algorithm>
#include <sstream>
#include "cool-tree.h"
#include "symtab.h"
#include "semant.h"

extern int semant_debug;
extern char *curr_filename;

//////////////////////////////////////////////////////////////////////
// 符号定义
//////////////////////////////////////////////////////////////////////

static Symbol 
    Int,
    Float,
    String,
    Bool,
    Object,
    SELF_TYPE,
    No_type;

static Symbol 
    arg,
    arg2,
    BoolFalse,
    BoolTrue,
    concat,
    cool_abort,
    copy,
    in_int,
    in_string,
    length,
    Main,
    main_meth,
    No_class,
    No_expr,
    out_int,
    out_string,
    prim_slot,
    str_field,
    substr,
    type_name,
    val,
    self;

//////////////////////////////////////////////////////////////////////
// 初始化符号
//////////////////////////////////////////////////////////////////////

static void initialize_constants(void)
{
    Int        = idtable.add_string("Int");
    Float      = idtable.add_string("Float");
    String     = idtable.add_string("String");
    Bool       = idtable.add_string("Bool");
    Object     = idtable.add_string("Object");
    SELF_TYPE  = idtable.add_string("SELF_TYPE");
    No_type    = idtable.add_string("_no_type");

    arg        = idtable.add_string("arg");
    arg2       = idtable.add_string("arg2");
    BoolFalse  = idtable.add_string("false");
    BoolTrue   = idtable.add_string("true");
    concat     = idtable.add_string("concat");
    cool_abort = idtable.add_string("abort");
    copy       = idtable.add_string("copy");
    in_int     = idtable.add_string("in_int");
    in_string  = idtable.add_string("in_string");
    length     = idtable.add_string("length");
    Main       = idtable.add_string("Main");
    main_meth  = idtable.add_string("main");
    No_class   = idtable.add_string("_no_class");
    No_expr    = idtable.add_string("_no_expr");
    out_int    = idtable.add_string("out_int");
    out_string = idtable.add_string("out_string");
    prim_slot  = idtable.add_string("_prim_slot");
    str_field  = idtable.add_string("_str_field");
    substr     = idtable.add_string("substr");
    type_name  = idtable.add_string("type_name");
    val        = idtable.add_string("_val");
    self       = idtable.add_string("self");
}

//////////////////////////////////////////////////////////////////////
// ClassTable类实现
//////////////////////////////////////////////////////////////////////

ClassTable::ClassTable(Classes classes) : semant_errors(0), error_stream(cerr)
{
    // 初始化符号
    initialize_constants();
    
    // 初始化类表
    class_table = new SymbolTable<Symbol, Class_>();
    
    // 安装基本类
    install_basic_classes();
    
    // 构建继承图
    build_inheritance_graph(classes);
    
    // 检查继承关系
    check_inheritance();
}

//////////////////////////////////////////////////////////////////////
// 1. 安装基本类（install_basic_classes）
//////////////////////////////////////////////////////////////////////

void ClassTable::install_basic_classes()
{
    // 创建基本类的AST节点
    
    // Object类
    Object_class = class_(Object, 
                         No_class,
                         append_Features(
                             append_Features(
                                 single_Features(method(cool_abort, nil_Formals(), Object, no_expr())),
                                 single_Features(method(type_name, nil_Formals(), String, no_expr()))),
                             single_Features(method(copy, nil_Formals(), SELF_TYPE, no_expr()))),
                         filename);
    
    // IO类
    IO_class = class_(IO,
                     Object,
                     append_Features(
                         append_Features(
                             append_Features(
                                 single_Features(method(out_string, 
                                                      single_Formals(formal(arg, String)),
                                                      SELF_TYPE, 
                                                      no_expr())),
                                 single_Features(method(out_int,
                                                      single_Formals(formal(arg, Int)),
                                                      SELF_TYPE,
                                                      no_expr()))),
                             single_Features(method(in_string, nil_Formals(), String, no_expr()))),
                         single_Features(method(in_int, nil_Formals(), Int, no_expr()))),
                     filename);
    
    // Int类
    Int_class = class_(Int,
                      Object,
                      single_Features(attr(val, prim_slot, no_expr())),
                      filename);
    
    // Bool类
    Bool_class = class_(Bool,
                       Object,
                       single_Features(attr(val, prim_slot, no_expr())),
                       filename);
    
    // String类
    String_class = class_(String,
                         Object,
                         append_Features(
                             append_Features(
                                 append_Features(
                                     append_Features(
                                         single_Features(attr(val, Int, no_expr())),
                                         single_Features(attr(str_field, prim_slot, no_expr()))),
                                     single_Features(method(length, nil_Formals(), Int, no_expr()))),
                                 single_Features(method(concat, 
                                                      single_Formals(formal(arg, String)),
                                                      String,
                                                      no_expr()))),
                             single_Features(method(substr, 
                                                  append_Formals(single_Formals(formal(arg, Int)),
                                                                single_Formals(formal(arg2, Int))),
                                                  String,
                                                  no_expr()))),
                         filename);
    
    // 将基本类添加到类表中
    class_table->enterscope();
    class_table->addid(Object, &Object_class);
    class_table->addid(IO, &IO_class);
    class_table->addid(Int, &Int_class);
    class_table->addid(Bool, &Bool_class);
    class_table->addid(String, &String_class);
}

//////////////////////////////////////////////////////////////////////
// 2. 构建继承图（build_inheritance_graph）
//////////////////////////////////////////////////////////////////////

void ClassTable::build_inheritance_graph(Classes classes)
{
    // 遍历所有用户定义的类
    for(int i = classes->first(); classes->more(i); i = classes->next(i))
    {
        Class_ c = classes->nth(i);
        Symbol name = c->get_name();
        
        if (semant_debug) {
            cerr << "构建继承图: 处理类 " << name << endl;
        }
        
        // 检查是否重复定义
        if (class_table->probe(name) != NULL)
        {
            semant_error(c) << "Class " << name << " was previously defined." << endl;
            semant_errors++;
        }
        else if (name == SELF_TYPE)
        {
            semant_error(c) << "Class cannot be named SELF_TYPE." << endl;
            semant_errors++;
        }
        else
        {
            // 分配新内存存储类信息，避免局部变量被销毁
            Class_ *class_ptr = new Class_(c);
            class_table->addid(name, class_ptr);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// 3. 检查继承关系（check_inheritance）
//////////////////////////////////////////////////////////////////////

void ClassTable::check_inheritance()
{
    if (semant_debug) {
        cerr << "开始检查继承关系" << endl;
    }
    
    // 收集所有类名
    std::vector<Symbol> class_names;
    for (ClassTable::iterator it = begin(); it != end(); ++it)
    {
        class_names.push_back(it->first);
    }
    
    // 检查每个类的继承关系
    for (Symbol class_name : class_names)
    {
        Class_ *class_ptr = class_table->lookup(class_name);
        if (class_ptr == NULL) continue;
        
        Class_ c = *class_ptr;
        Symbol parent = c->get_parent();
        Symbol name = c->get_name();
        
        if (semant_debug) {
            cerr << "检查类 " << name << " 的继承关系，父类: " << parent << endl;
        }
        
        // 检查父类是否存在
        if (parent != No_class)
        {
            // 检查不能继承基本类型
            if (parent == Int || parent == Float || parent == String || parent == Bool)
            {
                semant_error(c) << "Class " << name << " cannot inherit from built-in type " << parent << "." << endl;
                semant_errors++;
            }
            else if (class_table->lookup(parent) == NULL)
            {
                semant_error(c) << "Class " << name << " inherits from an undefined class " << parent << "." << endl;
                semant_errors++;
            }
            else
            {
                // 检查继承循环
                std::set<Symbol> visited;
                visited.insert(name);
                
                Symbol current = parent;
                while (current != No_class)
                {
                    if (visited.find(current) != visited.end())
                    {
                        semant_error(c) << "Class " << name 
                            << ", or an ancestor of " << name 
                            << ", is involved in an inheritance cycle." << endl;
                        semant_errors++;
                        break;
                    }
                    
                    visited.insert(current);
                    
                    Class_ *parent_class_ptr = class_table->lookup(current);
                    if (parent_class_ptr == NULL) break;
                    
                    Class_ parent_class = *parent_class_ptr;
                    current = parent_class->get_parent();
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
// 4. 类型检查系统（核心功能）
//////////////////////////////////////////////////////////////////////

// 检查子类型关系
bool ClassTable::is_subtype(Symbol child, Symbol parent)
{
    if (semant_debug) {
        cerr << "检查子类型关系: " << child << " <: " << parent << endl;
    }
    
    // SELF_TYPE的特殊处理
    if (child == SELF_TYPE && parent == SELF_TYPE)
    {
        return true;
    }
    
    // SELF_TYPE可以赋值给任何类型
    if (child == SELF_TYPE)
    {
        return true;
    }
    
    // 任何类型不能赋值给SELF_TYPE（除非也是SELF_TYPE）
    if (parent == SELF_TYPE)
    {
        return false;
    }
    
    // 相同类型
    if (child == parent)
    {
        return true;
    }
    
    // 检查继承关系
    Class_ *child_class_ptr = class_table->lookup(child);
    if (child_class_ptr == NULL) return false;
    
    Class_ child_class = *child_class_ptr;
    Symbol current_parent = child_class->get_parent();
    
    while (current_parent != No_class)
    {
        if (current_parent == parent)
        {
            return true;
        }
        
        Class_ *parent_class_ptr = class_table->lookup(current_parent);
        if (parent_class_ptr == NULL) break;
        
        Class_ parent_class = *parent_class_ptr;
        current_parent = parent_class->get_parent();
    }
    
    return false;
}

//////////////////////////////////////////////////////////////////////
// 5. 类型推断和LUB（Least Upper Bound）
//////////////////////////////////////////////////////////////////////

Symbol ClassTable::lub(Symbol type1, Symbol type2)
{
    if (semant_debug) {
        cerr << "计算LUB: " << type1 << " ∨ " << type2 << endl;
    }
    
    // 如果两个类型都是SELF_TYPE，返回SELF_TYPE
    if (type1 == SELF_TYPE && type2 == SELF_TYPE)
    {
        return SELF_TYPE;
    }
    
    // 如果只有一个类型是SELF_TYPE，返回Object
    if (type1 == SELF_TYPE || type2 == SELF_TYPE)
    {
        return Object;
    }
    
    // 如果type1是type2的子类型，返回type2
    if (is_subtype(type1, type2))
    {
        return type2;
    }
    
    // 如果type2是type1的子类型，返回type1
    if (is_subtype(type2, type1))
    {
        return type1;
    }
    
    // 寻找共同祖先
    std::vector<Symbol> type1_ancestors;
    Symbol current = type1;
    while (current != No_class)
    {
        type1_ancestors.push_back(current);
        Class_ *class_ptr = class_table->lookup(current);
        if (class_ptr == NULL) break;
        Class_ c = *class_ptr;
        current = c->get_parent();
    }
    type1_ancestors.push_back(Object);
    
    // 检查type2的祖先
    current = type2;
    while (current != No_class)
    {
        for (Symbol ancestor : type1_ancestors)
        {
            if (current == ancestor)
            {
                return ancestor;
            }
        }
        
        Class_ *class_ptr = class_table->lookup(current);
        if (class_ptr == NULL) break;
        Class_ c = *class_ptr;
        current = c->get_parent();
    }
    
    // 默认返回Object
    return Object;
}

//////////////////////////////////////////////////////////////////////
// 6. 方法查找和重写检查
//////////////////////////////////////////////////////////////////////

method_class* ClassTable::find_method(Symbol class_name, Symbol method_name)
{
    if (semant_debug) {
        cerr << "查找方法: " << class_name << "." << method_name << endl;
    }
    
    Symbol current_class = class_name;
    
    while (current_class != No_class)
    {
        Class_ *class_ptr = class_table->lookup(current_class);
        if (class_ptr == NULL) return NULL;
        
        Class_ c = *class_ptr;
        Features features = c->get_features();
        
        // 在当前类中查找方法
        for(int i = features->first(); features->more(i); i = features->next(i))
        {
            Feature f = features->nth(i);
            method_class *method = dynamic_cast<method_class*>(f);
            if (method != NULL && method->get_name() == method_name)
            {
                if (semant_debug) {
                    cerr << "找到方法: " << method_name << " 在类 " << current_class << endl;
                }
                return method;
            }
        }
        
        // 在父类中继续查找
        current_class = c->get_parent();
    }
    
    if (semant_debug) {
        cerr << "未找到方法: " << method_name << endl;
    }
    return NULL;
}

//////////////////////////////////////////////////////////////////////
// 7. 表达式类型检查（核心实现）
//////////////////////////////////////////////////////////////////////

Symbol ClassTable::type_check_expression(Expression expr, 
                                         Symbol current_class,
                                         SymbolTable<Symbol, Symbol>* object_env,
                                         const char* filename)
{
    if (expr == NULL) return No_type;
    
    if (semant_debug) {
        cerr << "类型检查表达式: " << expr->get_line_number() << endl;
    }
    
    Symbol result_type = No_type;
    
    // 处理不同类型的表达式
    if (dynamic_cast<int_const_class*>(expr) != NULL)
    {
        int_const_class* int_expr = (int_const_class*)expr;
        result_type = Int;
        int_expr->set_type(result_type);
    }
    else if (dynamic_cast<bool_const_class*>(expr) != NULL)
    {
        bool_const_class* bool_expr = (bool_const_class*)expr;
        result_type = Bool;
        bool_expr->set_type(result_type);
    }
    else if (dynamic_cast<string_const_class*>(expr) != NULL)
    {
        string_const_class* string_expr = (string_const_class*)expr;
        result_type = Str;
        string_expr->set_type(result_type);
    }
    else if (dynamic_cast<object_class*>(expr) != NULL)
    {
        object_class* obj_expr = (object_class*)expr;
        Symbol var_name = obj_expr->get_name();
        
        // 查找变量类型
        Symbol *type_ptr = object_env->lookup(var_name);
        if (type_ptr == NULL)
        {
            semant_error(filename, expr) << "Undeclared identifier " << var_name << "." << endl;
            semant_errors++;
            result_type = Object;
        }
        else
        {
            result_type = *type_ptr;
        }
        
        obj_expr->set_type(result_type);
    }
    else if (dynamic_cast<assign_class*>(expr) != NULL)
    {
        assign_class* assign_expr = (assign_class*)expr;
        Symbol var_name = assign_expr->get_name();
        
        // 检查变量是否已声明
        Symbol *var_type_ptr = object_env->lookup(var_name);
        if (var_type_ptr == NULL)
        {
            semant_error(filename, expr) << "Assignment to undeclared variable " << var_name << "." << endl;
            semant_errors++;
            result_type = Object;
        }
        else
        {
            Symbol var_type = *var_type_ptr;
            
            // 检查赋值表达式
            Expression rhs = assign_expr->get_expr();
            Symbol rhs_type = type_check_expression(rhs, current_class, object_env, filename);
            
            // 检查类型兼容性
            if (rhs_type == SELF_TYPE && var_type == SELF_TYPE)
            {
                result_type = rhs_type;
            }
            else if (!is_subtype(rhs_type, var_type))
            {
                semant_error(filename, expr) << "Type " << rhs_type 
                    << " of assigned expression does not conform to declared type " 
                    << var_type << " of identifier " << var_name << "." << endl;
                semant_errors++;
                result_type = var_type;
            }
            else
            {
                result_type = var_type;
            }
        }
        
        assign_expr->set_type(result_type);
    }
    else if (dynamic_cast<dispatch_class*>(expr) != NULL)
    {
        // 动态分派表达式
        dispatch_class* dispatch_expr = (dispatch_class*)expr;
        
        // 检查被调用的表达式
        Expression expr_obj = dispatch_expr->get_expr();
        Symbol expr_type = type_check_expression(expr_obj, current_class, object_env, filename);
        Symbol original_expr_type = expr_type;
        
        if (semant_debug) {
            cerr << "动态分派: 表达式类型 = " << expr_type << ", 原始类型 = " << original_expr_type << endl;
        }
        
        // 解析SELF_TYPE用于方法查找
        if (expr_type == SELF_TYPE)
        {
            expr_type = current_class;
        }
        
        // 查找方法
        method_class* method = find_method(expr_type, dispatch_expr->get_name());
        if (method == NULL)
        {
            semant_error(filename, expr) << "Dispatch to undefined method " 
                << dispatch_expr->get_name() << "." << endl;
            semant_errors++;
            result_type = Object;
        }
        else
        {
            // 检查参数
            Expressions actuals = dispatch_expr->get_actuals();
            Formals formals = method->get_formals();
            
            int actual_count = 0;
            for(int i = actuals->first(); actuals->more(i); i = actuals->next(i))
            {
                actual_count++;
            }
            
            int formal_count = 0;
            for(int i = formals->first(); formals->more(i); i = formals->next(i))
            {
                formal_count++;
            }
            
            if (actual_count != formal_count)
            {
                semant_error(filename, expr) << "Method " << dispatch_expr->get_name() 
                    << " called with wrong number of arguments." << endl;
                semant_errors++;
            }
            else
            {
                // 检查每个参数的类型
                int param_index = 0;
                for(int i = actuals->first(), j = formals->first(); 
                    actuals->more(i) && formals->more(j); 
                    i = actuals->next(i), j = formals->next(j))
                {
                    Expression actual = actuals->nth(i);
                    Formal formal = formals->nth(j);
                    
                    Symbol actual_type = type_check_expression(actual, current_class, object_env, filename);
                    Symbol formal_type = formal->get_type();
                    
                    if (!is_subtype(actual_type, formal_type))
                    {
                        semant_error(filename, expr) << "In call of method " << dispatch_expr->get_name() 
                            << ", type " << actual_type << " of parameter " << param_index 
                            << " does not conform to declared type " << formal_type << "." << endl;
                        semant_errors++;
                    }
                    
                    param_index++;
                }
            }
            
            // 设置返回类型（关键：SELF_TYPE处理）
            result_type = method->get_return_type();
            if (result_type == SELF_TYPE)
            {
                if (original_expr_type == SELF_TYPE)
                {
                    result_type = SELF_TYPE;
                }
                else
                {
                    result_type = original_expr_type;
                }
            }
            
            if (semant_debug) {
                cerr << "动态分派返回类型: " << result_type << endl;
            }
        }
        
        dispatch_expr->set_type(result_type);
    }
    else if (dynamic_cast<static_dispatch_class*>(expr) != NULL)
    {
        // 静态分派表达式
        static_dispatch_class* static_dispatch_expr = (static_dispatch_class*)expr;
        
        // 检查类型名称
        Symbol static_type = static_dispatch_expr->get_type_name();
        
        // 检查表达式
        Expression expr_obj = static_dispatch_expr->get_expr();
        Symbol expr_type = type_check_expression(expr_obj, current_class, object_env, filename);
        
        // 检查类型兼容性
        if (!is_subtype(expr_type, static_type))
        {
            semant_error(filename, expr) << "Expression type " << expr_type 
                << " does not conform to declared static dispatch type " << static_type << "." << endl;
            semant_errors++;
        }
        
        // 查找方法
        method_class* method = find_method(static_type, static_dispatch_expr->get_name());
        if (method == NULL)
        {
            semant_error(filename, expr) << "Dispatch to undefined method " 
                << static_dispatch_expr->get_name() << "." << endl;
            semant_errors++;
            result_type = Object;
        }
        else
        {
            // 检查参数
            Expressions actuals = static_dispatch_expr->get_actuals();
            Formals formals = method->get_formals();
            
            int actual_count = 0;
            for(int i = actuals->first(); actuals->more(i); i = actuals->next(i))
            {
                actual_count++;
            }
            
            int formal_count = 0;
            for(int i = formals->first(); formals->more(i); i = formals->next(i))
            {
                formal_count++;
            }
            
            if (actual_count != formal_count)
            {
                semant_error(filename, expr) << "Method " << static_dispatch_expr->get_name() 
                    << " called with wrong number of arguments." << endl;
                semant_errors++;
            }
            else
            {
                // 检查每个参数的类型
                int param_index = 0;
                for(int i = actuals->first(), j = formals->first(); 
                    actuals->more(i) && formals->more(j); 
                    i = actuals->next(i), j = formals->next(j))
                {
                    Expression actual = actuals->nth(i);
                    Formal formal = formals->nth(j);
                    
                    Symbol actual_type = type_check_expression(actual, current_class, object_env, filename);
                    Symbol formal_type = formal->get_type();
                    
                    if (!is_subtype(actual_type, formal_type))
                    {
                        semant_error(filename, expr) << "In call of method " << static_dispatch_expr->get_name() 
                            << ", type " << actual_type << " of parameter " << param_index 
                            << " does not conform to declared type " << formal_type << "." << endl;
                        semant_errors++;
                    }
                    
                    param_index++;
                }
            }
            
            // 设置返回类型
            result_type = method->get_return_type();
            if (result_type == SELF_TYPE)
            {
                result_type = static_type;
            }
        }
        
        static_dispatch_expr->set_type(result_type);
    }
    else if (dynamic_cast<cond_class*>(expr) != NULL)
    {
        // 条件表达式
        cond_class* cond_expr = (cond_class*)expr;
        
        // 检查条件表达式
        Expression pred = cond_expr->get_pred();
        Symbol pred_type = type_check_expression(pred, current_class, object_env, filename);
        
        if (pred_type != Bool)
        {
            semant_error(filename, expr) << "Predicate of 'if' does not have type Bool." << endl;
            semant_errors++;
        }
        
        // 检查then和else分支
        Expression then_exp = cond_expr->get_then_exp();
        Expression else_exp = cond_expr->get_else_exp();
        
        Symbol then_type = type_check_expression(then_exp, current_class, object_env, filename);
        Symbol else_type = type_check_expression(else_exp, current_class, object_env, filename);
        
        // 计算最小上界作为条件表达式的类型
        result_type = lub(then_type, else_type);
        
        cond_expr->set_type(result_type);
    }
    else if (dynamic_cast<loop_class*>(expr) != NULL)
    {
        // 循环表达式
        loop_class* loop_expr = (loop_class*)expr;
        
        // 检查条件表达式
        Expression pred = loop_expr->get_pred();
        Symbol pred_type = type_check_expression(pred, current_class, object_env, filename);
        
        if (pred_type != Bool)
        {
            semant_error(filename, expr) << "Loop condition does not have type Bool." << endl;
            semant_errors++;
        }
        
        // 检查循环体
        Expression body = loop_expr->get_body();
        type_check_expression(body, current_class, object_env, filename);
        
        // while循环的类型总是Object
        result_type = Object;
        loop_expr->set_type(result_type);
    }
    else if (dynamic_cast<block_class*>(expr) != NULL)
    {
        // 块表达式
        block_class* block_expr = (block_class*)expr;
        
        // 块表达式的类型是最后一个表达式的类型
        Expressions body = block_expr->get_body();
        
        // 遍历所有表达式
        for(int i = body->first(); body->more(i); i = body->next(i))
        {
            Expression e = body->nth(i);
            result_type = type_check_expression(e, current_class, object_env, filename);
        }
        
        block_expr->set_type(result_type);
    }
    else if (dynamic_cast<let_class*>(expr) != NULL)
    {
        // let表达式
        let_class* let_expr = (let_class*)expr;
        
        // 进入新的作用域
        object_env->enterscope();
        
        // 处理标识符
        Symbol identifier = let_expr->get_identifier();
        Symbol type_decl = let_expr->get_type_decl();
        
        // 检查类型声明是否存在
        if (type_decl != SELF_TYPE && class_table->lookup(type_decl) == NULL)
        {
            semant_error(filename, expr) << "Class " << type_decl << " of let-bound identifier " 
                << identifier << " is undefined." << endl;
            semant_errors++;
            type_decl = Object;
        }
        
        // 处理初始化表达式
        Expression init = let_expr->get_init();
        if (init->get_type() == NULL)  // 检查是否为空表达式
        {
            // 没有初始化表达式
            object_env->addid(identifier, new Symbol(type_decl));
        }
        else
        {
            // 有初始化表达式，检查类型
            Symbol init_type = type_check_expression(init, current_class, object_env, filename);
            
            if (!is_subtype(init_type, type_decl))
            {
                semant_error(filename, expr) << "Inferred type " << init_type 
                    << " of initialization of " << identifier 
                    << " does not conform to identifier's declared type " << type_decl << "." << endl;
                semant_errors++;
            }
            
            object_env->addid(identifier, new Symbol(type_decl));
        }
        
        // 处理主体表达式
        Expression body = let_expr->get_body();
        result_type = type_check_expression(body, current_class, object_env, filename);
        
        // 退出作用域
        object_env->exitscope();
        
        let_expr->set_type(result_type);
    }
    else if (dynamic_cast<plus_class*>(expr) != NULL)
    {
        // 加法表达式
        plus_class* plus_expr = (plus_class*)expr;
        
        // 检查左操作数
        Expression e1 = plus_expr->get_e1();
        Symbol type1 = type_check_expression(e1, current_class, object_env, filename);
        
        // 检查右操作数
        Expression e2 = plus_expr->get_e2();
        Symbol type2 = type_check_expression(e2, current_class, object_env, filename);
        
        // 两个操作数都必须是Int类型
        if (type1 != Int || type2 != Int)
        {
            semant_error(filename, expr) << "non-Int arguments: " << type1 << " + " << type2 << endl;
            semant_errors++;
        }
        
        result_type = Int;
        plus_expr->set_type(result_type);
    }
    else if (dynamic_cast<eq_class*>(expr) != NULL)
    {
        // 相等比较表达式
        eq_class* eq_expr = (eq_class*)expr;
        
        // 检查左操作数
        Expression e1 = eq_expr->get_e1();
        Symbol type1 = type_check_expression(e1, current_class, object_env, filename);
        
        // 检查右操作数
        Expression e2 = eq_expr->get_e2();
        Symbol type2 = type_check_expression(e2, current_class, object_env, filename);
        
        // 比较操作可以比较任何类型，但Int、String、Bool只能与相同类型比较
        if ((type1 == Int || type1 == String || type1 == Bool) && type1 != type2)
        {
            semant_error(filename, expr) << "Illegal comparison with a basic type." << endl;
            semant_errors++;
        }
        
        result_type = Bool;
        eq_expr->set_type(result_type);
    }
    else if (dynamic_cast<new__class*>(expr) != NULL)
    {
        // new表达式
        new__class* new_expr = (new__class*)expr;
        Symbol type_name = new_expr->get_type_name();
        
        // 检查类型是否存在
        if (type_name != SELF_TYPE && class_table->lookup(type_name) == NULL)
        {
            semant_error(filename, expr) << "'new' used with undefined class " << type_name << "." << endl;
            semant_errors++;
            result_type = Object;
        }
        else
        {
            result_type = type_name;
        }
        
        new_expr->set_type(result_type);
    }
    else if (dynamic_cast<isvoid_class*>(expr) != NULL)
    {
        // isvoid表达式
        isvoid_class* isvoid_expr = (isvoid_class*)expr;
        
        // 检查表达式
        Expression e1 = isvoid_expr->get_e1();
        type_check_expression(e1, current_class, object_env, filename);
        
        result_type = Bool;
        isvoid_expr->set_type(result_type);
    }
    
    // 设置表达式行号（用于输出格式）
    if (semant_debug) {
        cerr << "表达式 #" << expr->get_line_number() << " 类型: " << result_type << endl;
    }
    
    return result_type;
}

//////////////////////////////////////////////////////////////////////
// 类类型检查
//////////////////////////////////////////////////////////////////////

void ClassTable::type_check_class(Class_ c)
{
    if (c == NULL) return;
    
    Symbol class_name = c->get_name();
    Symbol parent = c->get_parent();
    Features features = c->get_features();
    const char* filename = c->get_filename()->get_string();
    
    if (semant_debug) {
        cerr << "类型检查类: " << class_name << endl;
    }
    
    // 创建对象环境（用于变量类型）
    SymbolTable<Symbol, Symbol>* object_env = new SymbolTable<Symbol, Symbol>();
    object_env->enterscope();
    
    // 添加self变量（类型为SELF_TYPE）
    object_env->addid(self, new Symbol(SELF_TYPE));
    
    // 遍历所有特性
    for(int i = features->first(); features->more(i); i = features->next(i))
    {
        Feature f = features->nth(i);
        
        if (dynamic_cast<attr_class*>(f) != NULL)
        {
            // 属性检查
            attr_class* attr = (attr_class*)f;
            Symbol attr_name = attr->get_name();
            Symbol attr_type = attr->get_type();
            
            if (semant_debug) {
                cerr << "检查属性: " << attr_name << " : " << attr_type << endl;
            }
            
            // 检查属性类型是否存在
            if (attr_type != SELF_TYPE && class_table->lookup(attr_type) == NULL)
            {
                semant_error(c) << "Class " << attr_type << " of attribute " << attr_name << " is undefined." << endl;
                semant_errors++;
                attr_type = Object;
            }
            
            // 检查初始化表达式
            Expression init = attr->get_init();
            if (init->get_type() != NULL)  // 如果有初始化表达式
            {
                Symbol init_type = type_check_expression(init, class_name, object_env, filename);
                
                if (!is_subtype(init_type, attr_type))
                {
                    semant_error(c) << "Inferred type " << init_type 
                        << " of initialization of attribute " << attr_name 
                        << " does not conform to declared type " << attr_type << "." << endl;
                    semant_errors++;
                }
            }
            
            // 添加属性到对象环境
            object_env->addid(attr_name, new Symbol(attr_type));
        }
        else if (dynamic_cast<method_class*>(f) != NULL)
        {
            // 方法检查
            method_class* method = (method_class*)f;
            Symbol method_name = method->get_name();
            Symbol return_type = method->get_return_type();
            Formals formals = method->get_formals();
            Expression expr = method->get_expr();
            
            if (semant_debug) {
                cerr << "检查方法: " << method_name << " : " << return_type << endl;
            }
            
            // 检查返回类型
            if (return_type != SELF_TYPE && class_table->lookup(return_type) == NULL)
            {
                semant_error(c) << "Undefined return type " << return_type 
                    << " in method " << method_name << "." << endl;
                semant_errors++;
                return_type = Object;
            }
            
            // 进入新的作用域
            object_env->enterscope();
            
            // 添加参数到环境
            for(int j = formals->first(); formals->more(j); j = formals->next(j))
            {
                Formal formal = formals->nth(j);
                Symbol formal_name = formal->get_name();
                Symbol formal_type = formal->get_type();
                
                // 检查参数类型
                if (formal_type != SELF_TYPE && class_table->lookup(formal_type) == NULL)
                {
                    semant_error(c) << "Class " << formal_type << " of formal parameter " 
                        << formal_name << " is undefined." << endl;
                    semant_errors++;
                    formal_type = Object;
                }
                
                // 检查参数名是否重复
                Symbol *existing_type = object_env->probe(formal_name);
                if (existing_type != NULL)
                {
                    semant_error(c) << "Formal parameter " << formal_name << " is multiply defined." << endl;
                    semant_errors++;
                }
                else
                {
                    object_env->addid(formal_name, new Symbol(formal_type));
                }
            }
            
            // 检查方法体
            Symbol expr_type = type_check_expression(expr, class_name, object_env, filename);
            
            // 检查返回类型
            if (return_type == SELF_TYPE)
            {
                if (expr_type != SELF_TYPE)
                {
                    semant_error(c) << "Inferred return type " << expr_type 
                        << " of method " << method_name 
                        << " does not conform to declared return type SELF_TYPE." << endl;
                    semant_errors++;
                }
            }
            else if (!is_subtype(expr_type, return_type))
            {
                semant_error(c) << "Inferred return type " << expr_type 
                    << " of method " << method_name 
                    << " does not conform to declared return type " << return_type << "." << endl;
                semant_errors++;
            }
            
            // 退出作用域
            object_env->exitscope();
            
            // 检查方法重写
            if (parent != No_class)
            {
                method_class* parent_method = find_method(parent, method_name);
                if (parent_method != NULL)
                {
                    // 检查参数数量
                    Formals parent_formals = parent_method->get_formals();
                    int parent_param_count = 0;
                    int child_param_count = 0;
                    
                    for(int j = parent_formals->first(); parent_formals->more(j); j = parent_formals->next(j))
                        parent_param_count++;
                    
                    for(int j = formals->first(); formals->more(j); j = formals->next(j))
                        child_param_count++;
                    
                    if (parent_param_count != child_param_count)
                    {
                        semant_error(c) << "In redefined method " << method_name 
                            << ", parameter number differs from original." << endl;
                        semant_errors++;
                    }
                    else
                    {
                        // 检查参数类型
                        for(int j = formals->first(), k = parent_formals->first(); 
                            formals->more(j) && parent_formals->more(k); 
                            j = formals->next(j), k = parent_formals->next(k))
                        {
                            Formal child_formal = formals->nth(j);
                            Formal parent_formal = parent_formals->nth(k);
                            
                            Symbol child_formal_type = child_formal->get_type();
                            Symbol parent_formal_type = parent_formal->get_type();
                            
                            if (child_formal_type != parent_formal_type)
                            {
                                semant_error(c) << "In redefined method " << method_name 
                                    << ", parameter type " << child_formal_type 
                                    << " differs from original type " << parent_formal_type << "." << endl;
                                semant_errors++;
                            }
                        }
                    }
                    
                    // 检查返回类型
                    Symbol parent_return_type = parent_method->get_return_type();
                    if (return_type != parent_return_type)
                    {
                        semant_error(c) << "In redefined method " << method_name 
                            << ", return type " << return_type 
                            << " differs from original return type " << parent_return_type << "." << endl;
                        semant_errors++;
                    }
                }
            }
        }
    }
    
    // 退出对象环境
    object_env->exitscope();
    delete object_env;
}

//////////////////////////////////////////////////////////////////////
// 整体类型检查
//////////////////////////////////////////////////////////////////////

void ClassTable::type_check()
{
    if (semant_debug) {
        cerr << "开始类型检查" << endl;
    }
    
    // 遍历所有类进行类型检查
    for (ClassTable::iterator it = begin(); it != end(); ++it)
    {
        Symbol class_name = it->first;
        Class_ *class_ptr = it->second;
        
        if (class_ptr != NULL)
        {
            Class_ c = *class_ptr;
            type_check_class(c);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// 输出格式：类型标注的AST
//////////////////////////////////////////////////////////////////////

// 语义分析器入口函数
void program_class::semant()
{
    initialize_constants();
    
    if (semant_debug) {
        cerr << "=== 开始语义分析 ===" << endl;
    }
    
    // 创建类表并进行语义分析
    ClassTable *classtable = new ClassTable(classes);
    
    // 如果有错误，直接返回
    if (classtable->errors()) {
        cerr << "Compilation halted due to static semantic errors." << endl;
        exit(1);
    }
    
    // 进行类型检查
    classtable->type_check();
    
    // 如果还有错误，退出
    if (classtable->errors()) {
        cerr << "Compilation halted due to static semantic errors." << endl;
        exit(1);
    }
    
    if (semant_debug) {
        cerr << "=== 语义分析完成 ===" << endl;
    }
}
