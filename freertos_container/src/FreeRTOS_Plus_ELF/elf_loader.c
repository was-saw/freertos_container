#include "elf_loader.h"
#include "FreeRTOS.h"
#include "elf_help_print.h"
#include "syscall.h"
#include <stddef.h>
#include <stdint.h>

#ifdef DEBUG_ELF_LOADER
extern void uart_puts(const char *str);
extern void uart_puthex(uint64_t v);
extern void uart_putchar(uint8_t c);
#endif

extern FreeRTOSSyscalls_t freertos_syscalls;
extern GOT_t              got;

static uint64_t   elf_bits_map;
static Elf64_Ctx  elf_ctxs[MAX_ELF];
static Elf64_Addr elf_memory_addr[MAX_ELF] = {};
static uint8_t    section_memory[ELF_MEMORY_SIZE * MAX_ELF];


/**
 * 校验 ELF 文件头部
 * @param elf_hdr ELF 头部指针
 * @return 成功返回 ELF_SUCCESS，失败返回相应错误码
 */
static int validate_elf_header(const Elf64_Ehdr *elf_hdr) {
    // 检查指针是否为空
    if (elf_hdr == NULL) {
        return ELF_ERROR_NULL_POINTER;
    }

    // 校验 ELF 魔数
    if (elf_hdr->e_ident[EI_MAG0] != ELF_MAGIC_0 || elf_hdr->e_ident[EI_MAG1] != ELF_MAGIC_1 ||
        elf_hdr->e_ident[EI_MAG2] != ELF_MAGIC_2 || elf_hdr->e_ident[EI_MAG3] != ELF_MAGIC_3) {
        return ELF_ERROR_INVALID_MAGIC;
    }

    // 校验文件类别（必须为64位）
    if (elf_hdr->e_ident[EI_CLASS] != ELF_CLASS_64) {
        return ELF_ERROR_INVALID_CLASS;
    }

    // 校验字节序（必须为小端序）
    if (elf_hdr->e_ident[EI_DATA] != ELF_DATA_LSB) {
        return ELF_ERROR_INVALID_ENDIAN;
    }

    // 校验 ELF 版本
    if (elf_hdr->e_ident[EI_VERSION] != ELF_VERSION_1) {
        return ELF_ERROR_INVALID_VERSION;
    }

    // 校验文件类型（可重定位文件）
    if (elf_hdr->e_type != ET_REL && elf_hdr->e_type != ET_EXEC) {
        return ELF_ERROR_INVALID_TYPE;
    }

    // 校验机器类型（ARM64/AArch64）
    if (elf_hdr->e_machine != ELF_MACHINE_AARCH64) {
        return ELF_ERROR_INVALID_MACHINE;
    }

    return ELF_SUCCESS;
}

/**
 * 解析节头表并验证其有效性
 * @param context ELF文件加载上下文
 * @return 成功返回 ELF_SUCCESS，失败返回相应错误码
 */
static int parse_section_headers(Elf64_Ctx *context) {
    // 检查是否有节头表
    if (context->elf_hdr->e_shnum == 0) {
        context->section_headers = NULL;
        return ELF_SUCCESS;
    }

    // 计算节头表的文件偏移和大小
    Elf64_Off  shoff = context->elf_hdr->e_shoff;
    Elf64_Half shentsize = context->elf_hdr->e_shentsize;
    Elf64_Half shnum = context->elf_hdr->e_shnum;

    // 检查节头表偏移是否有效
    if (shoff == 0) {
        return ELF_ERROR_SECTION_NOT_FOUND;
    }

    // 检查节头表条目大小是否正确
    if (shentsize != sizeof(Elf64_Shdr)) {
        return ELF_ERROR_SECTION_NOT_FOUND;
    }

    // 检查节头表是否超出文件范围
    size_t section_table_size = (size_t)shnum * shentsize;
    if (shoff + section_table_size > context->elf_size) {
        return ELF_ERROR_SECTION_NOT_FOUND;
    }

    // 获取节头表指针
    context->section_headers = (Elf64_Shdr *)(context->elf_data + shoff);

    return ELF_SUCCESS;
}

