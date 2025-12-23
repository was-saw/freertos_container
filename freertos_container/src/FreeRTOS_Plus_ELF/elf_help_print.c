#include "elf_help_print.h"
#include "elf_loader.h"
#include <stddef.h>
#include <stdint.h>

extern void uart_puts(const char *str);
extern void uart_puthex(uint64_t v);
extern void uart_putchar(uint8_t c);
extern void uart_putcharex(uint64_t v);

/**
 * 打印错误信息
 * @param error_code 错误代码
 */
void print_error(int error_code) {
    switch (error_code) {
        case ELF_ERROR_NULL_POINTER:
            uart_puts("Error: NULL pointer encountered.\n");
            break;
        case ELF_ERROR_INVALID_MAGIC:
            uart_puts("Error: Invalid ELF magic number.\n");
            break;
        case ELF_ERROR_INVALID_CLASS:
            uart_puts("Error: Invalid ELF class.\n");
            break;
        case ELF_ERROR_INVALID_ENDIAN:
            uart_puts("Error: Invalid ELF endianness.\n");
            break;
        case ELF_ERROR_INVALID_VERSION:
            uart_puts("Error: Invalid ELF version.\n");
            break;
        case ELF_ERROR_INVALID_TYPE:
            uart_puts("Error: Invalid ELF type.\n");
            break;
        case ELF_ERROR_INVALID_MACHINE:
            uart_puts("Error: Invalid ELF machine.\n");
            break;
        case ELF_ERROR_SECTION_NOT_FOUND:
            uart_puts("Error: Section not found.\n");
            break;
        case ELF_ERROR_SYMTAB_NOT_FOUND:
            uart_puts("Error: Symbol table not found.\n");
            break;
        case ELF_ERROR_STRTAB_NOT_FOUND:
            uart_puts("Error: String table not found.\n");
            break;
        case ELF_ERROR_RELOCATION_FAILED:
            uart_puts("Error: Relocation failed.\n");
            break;
        default:
            uart_puts("Error: Unknown error code.\n");
            break;
    }
}

/**
 * 输出 ELF 头部信息
 * @param elf_hdr ELF 头部指针
 * 按照结构体的形式输出而非类似readelf那样的输出
 * 主要是用来debug
 */
void print_elf_header(const Elf64_Ehdr *elf_hdr) {
    if (elf_hdr == NULL) {
        uart_puts("ELF Header is NULL.\n");
        return;
    }

    uart_puts("Elf64_Ehdr: {\n");
    uart_puts("  e_ident: { ");
    for (int i = 0; i < EI_NIDENT; i++) {
        uart_puthex(elf_hdr->e_ident[i]);
        uart_puts(" ");
    }
    uart_puts("}\n");
    uart_puts("  e_type: ");
    uart_puthex(elf_hdr->e_type);
    uart_puts("\n");
    uart_puts("  e_machine: ");
    uart_puthex(elf_hdr->e_machine);
    uart_puts("\n");
    uart_puts("  e_version: ");
    uart_puthex(elf_hdr->e_version);
    uart_puts("\n");
    uart_puts("  e_entry: ");
    uart_puthex(elf_hdr->e_entry);
    uart_puts("\n");
    uart_puts("  e_phoff: ");
    uart_puthex(elf_hdr->e_phoff);
    uart_puts("\n");
    uart_puts("  e_shoff: ");
    uart_puthex(elf_hdr->e_shoff);
    uart_puts("\n");
    uart_puts("  e_flags: ");
    uart_puthex(elf_hdr->e_flags);
    uart_puts("\n");
    uart_puts("  e_ehsize: ");
    uart_puthex(elf_hdr->e_ehsize);
    uart_puts("\n");
    uart_puts("  e_phentsize: ");
    uart_puthex(elf_hdr->e_phentsize);
    uart_puts("\n");
    uart_puts("  e_phnum: ");
    uart_puthex(elf_hdr->e_phnum);
    uart_puts("\n");
    uart_puts("  e_shentsize: ");
    uart_puthex(elf_hdr->e_shentsize);
    uart_puts("\n");
    uart_puts("  e_shnum: ");
    uart_puthex(elf_hdr->e_shnum);
    uart_puts("\n");
    uart_puts("  e_shstrndx: ");
    uart_puthex(elf_hdr->e_shstrndx);
    uart_puts("\n");
    uart_puts("}\n");
}

/**
 * 输出 节/符号的名字
 * @param name_no 名字在字符串表中的索引
 * @param strsh 所用字符串表的节头
 * @param elf_data ELF 文件数据指针
 */
