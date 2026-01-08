#!/usr/bin/env python3
"""
从输入的多个文件生成一个镜像文件

文件格式：
- 第1个字节：文件总数（0-255）
- 对于每个文件：
  - 8字节：文件大小（小端序）
  - 256字节：文件名（以null结尾的字符串，不足部分填充0）
  - N字节：文件内容
"""

import sys
import os
import struct


def create_image_from_files(input_files, output_file):
    """
    从多个输入文件创建一个镜像文件
    
    Args:
        input_files: 输入文件路径列表
        output_file: 输出镜像文件路径
    """
    # 检查文件数量
    num_files = len(input_files)
    if num_files > 255:
        print(f"错误：文件数量 {num_files} 超过最大限制 255", file=sys.stderr)
        sys.exit(1)
    
    if num_files == 0:
        print("错误：没有输入文件", file=sys.stderr)
        sys.exit(1)
    
    # 检查所有输入文件是否存在
    for input_file in input_files:
        if not os.path.isfile(input_file):
            print(f"错误：文件不存在: {input_file}", file=sys.stderr)
            sys.exit(1)
    
    # 创建镜像文件
    try:
        with open(output_file, 'wb') as out_f:
            # 写入文件头：文件总数（1字节）
            out_f.write(struct.pack('B', num_files))
            print(f"文件总数: {num_files}")
            
            # 处理每个输入文件
            for idx, input_file in enumerate(input_files, 1):
                # 读取文件内容
                with open(input_file, 'rb') as in_f:
                    file_data = in_f.read()
                
                file_size = len(file_data)
                
                # 获取文件名（只保留basename）
                filename = os.path.basename(input_file)
                
                # 写入文件大小（8字节，小端序）
                out_f.write(struct.pack('<Q', file_size))
                
                # 写入文件名（256字节，以null结尾，不足部分填充0）
                filename_bytes = filename.encode('utf-8')
                if len(filename_bytes) >= 256:
                    # 文件名太长，截断并确保最后一个字节为0
                    filename_bytes = filename_bytes[:255]
                # 创建256字节的数组，用0填充
                filename_array = filename_bytes + b'\x00' * (256 - len(filename_bytes))
                out_f.write(filename_array)
                
                # 写入文件内容
                out_f.write(file_data)
                
                print(f"  文件 {idx}: {filename} ({file_size} 字节)")
        
        output_size = os.path.getsize(output_file)
        print(f"\n成功创建镜像文件: {output_file}")
        print(f"总大小: {output_size} 字节")
        
    except IOError as e:
        print(f"错误：无法写入文件 {output_file}: {e}", file=sys.stderr)
        sys.exit(1)


def main():
    """主函数"""
    if len(sys.argv) < 3:
        print("用法: {} <输出文件> <输入文件1> [<输入文件2> ...]".format(sys.argv[0]))
        print("\n示例:")
        print("  {} output.img file1.bin file2.bin file3.bin".format(sys.argv[0]))
        sys.exit(1)
    
    output_file = sys.argv[1]
    input_files = sys.argv[2:]
    
    create_image_from_files(input_files, output_file)


if __name__ == '__main__':
    main()