/**
 * 获取节字符串表
 * @param context ELF文件加载上下文
 * @return 成功返回 ELF_SUCCESS，失败返回相应错误码
 */
static int get_section_string_table(Elf64_Ctx *context) {
    const Elf64_Ehdr *elf_hdr = context->elf_hdr;
    Elf64_Half        shstrndx = context->elf_hdr->e_shstrndx;
    const Elf64_Shdr *shstrtab_hdr = NULL;

    // 检查节字符串表索引是否有效
    if (shstrndx == SHN_UNDEF || shstrndx >= elf_hdr->e_shnum) {
        context->shstrtab = NULL;
        return ELF_SUCCESS;
    }

    shstrtab_hdr = &context->section_headers[shstrndx];
#ifdef DEBUG_ELF_LOADER
    print_section_header(shstrtab_hdr, shstrtab_hdr, context->elf_data);
    // 检查节字符串表是否为字符串表类型
#endif
    if (shstrtab_hdr->sh_type != SHT_STRTAB) {
        return ELF_ERROR_STRTAB_NOT_FOUND;
    }

    // 检查节字符串表是否超出文件范围
    if (shstrtab_hdr->sh_offset + shstrtab_hdr->sh_size > context->elf_size) {
        return ELF_ERROR_STRTAB_NOT_FOUND;
    }

    context->shstrtab_hdr = shstrtab_hdr;
    context->shstrtab = (const char *)(context->elf_data + shstrtab_hdr->sh_offset);
    return ELF_SUCCESS;
}


/**
 * 查找符号表和字符串表
 * @param context Elf加载上下文
 * @return 成功返回 ELF_SUCCESS，失败返回相应错误码
 */
static int find_symbol_tables(Elf64_Ctx *context) {
    const Elf64_Ehdr *elf_hdr = context->elf_hdr;
    const Elf64_Shdr *section_headers = context->section_headers;
    context->symtab_hdr = NULL;
    context->strtab_hdr = NULL;

    for (int i = 0; i < elf_hdr->e_shnum; i++) {
        if (section_headers[i].sh_type == SHT_SYMTAB) {
            context->symtab_hdr = &section_headers[i];

            // 符号表的sh_link字段指向关联的字符串表
            if (section_headers[i].sh_link < elf_hdr->e_shnum) {
                context->strtab_hdr = &section_headers[section_headers[i].sh_link];
                break;
            }
        }
    }

    if (context->symtab_hdr == NULL) {
        return ELF_ERROR_SYMTAB_NOT_FOUND;
    }

    if (context->strtab_hdr == NULL) {
        return ELF_ERROR_STRTAB_NOT_FOUND;
    }

    return ELF_SUCCESS;
}

/**
 * 分配内存并加载段
 * @param elf_data ELF 文件数据指针
 * @param elf_size ELF 文件大小
 * @param section_headers 节头表指针
 * @param shnum 节头表条目数
 * @param loaded_sections 输出的已加载段指针数组
 * @return 成功返回 ELF_SUCCESS，失败返回相应错误码
 */
