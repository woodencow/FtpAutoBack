#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
函数比较脚本
用于提取main_sysmod.c和main_sysmod-old.c中的函数并进行对比
"""

import re
import os
from difflib import unified_diff

def extract_functions(file_path):
    """
    从C文件中提取函数定义
    返回函数名和函数内容的字典
    """
    functions = {}
    
    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            content = file.read()
    except UnicodeDecodeError:
        # 如果UTF-8解码失败，尝试使用其他编码
        try:
            with open(file_path, 'r', encoding='latin-1') as file:
                content = file.read()
        except Exception as e:
            print(f"无法读取文件 {file_path}: {e}")
            return functions
    
    # 预处理：移除注释
    content = remove_comments(content)
    
    # 匹配函数定义的正则表达式
    # 支持多种函数定义格式，包括static、inline等修饰符
    function_pattern = r'^((?:static\s+|inline\s+|Result\s+|void\s+|int\s+|bool\s+|char\s+\*?\s+|u\d+\s+|s\d+\s+|size_t\s+|uint\d+_t\s+|int\d+_t\s+)?[a-zA-Z_][a-zA-Z0-9_]*\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\([^)]*\)\s*\{)'
    
    lines = content.split('\n')
    current_function = None
    current_content = []
    brace_count = 0
    in_function = False
    
    for line_num, line in enumerate(lines, 1):
        # 检查是否是函数定义开始
        if not in_function and re.match(function_pattern, line):
            # 提取函数名
            func_match = re.search(r'([a-zA-Z_][a-zA-Z0-9_]*)\s*\(', line)
            if func_match:
                current_function = func_match.group(1)
                current_content = [line]
                brace_count = line.count('{') - line.count('}')
                in_function = brace_count > 0
                continue
        
        # 如果在函数内部，继续收集内容
        if in_function and current_function:
            current_content.append(line)
            brace_count += line.count('{') - line.count('}')
            
            # 当大括号平衡时，函数结束
            if brace_count == 0:
                functions[current_function] = '\n'.join(current_content)
                current_function = None
                current_content = []
                in_function = False
    
    return functions

def remove_comments(content):
    """
    移除C语言中的注释，包括单行注释和多行注释
    """
    # 移除多行注释 /* ... */
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    
    # 移除单行注释 // ...
    content = re.sub(r'//.*$', '', content, flags=re.MULTILINE)
    
    return content

def compare_functions_detailed(funcs1, funcs2, file1_name, file2_name):
    """
    更详细地比较两个函数字典，输出每行每个变量的差异
    """
    all_func_names = set(funcs1.keys()).union(set(funcs2.keys()))
    
    print(f"\n{'='*80}")
    print(f"详细函数比较结果: {file1_name} vs {file2_name}")
    print(f"{'='*80}")
    
    # 找出仅在第一个文件中存在的函数
    only_in_file1 = set(funcs1.keys()) - set(funcs2.keys())
    if only_in_file1:
        print(f"\n仅在 {file1_name} 中存在的函数:")
        for func in sorted(only_in_file1):
            print(f"  - {func}")
    
    # 找出仅在第二个文件中存在的函数
    only_in_file2 = set(funcs2.keys()) - set(funcs1.keys())
    if only_in_file2:
        print(f"\n仅在 {file2_name} 中存在的函数:")
        for func in sorted(only_in_file2):
            print(f"  - {func}")
    
    # 比较两个文件中都存在的函数
    common_funcs = set(funcs1.keys()) & set(funcs2.keys())
    different_funcs = []
    
    for func in sorted(common_funcs):
        if funcs1[func] != funcs2[func]:
            different_funcs.append(func)
    
    if different_funcs:
        print(f"\n两个文件中都存在但实现不同的函数:")
        for func in different_funcs:
            print(f"\n{'='*60}")
            print(f"函数: {func}")
            print(f"{'='*60}")
            
            # 获取函数内容
            content1 = funcs1[func]
            content2 = funcs2[func]
            
            # 分割为行
            lines1 = content1.split('\n')
            lines2 = content2.split('\n')
            
            print(f"在 {file1_name} 中有 {len(lines1)} 行")
            print(f"在 {file2_name} 中有 {len(lines2)} 行")
            print(f"行数差异: {len(lines1) - len(lines2):+d}")
            
            # 使用difflib生成差异
            diff = unified_diff(
                lines1,
                lines2,
                fromfile=f"{file1_name}:{func}",
                tofile=f"{file2_name}:{func}",
                lineterm=""
            )
            
            # 打印完整差异
            print("\n详细行级差异:")
            diff_lines = list(diff)
            for line in diff_lines:
                print(line)
            
            # 分析变量差异
            print("\n变量差异分析:")
            analyze_variable_differences(content1, content2, file1_name, file2_name)
            
            # 分析函数调用差异
            print("\n函数调用差异分析:")
            analyze_function_calls(content1, content2, file1_name, file2_name)
    
    # 汇总统计
    print(f"\n{'='*80}")
    print("汇总统计:")
    print(f"  {file1_name} 函数总数: {len(funcs1)}")
    print(f"  {file2_name} 函数总数: {len(funcs2)}")
    print(f"  仅在 {file1_name} 中存在的函数: {len(only_in_file1)}")
    print(f"  仅在 {file2_name} 中存在的函数: {len(only_in_file2)}")
    print(f"  两个文件中都存在的函数: {len(common_funcs)}")
    print(f"  实现不同的函数: {len(different_funcs)}")
    
    # 如果有差异函数，列出它们
    if different_funcs:
        print(f"\n差异函数列表:")
        for func in sorted(different_funcs):
            lines1 = funcs1[func].split('\n')
            lines2 = funcs2[func].split('\n')
            line_count_diff = len(lines1) - len(lines2)
            print(f"  - {func} (行数差异: {line_count_diff:+d})")
    
    print(f"{'='*80}")

def analyze_variable_differences(content1, content2, file1_name, file2_name):
    """
    分析两个函数内容中的变量差异
    """
    # 提取变量声明
    var_pattern = r'\b(?:int|char|float|double|bool|u\d+|s\d+|size_t|uint\d+_t|int\d+_t|void\s*\*|Result|CURL\*|FsFile|AccountUid|time_t|struct\s+\w+)\s+(\*?\s*[a-zA-Z_][a-zA-Z0-9_]*(?:\s*\[[^\]]*\])?)\s*(?:=|;|,)'
    
    vars1 = re.findall(var_pattern, content1)
    vars2 = re.findall(var_pattern, content2)
    
    # 提取变量名
    var_names1 = set()
    for var in vars1:
        # 提取变量名部分
        var_name_match = re.search(r'\*?\s*([a-zA-Z_][a-zA-Z0-9_]*(?:\s*\[[^\]]*\])?)', var)
        if var_name_match:
            var_names1.add(var_name_match.group(1).strip())
    
    var_names2 = set()
    for var in vars2:
        # 提取变量名部分
        var_name_match = re.search(r'\*?\s*([a-zA-Z_][a-zA-Z0-9_]*(?:\s*\[[^\]]*\])?)', var)
        if var_name_match:
            var_names2.add(var_name_match.group(1).strip())
    
    # 找出仅在第一个文件中存在的变量
    only_in_file1 = var_names1 - var_names2
    if only_in_file1:
        print(f"  仅在 {file1_name} 中存在的变量:")
        for var in sorted(only_in_file1):
            print(f"    - {var}")
    
    # 找出仅在第二个文件中存在的变量
    only_in_file2 = var_names2 - var_names1
    if only_in_file2:
        print(f"  仅在 {file2_name} 中存在的变量:")
        for var in sorted(only_in_file2):
            print(f"    - {var}")
    
    # 找出共同的变量
    common_vars = var_names1 & var_names2
    if common_vars:
        print(f"  两个文件中都存在的变量:")
        for var in sorted(common_vars):
            print(f"    - {var}")

def analyze_function_calls(content1, content2, file1_name, file2_name):
    """
    分析两个函数内容中的函数调用差异
    """
    # 提取函数调用
    func_call_pattern = r'\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\('
    
    # 排除C语言关键字和类型
    excluded = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'int', 'char', 'void', 'static', 'const'}
    
    calls1 = set(re.findall(func_call_pattern, content1)) - excluded
    calls2 = set(re.findall(func_call_pattern, content2)) - excluded
    
    # 找出仅在第一个文件中存在的函数调用
    only_in_file1 = calls1 - calls2
    if only_in_file1:
        print(f"  仅在 {file1_name} 中存在的函数调用:")
        for call in sorted(only_in_file1):
            print(f"    - {call}()")
    
    # 找出仅在第二个文件中存在的函数调用
    only_in_file2 = calls2 - calls1
    if only_in_file2:
        print(f"  仅在 {file2_name} 中存在的函数调用:")
        for call in sorted(only_in_file2):
            print(f"    - {call}()")
    
    # 找出共同的函数调用
    common_calls = calls1 & calls2
    if common_calls:
        print(f"  两个文件中都存在的函数调用:")
        for call in sorted(common_calls):
            print(f"    - {call}()")

def save_results_to_file(funcs1, funcs2, file1_name, file2_name, output_file="function_comparison_results.txt"):
    """
    将比较结果保存到文件
    """
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(f"详细函数比较结果: {file1_name} vs {file2_name}\n")
        f.write(f"{'='*80}\n\n")
        
        # 找出仅在第一个文件中存在的函数
        only_in_file1 = set(funcs1.keys()) - set(funcs2.keys())
        if only_in_file1:
            f.write(f"仅在 {file1_name} 中存在的函数:\n")
            for func in sorted(only_in_file1):
                f.write(f"  - {func}\n")
            f.write("\n")
        
        # 找出仅在第二个文件中存在的函数
        only_in_file2 = set(funcs2.keys()) - set(funcs1.keys())
        if only_in_file2:
            f.write(f"仅在 {file2_name} 中存在的函数:\n")
            for func in sorted(only_in_file2):
                f.write(f"  - {func}\n")
            f.write("\n")
        
        # 比较两个文件中都存在的函数
        common_funcs = set(funcs1.keys()) & set(funcs2.keys())
        different_funcs = []
        
        for func in sorted(common_funcs):
            if funcs1[func] != funcs2[func]:
                different_funcs.append(func)
        
        if different_funcs:
            f.write("两个文件中都存在但实现不同的函数:\n")
            for func in different_funcs:
                f.write(f"\n{'='*60}\n")
                f.write(f"函数: {func}\n")
                f.write(f"{'='*60}\n\n")
                
                # 获取函数内容
                content1 = funcs1[func]
                content2 = funcs2[func]
                
                # 分割为行
                lines1 = content1.split('\n')
                lines2 = content2.split('\n')
                
                f.write(f"在 {file1_name} 中有 {len(lines1)} 行\n")
                f.write(f"在 {file2_name} 中有 {len(lines2)} 行\n")
                f.write(f"行数差异: {len(lines1) - len(lines2):+d}\n\n")
                
                # 使用difflib生成差异
                diff = unified_diff(
                    lines1,
                    lines2,
                    fromfile=f"{file1_name}:{func}",
                    tofile=f"{file2_name}:{func}",
                    lineterm=""
                )
                
                # 写入完整差异
                f.write("详细行级差异:\n")
                diff_lines = list(diff)
                for line in diff_lines:
                    f.write(f"{line}\n")
                
                # 分析变量差异
                f.write("\n变量差异分析:\n")
                save_variable_differences_to_file(f, content1, content2, file1_name, file2_name)
                
                # 分析函数调用差异
                f.write("\n函数调用差异分析:\n")
                save_function_calls_to_file(f, content1, content2, file1_name, file2_name)
                f.write("\n")
        
        # 汇总统计
        f.write(f"\n{'='*80}\n")
        f.write("汇总统计:\n")
        f.write(f"  {file1_name} 函数总数: {len(funcs1)}\n")
        f.write(f"  {file2_name} 函数总数: {len(funcs2)}\n")
        f.write(f"  仅在 {file1_name} 中存在的函数: {len(only_in_file1)}\n")
        f.write(f"  仅在 {file2_name} 中存在的函数: {len(only_in_file2)}\n")
        f.write(f"  两个文件中都存在的函数: {len(common_funcs)}\n")
        f.write(f"  实现不同的函数: {len(different_funcs)}\n")
        
        # 如果有差异函数，列出它们
        if different_funcs:
            f.write(f"\n差异函数列表:\n")
            for func in sorted(different_funcs):
                lines1 = funcs1[func].split('\n')
                lines2 = funcs2[func].split('\n')
                line_count_diff = len(lines1) - len(lines2)
                f.write(f"  - {func} (行数差异: {line_count_diff:+d})\n")
        
        f.write(f"{'='*80}\n")

def save_variable_differences_to_file(f, content1, content2, file1_name, file2_name):
    """
    将变量差异分析保存到文件
    """
    # 提取变量声明
    var_pattern = r'\b(?:int|char|float|double|bool|u\d+|s\d+|size_t|uint\d+_t|int\d+_t|void\s*\*|Result|CURL\*|FsFile|AccountUid|time_t|struct\s+\w+)\s+(\*?\s*[a-zA-Z_][a-zA-Z0-9_]*(?:\s*\[[^\]]*\])?)\s*(?:=|;|,)'
    
    vars1 = re.findall(var_pattern, content1)
    vars2 = re.findall(var_pattern, content2)
    
    # 提取变量名
    var_names1 = set()
    for var in vars1:
        # 提取变量名部分
        var_name_match = re.search(r'\*?\s*([a-zA-Z_][a-zA-Z0-9_]*(?:\s*\[[^\]]*\])?)', var)
        if var_name_match:
            var_names1.add(var_name_match.group(1).strip())
    
    var_names2 = set()
    for var in vars2:
        # 提取变量名部分
        var_name_match = re.search(r'\*?\s*([a-zA-Z_][a-zA-Z0-9_]*(?:\s*\[[^\]]*\])?)', var)
        if var_name_match:
            var_names2.add(var_name_match.group(1).strip())
    
    # 找出仅在第一个文件中存在的变量
    only_in_file1 = var_names1 - var_names2
    if only_in_file1:
        f.write(f"  仅在 {file1_name} 中存在的变量:\n")
        for var in sorted(only_in_file1):
            f.write(f"    - {var}\n")
    
    # 找出仅在第二个文件中存在的变量
    only_in_file2 = var_names2 - var_names1
    if only_in_file2:
        f.write(f"  仅在 {file2_name} 中存在的变量:\n")
        for var in sorted(only_in_file2):
            f.write(f"    - {var}\n")
    
    # 找出共同的变量
    common_vars = var_names1 & var_names2
    if common_vars:
        f.write(f"  两个文件中都存在的变量:\n")
        for var in sorted(common_vars):
            f.write(f"    - {var}\n")

def save_function_calls_to_file(f, content1, content2, file1_name, file2_name):
    """
    将函数调用差异分析保存到文件
    """
    # 提取函数调用
    func_call_pattern = r'\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\('
    
    # 排除C语言关键字和类型
    excluded = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'int', 'char', 'void', 'static', 'const'}
    
    calls1 = set(re.findall(func_call_pattern, content1)) - excluded
    calls2 = set(re.findall(func_call_pattern, content2)) - excluded
    
    # 找出仅在第一个文件中存在的函数调用
    only_in_file1 = calls1 - calls2
    if only_in_file1:
        f.write(f"  仅在 {file1_name} 中存在的函数调用:\n")
        for call in sorted(only_in_file1):
            f.write(f"    - {call}()\n")
    
    # 找出仅在第二个文件中存在的函数调用
    only_in_file2 = calls2 - calls1
    if only_in_file2:
        f.write(f"  仅在 {file2_name} 中存在的函数调用:\n")
        for call in sorted(only_in_file2):
            f.write(f"    - {call}()\n")
    
    # 找出共同的函数调用
    common_calls = calls1 & calls2
    if common_calls:
        f.write(f"  两个文件中都存在的函数调用:\n")
        for call in sorted(common_calls):
            f.write(f"    - {call}()\n")

def main():
    # 文件路径
    file1_path = "src/platform/nx/main_sysmod.c"
    file2_path = "main_sysmod-old.c"
    
    # 检查文件是否存在
    if not os.path.exists(file1_path):
        print(f"错误: 文件 {file1_path} 不存在")
        return
    
    if not os.path.exists(file2_path):
        print(f"错误: 文件 {file2_path} 不存在")
        return
    
    print("正在提取函数...")
    
    # 提取函数
    funcs1 = extract_functions(file1_path)
    funcs2 = extract_functions(file2_path)
    
    print(f"从 {file1_path} 中提取了 {len(funcs1)} 个函数")
    print(f"从 {file2_path} 中提取了 {len(funcs2)} 个函数")
    
    # 比较函数
    compare_functions_detailed(funcs1, funcs2, file1_path, file2_path)
    
    # 保存结果到文件
    save_results_to_file(funcs1, funcs2, file1_path, file2_path)

if __name__ == "__main__":
    main()