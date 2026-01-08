#!/bin/bash
# 自动从 lfs_app 软链接目录中的所有 .elf 文件生成镜像

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 输出镜像文件名
OUTPUT_IMAGE="image"

# 查找所有 .elf 文件（按名称排序）
# 软链接需要使用 -L 选项来跟随链接
ELF_FILES=($(find -L lfs_app -maxdepth 1 -name "*-exec.elf" -type f | sort))

if [ ${#ELF_FILES[@]} -eq 0 ]; then
    echo "错误：在 lfs_app/ 目录中没有找到 .elf 文件"
    exit 1
fi

echo "找到 ${#ELF_FILES[@]} 个 ELF 文件："
for elf in "${ELF_FILES[@]}"; do
    echo "  - $elf"
done

# 调用 Python 脚本生成镜像
python3 make_image_from_files.py "$OUTPUT_IMAGE" "${ELF_FILES[@]}"

if [ $? -eq 0 ]; then
    echo ""
    echo "镜像已生成: $OUTPUT_IMAGE"
    ls -lh "$OUTPUT_IMAGE"
else
    echo "错误：镜像生成失败"
    exit 1
fi