static int relo_load_sections(Elf64_Ctx *context) {
    const Elf64_Shdr *section_headers = context->section_headers;
    Elf64_Addr        memory_offset = 0;

    for (int i = 0; i < MAX_ELF; i++) {
        if (elf_memory_addr[i] == (Elf64_Addr)NULL) {
            context->memory_pool_index = i;
            break;
        }
    }

    for (int i = 0; i < context->elf_hdr->e_shnum; i++) {
        const Elf64_Shdr *shdr = &section_headers[i];

        // 只加载需要分配的段（SHF_ALLOC）
        if (!(shdr->sh_flags & SHF_ALLOC)) {
            continue;
        }

        // 跳过大小为0的段
        if (shdr->sh_size == 0) {
            continue;
        }

        // 检查段大小是否超过内存池限制
        if (shdr->sh_size > ELF_MEMORY_SIZE) {
#ifdef DEBUG_ELF_LOADER
            uart_puts("Section ");
            uart_puthex(i);
            uart_puts(" size (");
            uart_puthex(shdr->sh_size);
            uart_puts(") exceeds maximum (");
            uart_puthex(ELF_MEMORY_SIZE);
            uart_puts(")\n");
#endif
            return ELF_ERROR_NULL_POINTER;
        }

#ifdef DEBUG_ELF_LOADER
        print_context(context);
        uart_puthex(memory_offset);
        uart_putchar('\n');
#endif
        // 如果是PROGBITS段，从文件中复制数据
        if (shdr->sh_type == SHT_PROGBITS) {
            context->load_sections[i] =
                (Elf64_Addr)(section_memory + context->memory_pool_index * ELF_MEMORY_SIZE +
                             memory_offset);
            if (shdr->sh_offset + shdr->sh_size > context->elf_size) {
#ifdef DEBUG_ELF_LOADER
                uart_puts("Section data exceeds file size for section ");
                uart_puthex(i);
                uart_puts("\n");
#endif
                return ELF_ERROR_SECTION_NOT_FOUND;
            }

            // 复制数据
            uint8_t       *dest = (uint8_t *)context->load_sections[i];
            const uint8_t *src = context->elf_data + shdr->sh_offset;
            for (size_t j = 0; j < shdr->sh_size; j++) {
                dest[j] = src[j];
            }
        } else if (shdr->sh_type == SHT_NOBITS) {
            // BSS段，清零
            uint8_t *dest = (uint8_t *)context->load_sections[i];
            for (size_t j = 0; j < shdr->sh_size; j++) {
                dest[j] = 0;
            }
        }

        memory_offset += shdr->sh_size;

#ifdef DEBUG_ELF_LOADER
        // uart_puts("Loaded section ");
        // uart_puthex(i);
        // uart_puts(" at address ");
        // uart_puthex((uint64_t)context->load_sections[i]);
        // uart_puts(" size ");
        // uart_puthex(shdr->sh_size);
        // uart_puts("\n");
        // print_section_header(shdr, context->shstrtab_hdr, context->elf_data);
#endif
    }

    context->memory_size = memory_offset;
#ifdef DEBUG_ELF_LOADER
    print_context(context);
    uart_puthex(memory_offset);
    uart_putchar('\n');
#endif

    return ELF_SUCCESS;
}

/**
 * 简单字符串比较函数
 * @param s1 字符串1
 * @param s2 字符串2
 * @return 相等返回0，不相等返回非0
 */
