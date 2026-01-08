#!/usr/bin/env python3
"""
将镜像文件解包为多个文件

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


def unpack_image_to_files(image_file, output_dir):
    """
    将镜像文件解包为多个文件
    
    Args:
        image_file: 输入镜像文件路径
        output_dir: 输出目录路径
    """
    # 检查镜像文件是否存在
    if not os.path.isfile(image_file):
        print(f"错误：镜像文件不存在: {image_file}", file=sys.stderr)
        sys.exit(1)
    
    # 创建输出目录
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
        print(f"创建输出目录: {output_dir}")
    elif not os.path.isdir(output_dir):
        print(f"错误：{output_dir} 不是一个目录", file=sys.stderr)
        sys.exit(1)
    
    try:
        with open(image_file, 'rb') as img_f:
            # 读取文件头：文件总数（1字节）
            num_files_data = img_f.read(1)
            if len(num_files_data) != 1:
                print("错误：无法读取文件头", file=sys.stderr)
                sys.exit(1)
            
            num_files = struct.unpack('B', num_files_data)[0]
            print(f"镜像中的文件总数: {num_files}")
            
            if num_files == 0:
                print("警告：镜像中没有文件")
                return
            
            # 解包每个文件
            for idx in range(1, num_files + 1):
                # 读取文件大小（8字节）
                size_data = img_f.read(8)
                if len(size_data) != 8:
                    print(f"错误：无法读取文件 {idx} 的大小", file=sys.stderr)
                    sys.exit(1)
                
                file_size = struct.unpack('<Q', size_data)[0]
                
                # 读取文件名（256字节）
                filename_data = img_f.read(256)
                if len(filename_data) != 256:
                    print(f"错误：无法读取文件 {idx} 的文件名", file=sys.stderr)
                    sys.exit(1)
                
                # 解析文件名（去除null字节后的内容）
                filename = filename_data.split(b'\x00', 1)[0].decode('utf-8')
                if not filename:
                    filename = f"file_{idx}.bin"
                    print(f"  警告：文件 {idx} 没有文件名，使用默认名称: {filename}")
                
                # 读取文件内容
                file_data = img_f.read(file_size)
                if len(file_data) != file_size:
                    print(f"错误：无法读取文件 {idx} 的完整内容 (期望 {file_size} 字节，实际读取 {len(file_data)} 字节)", file=sys.stderr)
                    sys.exit(1)
                
                # 写入文件
                output_path = os.path.join(output_dir, filename)
                
                # 确保输出路径的目录存在
                output_file_dir = os.path.dirname(output_path)
                if output_file_dir and not os.path.exists(output_file_dir):
                    os.makedirs(output_file_dir)
                
                with open(output_path, 'wb') as out_f:
                    out_f.write(file_data)
                
                print(f"  文件 {idx}: {filename} ({file_size} 字节) -> {output_path}")
            
            # 检查是否还有多余的数据
            remaining = img_f.read()
            if remaining:
                print(f"\n警告：镜像文件末尾有 {len(remaining)} 字节未使用的数据")
            
            print(f"\n成功解包 {num_files} 个文件到: {output_dir}")
            
    except IOError as e:
        print(f"错误：读取文件失败: {e}", file=sys.stderr)
        sys.exit(1)
    except UnicodeDecodeError as e:
        print(f"错误：文件名解码失败: {e}", file=sys.stderr)
        sys.exit(1)


def main():
    """主函数"""
    if len(sys.argv) != 3:
        print("用法: {} <镜像文件> <输出目录>".format(sys.argv[0]))
        print("\n示例:")
        print("  {} output.img ./extracted_files".format(sys.argv[0]))
        sys.exit(1)
    
    image_file = sys.argv[1]
    output_dir = sys.argv[2]
    
    unpack_image_to_files(image_file, output_dir)


if __name__ == '__main__':
    main()