void print_name(Elf64_Word name_no, const Elf64_Shdr *strsh, const uint8_t *elf_data) {
    if (strsh == NULL) {
        uart_puts("String section header is NULL.");
        return;
    }

    // 获取字符串表的起始地址
    const char *strtab = (const char *)(strsh->sh_offset + elf_data);
    if (name_no >= strsh->sh_size) {
        uart_puts("Name index out of bounds.");
    } else {
        uart_puts(&strtab[name_no]);
    }
    return;
}

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
                           const uint8_t    *elf_data) {
    if (section_headers == NULL) {
        uart_puts("Section Headers are NULL.\n");
        return;
    }

    uart_puts("Elf64_Shdr: {\n");
    for (int i = 0; i < shnum; i++) {
        uart_puts(" Section Header ");
        uart_puthex(i);
        uart_puts(" {\n");
        uart_puts("  sh_name: ");
        print_name(section_headers[i].sh_name, strsh, elf_data);
        uart_puts("\n");
        uart_puts("  sh_type: ");
        uart_puthex(section_headers[i].sh_type);
        uart_puts("\n");
        uart_puts("  sh_flags: ");
        uart_puthex(section_headers[i].sh_flags);
        uart_puts("\n");
        uart_puts("  sh_addr: ");
        uart_puthex(section_headers[i].sh_addr);
        uart_puts("\n");
        uart_puts("  sh_offset: ");
        uart_puthex(section_headers[i].sh_offset);
        uart_puts("\n");
        uart_puts("  sh_size: ");
        uart_puthex(section_headers[i].sh_size);
        uart_puts("\n");
        uart_puts("  sh_link: ");
        uart_puthex(section_headers[i].sh_link);
        uart_puts("\n");
        uart_puts("  sh_info: ");
        uart_puthex(section_headers[i].sh_info);
        uart_puts("\n");
        uart_puts("  sh_addralign: ");
        uart_puthex(section_headers[i].sh_addralign);
        uart_puts("\n");
        uart_puts("  sh_entsize: ");
        uart_puthex(section_headers[i].sh_entsize);
        uart_puts("\n");
        uart_puts(" }\n");
    }
    uart_puts("}\n");
}

/**
 * 输出 一项节头信息
 * @param shdr 节头指针
 * @param strsh 字符串表节头指针
 * @param elf_data ELF 文件数据指针
 * 按照结构体的形式输出而非类似readelf那样的输出
 * 主要是用来debug
 */
void print_section_header(const Elf64_Shdr *shdr,
                          const Elf64_Shdr *strsh,
                          const uint8_t    *elf_data) {
    if (shdr == NULL) {
        uart_puts("Section Header is NULL.\n");
        return;
    }

    uart_puts("Elf64_Shdr: {\n");
    uart_puts("  sh_name: ");
    print_name(shdr->sh_name, strsh, elf_data);
    uart_puts("\n");
    uart_puts("  sh_type: ");
    uart_puthex(shdr->sh_type);
    uart_puts("\n");
    uart_puts("  sh_flags: ");
    uart_puthex(shdr->sh_flags);
    uart_puts("\n");
    uart_puts("  sh_addr: ");
    uart_puthex(shdr->sh_addr);
    uart_puts("\n");
    uart_puts("  sh_offset: ");
    uart_puthex(shdr->sh_offset);
    uart_puts("\n");
    uart_puts("  sh_size: ");
    uart_puthex(shdr->sh_size);
    uart_puts("\n");
    uart_puts("  sh_link: ");
    uart_puthex(shdr->sh_link);
    uart_puts("\n");
    uart_puts("  sh_info: ");
    uart_puthex(shdr->sh_info);
    uart_puts("\n");
    uart_puts("  sh_addralign: ");
    uart_puthex(shdr->sh_addralign);
    uart_puts("\n");
    uart_puts("  sh_entsize: ");
    uart_puthex(shdr->sh_entsize);
    uart_puts("\n");
    uart_puts("}\n");
}

/**
 * 输出符号信息
 * @param symtab_hdr 符号表节头指针
 * @param strtab_hdr 字符串表节头指针
 * @param elf_data ELF 文件数据指针
 */