static int strcmp_simple(const char *s1, const char *s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/**
 * 解析外部符号
 * @param sym_name 符号名称
 * @return 符号地址，失败返回0
 */
static Elf64_Addr resolve_external_symbol(const char *sym_name) {
    // 比较字符串
    for (size_t i = 0; i < got.num_entries; i++) {
        if (strcmp_simple(got.entrys[i].name, sym_name) == 0) {
            return (Elf64_Addr)(&got.entrys[i].address);
        }
    }
    return 0;
}


/// S (when used on its own) is the address of the symbol.
/// A is the addend for the relocation.
/// P is the address of the place being relocated (derived from r_offset).
/// X is the result of a relocation operation, before any masking or bit-selection operation is
/// applied Page(expr) is the page address of the expression expr, defined as (expr & ~0xFFF). (This
/// applies even if the machine page size supported by the platform has a different value.) GOT is
/// the address of the Global Offset Table, the table of code and data addresses to be resolved at
/// dynamic link time. The GOT and each entry in it must be, 64-bit aligned for ELF64 or 32-bit
/// aligned for ELF32. GDAT(S+A) represents a pointer-sized entry in the GOT for address S+A. The
/// entry will be relocated at run time with relocation R_<CLS>_GLOB_DAT(S+A). G(expr) is the
/// address of the GOT entry for the expression expr. Delta(S) if S is a normal symbol, resolves to
/// the difference between the static link address of S and the execution address of S. If S is the
/// null symbol (ELF symbol index 0), resolves to the difference between the static link address of
/// P and the execution address of P. Indirect(expr) represents the result of calling expr as a
/// function. The result is the return value from the function that is returned in r0. The arguments
/// passed to the function are defined by the platform ABI. [msb:lsb] is a bit-mask operation
/// representing the selection of bits in a value. The bits selected range from lsb up to msb
/// inclusive. For example, ‘bits [3:0]’ represents the bits under the mask 0x0000000F. When range
/// checking is applied to a value, it is applied before the masking operation is performed.

/**
 * 应用单个重定位
 * @param context Elf文件加载上下文
 * @param target_section 目标段指针
 * @param rela_index 重定位条目索引
 * @return 成功返回 ELF_SUCCESS，失败返回相应错误码
 */
static int apply_relocation(Elf64_Ctx *context, Elf64_Addr target_section, int rela_index) {
    const Elf64_Rela *rela = &context->rela[rela_index];
    const Elf64_Shdr *symtab_hdr = context->symtab_hdr;
    const Elf64_Sym  *symtab = context->symtab;
    Elf64_Half        shnum = context->elf_hdr->e_shnum;
    Elf64_Addr        reloc_addr = target_section + rela->r_offset;
    Elf64_Xword       sym_idx = ELF64_R_SYM(rela->r_info);
    Elf64_Xword       reloc_type = ELF64_R_TYPE(rela->r_info);

    if (sym_idx >= (symtab_hdr->sh_size / sizeof(Elf64_Sym))) {
#ifdef DEBUG_ELF_LOADER
        uart_puts("Invalid symbol index in relocation\n");
#endif
        return ELF_ERROR_RELOCATION_FAILED;
    }

    const Elf64_Sym *sym = &symtab[sym_idx];
#ifdef DEBUG_ELF_LOADER
    print_symbol(*sym, context->strtab_hdr, context->elf_data);
#endif
    Elf64_Addr sym_value = 0;

    // 计算符号值
    if (sym->st_shndx == SHN_UNDEF) {
        // 外部符号，需要查找系统调用
        if (sym->st_name < symtab_hdr->sh_size) {
            const char *sym_name = context->strtab + sym->st_name;
            sym_value = resolve_external_symbol(sym_name);
            if (sym_value == 0) {
#ifdef DEBUG_ELF_LOADER
                uart_puts("Failed to resolve symbol: ");
                uart_puts(sym_name);
                uart_puts("\n");
#endif
                return ELF_ERROR_RELOCATION_FAILED;
            }
        }
    } else if (sym->st_shndx < shnum && context->load_sections[sym->st_shndx] != 0) {
        // 内部符号
        sym_value = (Elf64_Addr)context->load_sections[sym->st_shndx] + sym->st_value;
    } else {
#ifdef DEBUG_ELF_LOADER
        uart_puts("Invalid symbol section index\n");
#endif
        return ELF_ERROR_RELOCATION_FAILED;
    }
    // 应用重定位
    switch (reloc_type) {
        case R_AARCH64_CALL26: {
            // S + A - P
            // Set a CALL immediate field to bits [27:2] of X; check that -2^27 <= X < 2^27
            Elf64_Addr S = sym_value;
            Elf64_Addr A = (Elf64_Addr)rela->r_addend;
            Elf64_Addr P = (Elf64_Addr)reloc_addr;
            Elf64_Addr X = S + A - P;
            // 检查范围
            if ((int64_t)X < -(1 << 27) || (int64_t)X >= (1 << 27)) {
                return ELF_ERROR_RELOCATION_FAILED;
            } else {
                // 修改指令
                uint32_t *instr = (uint32_t *)reloc_addr;
                uint32_t  imm26 = (uint32_t)((X >> 2) & 0x03FFFFFF);
                *instr = (*instr & 0xFC000000) | imm26;
            }
        } break;
        case R_AARCH64_ADR_PREL_LO21: {
            // S + A - P
            // Set an ADR immediate value to bits [20:0] of X; check that -2^20 <= X < 2^20
            Elf64_Addr S = sym_value;
            Elf64_Addr A = (Elf64_Addr)rela->r_addend;
            Elf64_Addr P = (Elf64_Addr)reloc_addr;
            Elf64_Addr X = S + A - P;
            // 检查范围
            if ((int64_t)X < -(1 << 20) || (int64_t)X >= (1 << 20)) {
                return ELF_ERROR_RELOCATION_FAILED;
            } else {
                // 修改指令
                uint32_t *instr = (uint32_t *)reloc_addr;
                uint32_t  imm = (uint32_t)(X & 0x1FFFFF);
                uint32_t  immlo = imm & 0x3;
                uint32_t  immhi = (imm >> 2) & 0x7FFFF;
                *instr = (*instr & 0x9F00001F) | (immlo << 29) | (immhi << 5);
            }
        } break;
        case R_AARCH64_GOT_LD_PREL19: {
            // print_symbol(*sym, strtab_hdr, elf_data);
            // print_relocation_info(rela);
            // G(GDAT(S))- P
            // Set a load-literal immediate field[23:5] to bits [20:2] of X; check –2^20 <= X < 2^20
            Elf64_Addr G_GDAT_S = sym_value;
            Elf64_Addr P = (Elf64_Addr)reloc_addr;
            Elf64_Addr X = G_GDAT_S - P;
            // 检查范围
            if ((int64_t)X < -(1 << 20) || (int64_t)X >= (1 << 20)) {
                return ELF_ERROR_RELOCATION_FAILED;
            } else {
                // 修改指令
                uint32_t *instr = (uint32_t *)reloc_addr;
                uint32_t  imm = (uint32_t)(X & 0x1FFFFF);
                uint32_t  imm19 = (imm >> 2) & 0x7FFFF;
                *instr = (*instr & 0xFF00001F) | (imm19 << 5);
            }
        } break;
        default:
#ifdef DEBUG_ELF_LOADER
            uart_puts("Unsupported relocation type: ");
            uart_puthex(reloc_type);
            uart_puts("\n");
#endif
            return ELF_ERROR_RELOCATION_FAILED;
    }

    return ELF_SUCCESS;
}

/**
 * 处理重定位
 * @param context ELF文件加载上下文
 * @return 成功返回 ELF_SUCCESS，失败返回相应错误码
 */
static int process_relocations(Elf64_Ctx *context) {
    const Elf64_Shdr *section_headers = context->section_headers;
    Elf64_Half        shnum = context->elf_hdr->e_shnum;
    for (int i = 0; i < shnum; i++) {
        const Elf64_Shdr *shdr = &section_headers[i];

        // 处理重定位段
        if (shdr->sh_type == SHT_RELA) {
#ifdef DEBUG_ELF_LOADER
            print_section_header(shdr, context->shstrtab_hdr, context->elf_data);
#endif
            // sh_info字段指向要重定位的目标段
            if (shdr->sh_info >= shnum || context->load_sections[shdr->sh_info] == 0) {
#ifdef DEBUG_ELF_LOADER
                uart_puts("Invalid target section for relocation\n");
#endif
                continue;
            }

            context->rela = (const Elf64_Rela *)(context->elf_data + shdr->sh_offset);
            size_t rela_count = shdr->sh_size / shdr->sh_entsize;

            for (size_t j = 0; j < rela_count; j++) {
                int result = apply_relocation(context, context->load_sections[shdr->sh_info], j);
                if (result != ELF_SUCCESS) {
                    return result;
                }
            }
        }
    }

    return ELF_SUCCESS;
}

/**
 * 查找并执行main函数
 * @param context ELF文件加载上下文
 * @return 成功返回 ELF_SUCCESS，失败返回相应错误码
 */
static int find_and_execute_main(Elf64_Ctx *context) {
    if (context->elf_hdr->e_type == ET_REL) {
        const Elf64_Sym  *symtab = context->symtab;
        const char       *strtab = context->strtab;
        const Elf64_Shdr *symtab_hdr = context->symtab_hdr;
        const Elf64_Addr *loaded_sections = context->load_sections;

        size_t sym_count = symtab_hdr->sh_size / sizeof(Elf64_Sym);
        for (size_t i = 0; i < sym_count; i++) {
            const Elf64_Sym *sym = &symtab[i];

            // 查找main函数
            if (ELF64_ST_TYPE(sym->st_info) == STT_FUNC && sym->st_shndx != SHN_UNDEF &&
                sym->st_name < symtab_hdr->sh_size) {

                const char *sym_name = strtab + sym->st_name;
                if (strcmp_simple(sym_name, "main") == 0) {
                    // 找到main函数
                    if (loaded_sections[sym->st_shndx] != 0) {
                        void *main_addr =
                            (void *)((uint8_t *)loaded_sections[sym->st_shndx] + sym->st_value);
                        // 执行main函数
                        int (*main_func)(void) = (int (*)(void))main_addr;
                        context->result = main_func();
                        return ELF_SUCCESS;
                    }
                }
            }
        }
#ifdef DEBUG_ELF_LOADER
        uart_puts("Main function not found\n");
#endif
        return ELF_ERROR_SYMTAB_NOT_FOUND;
    } else {
        if (context->elf_hdr->e_entry != 0) {
            void *entry_addr =
                (void *)(section_memory + context->memory_pool_index * ELF_MEMORY_SIZE +
                         context->elf_hdr->e_entry);
            // 执行入口函数
            int (*entry_func)(void) = (int (*)(void))entry_addr;
            context->result = entry_func();
            return ELF_SUCCESS;
        } else {
            return ELF_ERROR_STRTAB_NOT_FOUND;
        }
    }
}

/**
 * 清理已加载的段
 * @param section_headers 节头表指针
 * @param shnum 节头表条目数
 * @param loaded_sections 已加载段指针数组
 */
static void cleanup_loaded_sections(int memory_used_index) {
    elf_memory_addr[memory_used_index] = (Elf64_Addr)NULL;
}

/**
 * 解析程序头表并验证其有效性
 * @param context ELF文件加载上下文
 * @return 成功返回 ELF_SUCCESS，失败返回相应错误码
 */
static int parse_program_headers(Elf64_Ctx *context) {
    const Elf64_Ehdr *elf_hdr = context->elf_hdr;

    // 检查是否有程序头表
    if (context->elf_hdr->e_phnum == 0) {
        context->program_headers = NULL;
        return ELF_SUCCESS;
    }

    // 计算程序头表的文件偏移和大小
    Elf64_Off  phoff = elf_hdr->e_phoff;
    Elf64_Half phentsize = elf_hdr->e_phentsize;
    Elf64_Half phnum = elf_hdr->e_phnum;

    // 检查程序头表偏移是否有效
    if (phoff == 0) {
        return ELF_ERROR_PROGRAM_NOT_FOUND;
    }

    // 检查程序头表条目大小是否正确
    if (phentsize != sizeof(Elf64_Phdr)) {
        return ELF_ERROR_PROGRAM_NOT_FOUND;
    }

    // 检查程序头表是否超出文件范围
    if (phoff + phentsize * phnum > context->elf_size) {
        return ELF_ERROR_PROGRAM_NOT_FOUND;
    }

    // 获取程序头表指针
    context->program_headers = (const Elf64_Phdr *)(context->elf_data + phoff);
    return ELF_SUCCESS;
}

/**
 * 加载可执行elf的section
 * @param context ELF文件加载上下文
 */

static int exec_load_sections(Elf64_Ctx *context) {
    const Elf64_Phdr *program_headers = context->program_headers;
    Elf64_Half        phnum = context->elf_hdr->e_phnum;
    Elf64_Addr        memory_offset = 0;

    for (int i = 0; i < MAX_ELF; i++) {
        if (elf_memory_addr[i] == (Elf64_Addr)NULL) {
            context->memory_pool_index = i;
            break;
        }
    }

    for (int i = 0; i < phnum; i++) {
        const Elf64_Phdr *phdr = &program_headers[i];

        // 只加载需要分配的段（PT_LOAD）
        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        // 跳过大小为0的段
        if (phdr->p_memsz == 0) {
            continue;
        }

        // 检查段大小是否超过内存池限制
        if (phdr->p_memsz > ELF_MEMORY_SIZE) {
            return ELF_OOM;
        }

        for (Elf64_Xword i = 0; i < phdr->p_filesz; i++) {
            // 将 ELF 文件中的数据加载到内存中
            uint8_t *dest = (uint8_t *)section_memory +
                            context->memory_pool_index * ELF_MEMORY_SIZE + memory_offset + i;
            *dest = context->elf_data[phdr->p_offset + i];
        }
        memory_offset += phdr->p_memsz;
    }
    context->memory_size = memory_offset;

    return ELF_SUCCESS;
}

/**
 * 加载 ELF 文件并执行 main 函数
 * @param elf_data ELF 文件数据指针
 * @param elf_size ELF 文件大小
 * @return 成功返回 ELF_SUCCESS，失败返回相应错误码
 */
int elf_load_and_run(const uint8_t *elf_data, size_t elf_size) {
    int        result;
    int        elf_ctx_index = 0;
    Elf64_Ctx *context = NULL;
    for (; elf_ctx_index < MAX_ELF; elf_ctx_index++) {
        if ((elf_bits_map & (1 << elf_ctx_index)) == 0) {
            elf_bits_map |= (1 << elf_ctx_index);
            context = &elf_ctxs[elf_ctx_index];
            context->elf_data = elf_data;
            context->elf_size = elf_size;
            context->elf_hdr = NULL;
            context->section_headers = NULL;
            context->program_headers = NULL;
            context->shstrtab = NULL;
            context->shstrtab_hdr = NULL;
            context->symtab_hdr = NULL;
            context->strtab_hdr = NULL;
            context->symtab = NULL;
            context->strtab = NULL;
            context->rela = NULL;
            context->memory_pool_index = -1;
            for (int i = 0; i < MAX_ELF; i++) {
                context->load_sections[i] = 0;
            }
            context->result = ELF_NOT_RUN;
            break;
        }
    }

    // 检查输入参数
    if (elf_data == NULL) {
        result = ELF_ERROR_NULL_POINTER;
        goto failed;
    }

    // 检查文件大小是否足够包含 ELF 头部
    if (elf_size < sizeof(Elf64_Ehdr)) {
        result = ELF_ERROR_INVALID_MAGIC;
        goto failed;
    }

    // 获取 ELF 头部指针
    context->elf_hdr = (Elf64_Ehdr *)elf_data;

    // 校验 ELF 头部
    result = validate_elf_header(context->elf_hdr);
    if (result != ELF_SUCCESS) {
        goto failed;
    }

#ifdef DEBUG_ELF_LOADER
    uart_puts("ELF header is valid.\n");
    print_elf_header(context->elf_hdr);
#endif

    if (context->elf_hdr->e_type == ET_REL) {
        result = parse_section_headers(context);
        if (result != ELF_SUCCESS) {
            goto failed;
        }

        if (context->section_headers != NULL) {
#ifdef DEBUG_ELF_LOADER
            uart_puts("Section header table loaded successfully.\n");
#endif
        } else {
#ifdef DEBUG_ELF_LOADER
            uart_puts("Warning: No section headers found.\n");
#endif
            result = ELF_ERROR_SECTION_NOT_FOUND;
            goto failed;
        }

        // 获取节字符串表
        if (context->section_headers != NULL) {
            result = get_section_string_table(context);
            if (result != ELF_SUCCESS) {
                goto failed;
            }

            if (context->shstrtab != NULL) {
#ifdef DEBUG_ELF_LOADER
                uart_puts("Section string table loaded successfully.\n");
#endif
            } else {
#ifdef DEBUG_ELF_LOADER
                uart_puts("Warning: No section string table found.\n");
#endif
                result = ELF_ERROR_STRTAB_NOT_FOUND;
                goto failed;
            }
        }

        // 查找符号表和字符串表

        result = find_symbol_tables(context);
        if (result != ELF_SUCCESS) {
            goto failed;
        }

        if (context->symtab_hdr != NULL && context->strtab_hdr != NULL) {
            context->symtab = (const Elf64_Sym *)(elf_data + context->symtab_hdr->sh_offset);
            context->strtab = (const char *)(elf_data + context->strtab_hdr->sh_offset);

#ifdef DEBUG_ELF_LOADER
            uart_puts("Symbol table and string table found.\n");
            print_section_header(context->symtab_hdr, context->shstrtab_hdr, elf_data);
            print_section_header(context->strtab_hdr, context->shstrtab_hdr, elf_data);
            uart_puts("\n---\n");
            for (Elf64_Xword i = 0; i < context->strtab_hdr->sh_size; i++) {
                uart_putchar(context->strtab[i]);
            }
            uart_puts("\n---\n");
            for (Elf64_Xword i = 0; i < context->symtab_hdr->sh_size / sizeof(Elf64_Sym); i++) {
                print_symbol(context->symtab[i], context->strtab_hdr, elf_data);
            }
#endif

        } else {

#ifdef DEBUG_ELF_LOADER
            uart_puts("Warning: Symbol table or string table not found.\n");
#endif
            result = ELF_ERROR_SYMTAB_NOT_FOUND;
            goto failed;
        }

        // 检查段数量是否超过限制
        if (context->elf_hdr->e_shnum > MAX_ELF) {

#ifdef DEBUG_ELF_LOADER
            uart_puts("Error: Too many sections (");
            uart_puthex(context->elf_hdr->e_shnum);
            uart_puts("), maximum supported: ");
            uart_puthex(MAX_ELF);
            uart_puts("\n");
#endif

            result = ELF_ERROR_SECTION_NOT_FOUND;
            goto failed;
        }

        result = relo_load_sections(context);
        if (result != ELF_SUCCESS) {
            goto cleanup_sections;
        }

        // 处理重定位
        result = process_relocations(context);
        if (result != ELF_SUCCESS) {
            goto cleanup_sections;
        }

#ifdef DEBUG_ELF_LOADER
        print_code(context, section_memory);
        print_context(context);
#endif

        // 查找并执行main函数
        result = find_and_execute_main(context);
        if (result != ELF_SUCCESS) {
            goto cleanup_sections;
        }
    } else if (context->elf_hdr->e_type == ET_EXEC) {
        // 解析程序头表
        result = parse_program_headers(context);
        if (result != ELF_SUCCESS) {
            goto failed;
        }

#ifdef DEBUG_ELF_LOADER
        if (context->program_headers != NULL) {
            uart_puts("Program header table loaded successfully.\n");
        }
#endif
        result = exec_load_sections(context);
        if (result != ELF_SUCCESS) {
            goto cleanup_sections;
        }
#ifdef DEBUG_ELF_LOADER
        print_code(context, section_memory);
#endif
        result = find_and_execute_main(context);

        if (result != ELF_SUCCESS) {
            goto cleanup_sections;
        }
    }
cleanup_sections:
    // 清理分配的内存
    cleanup_loaded_sections(context->memory_pool_index);
    if (result == ELF_SUCCESS) {
        return context->result;
    } else {
        goto failed;
    }

failed:
    print_error(result);
    return result;
}