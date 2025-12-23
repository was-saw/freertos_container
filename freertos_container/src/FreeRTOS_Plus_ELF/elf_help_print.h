#ifndef ELF_HELP_PRINT_H
#define ELF_HELP_PRINT_H

// 静态内存池定义
#define MAX_SECTIONS 16
#define SECTION_MEMORY_SIZE (64 * 1024) // 每个段最大64KB

#include "elf_loader.h"
#include <stddef.h>
#include <stdint.h>

/**
 * 输出 ELF 头部信息
 * @param elf_hdr ELF 头部指针
 * 按照结构体的形式输出而非类似readelf那样的输出
 * 主要是用来debug
 */
void print_elf_header(const Elf64_Ehdr *elf_hdr);

/**
 * 输出 节/符号的名字
 * @param name_no 名字在字符串表中的索引
 * @param strsh 所用字符串表的节头
 * @param elf_data ELF 文件数据指针
 */
void print_name(Elf64_Word name_no, const Elf64_Shdr *strsh, const uint8_t *elf_data);

/**
 * 输出 节头表 信息
 * @param section_headers 节头表指针
 * @param shnum 节头表条目数
 * @param strsh 字符串表节头指针
 * @param elf_data ELF 文件数据指针
 * 按照结构体的形式输出而非类似readelf那样的输出
 * 主要是用来debug
 */
void print_section_headers(const Elf64_Shdr *section_headers,
                           Elf64_Half        shnum,
                           const Elf64_Shdr *strsh,
                           const uint8_t    *elf_data);

/**
 * 输出 一项节头信息
 * @param shdr 节头指针
 * @param strsh 字符串表节头指针
 * @param elf_data ELF 文件数据指针
 * 按照结构体的形式输出而非类似readelf那样的输出
 * 主要是用来debug
 */
void print_section_header(const Elf64_Shdr *shdr, const Elf64_Shdr *strsh, const uint8_t *elf_data);

/**
 * 输出符号表信息
 * @param symtab_hdr 符号表节头指针
 * @param strtab_hdr 字符串表节头指针
 * @param elf_data ELF 文件数据指针
 */
void print_symbol_table(const Elf64_Shdr *symtab_hdr,
                        const Elf64_Shdr *strtab_hdr,
                        const uint8_t    *elf_data);

/**
 * 输出符号信息
 * @param symtab_hdr 符号表节头指针
 * @param strtab_hdr 字符串表节头指针
 * @param elf_data ELF 文件数据指针
 */
void print_symbol(const Elf64_Sym sym, const Elf64_Shdr *strtab_hdr, const uint8_t *elf_data);

/**
 * 输出所有加载到内存的代码段
 * @param context ELF文件加载上下文
 * @param section_memory
 */

void print_code(Elf64_Ctx *context, uint8_t section_memory[MAX_SECTIONS * SECTION_MEMORY_SIZE]);

/**
 * 打印错误信息
 * @param error_code 错误代码
 */
void print_error(int error_code);

/**
 * 输出重定位信息
 * @param rela 重定位条目指针
 */
void print_relocation_info(const Elf64_Rela *rela);

/**
 * 输出elf context信息
 * @param context ELF文件加载上下文
 */
void print_context(const Elf64_Ctx *context);

#endif // ELF_HELP_PRINT_H