void print_symbol(const Elf64_Sym sym, const Elf64_Shdr *strtab_hdr, const uint8_t *elf_data) {
    if (strtab_hdr == NULL) {
        uart_puts("String table header is NULL.\n");
        return;
    }

    uart_puts("Elf64_Sym: {\n");
    uart_puts("  st_name: ");
    print_name(sym.st_name, strtab_hdr, elf_data);
    uart_puts("\n");
    uart_puts("  st_info: ");
    uart_puthex(sym.st_info);
    uart_puts("\n");
    uart_puts("  st_other: ");
    uart_puthex(sym.st_other);
    uart_puts("\n");
    uart_puts("  st_shndx: ");
    uart_puthex(sym.st_shndx);
    uart_puts("\n");
    uart_puts("  st_value: ");
    uart_puthex(sym.st_value);
    uart_puts("\n");
    uart_puts("  st_size: ");
    uart_puthex(sym.st_size);
    uart_puts("\n");
    uart_puts("}\n");
}

/**
 * 输出所有加载到内存的代码段
 * @param context ELF文件加载上下文
 * @param section_memory
 */

void print_code(Elf64_Ctx *context, uint8_t section_memory[MAX_SECTIONS * SECTION_MEMORY_SIZE]) {
    if (context == NULL) {
        uart_puts("Context is NULL.\n");
        return;
    }

    uart_puts("Loaded Sections:\n");
    uint8_t *memory_start = context->memory_pool_index * ELF_MEMORY_SIZE + section_memory;
    for (size_t i = 0; i < context->memory_size; i++) {
        if ((i + 1) % 4 == 0) {
            uart_putcharex(memory_start[i]);
            uart_putchar(' ');
            uart_putcharex(memory_start[i - 1]);
            uart_putchar(' ');
            uart_putcharex(memory_start[i - 2]);
            uart_putchar(' ');
            uart_putcharex(memory_start[i - 3]);
            uart_puts("\n");
        }
    }
}

/**
 * 输出重定位信息
 * @param rela 重定位条目指针
 */
void print_relocation_info(const Elf64_Rela *rela) {
    if (rela == NULL) {
        uart_puts("Relocation entry is NULL.\n");
        return;
    }

    uart_puts("Elf64_Rela: {\n");
    uart_puts("  r_offset: ");
    uart_puthex(rela->r_offset);
    uart_puts("\n");
    uart_puts("  r_info: ");
    uart_puthex(rela->r_info);
    uart_puts("\n");
    uart_puts("  r_addend: ");
    uart_puthex(rela->r_addend);
    uart_puts("\n");
    uart_puts("}\n");
}

/**
 * 输出elf context信息
 * @param context ELF文件加载上下文
 */
void print_context(const Elf64_Ctx *context) {
    if (context == NULL) {
        uart_puts("Context is NULL.\n");
        return;
    }

    uart_puts("Elf64_Ctx: {\n");
    uart_puts("  elf_data: ");
    uart_puthex((uint64_t)context->elf_data);
    uart_puts("\n");
    uart_puts("  elf_size: ");
    uart_puthex(context->elf_size);
    uart_puts("\n");
    uart_puts("  elf_hdr: ");
    uart_puthex((uint64_t)context->elf_hdr);
    uart_puts("\n");
    uart_puts("  section_headers: ");
    uart_puthex((uint64_t)context->section_headers);
    uart_puts("\n");
    uart_puts("  program_headers: ");
    uart_puthex((uint64_t)context->program_headers);
    uart_puts("\n");
    uart_puts("  shstrtab: ");
    uart_puthex((uint64_t)context->shstrtab);
    uart_puts("\n");
    uart_puts("  shstrtab_hdr: ");
    uart_puthex((uint64_t)context->shstrtab_hdr);
    uart_puts("\n");
    uart_puts("  symtab_hdr: ");
    uart_puthex((uint64_t)context->symtab_hdr);
    uart_puts("\n");
    uart_puts("  strtab_hdr: ");
    uart_puthex((uint64_t)context->strtab_hdr);
    uart_puts("\n");
    uart_puts("  symtab: ");
    uart_puthex((uint64_t)context->symtab);
    uart_puts("\n");
    uart_puts("  strtab: ");
    uart_puthex((uint64_t)context->strtab);
    uart_puts("\n");
    uart_puts("  rela: ");
    uart_puthex((uint64_t)context->rela);
    uart_puts("\n");
    uart_puts("  memory_pool_index: ");
    uart_puthex(context->memory_pool_index);
    uart_puts("\n");
    uart_puts("  memory_size: ");
    uart_puthex(context->memory_size);
    uart_puts("\n");
    for (int i = 0; i < MAX_SECTIONS; i++) {
        uart_puts("  load_sections[");
        uart_putcharex(i);
        uart_puts("]: ");
        uart_puthex(context->load_sections[i]);
        uart_puts("\n");
    }
    uart_puts("  result: ");
    uart_puthex(context->result);
    uart_putchar('\n');
}