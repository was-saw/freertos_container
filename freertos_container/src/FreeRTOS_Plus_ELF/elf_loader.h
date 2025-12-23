#ifndef ELF_LOADER_H
#define ELF_LOADER_H
#include <stddef.h>
#include <stdint.h>


// ELF 魔数常量
#define ELF_MAGIC_0 0x7f
#define ELF_MAGIC_1 'E'
#define ELF_MAGIC_2 'L'
#define ELF_MAGIC_3 'F'
#define ELF_CLASS_64 2
#define ELF_DATA_LSB 1
#define ELF_VERSION_1 1
#define ELF_TYPE_REL 1
#define ELF_MACHINE_AARCH64 183

// 错误代码定义
#define ELF_OOM -2
#define ELF_NOT_RUN -1
#define ELF_SUCCESS 0
#define ELF_ERROR_NULL_POINTER -1
#define ELF_ERROR_INVALID_MAGIC -2
#define ELF_ERROR_INVALID_CLASS -3
#define ELF_ERROR_INVALID_ENDIAN -4
#define ELF_ERROR_INVALID_VERSION -5
#define ELF_ERROR_INVALID_TYPE -6
#define ELF_ERROR_INVALID_MACHINE -7
#define ELF_ERROR_SECTION_NOT_FOUND -8
#define ELF_ERROR_SYMTAB_NOT_FOUND -9
#define ELF_ERROR_STRTAB_NOT_FOUND -10
#define ELF_ERROR_RELOCATION_FAILED -11
#define ELF_ERROR_PROGRAM_NOT_FOUND -12

/* 64-bit ELF base types. */
typedef uint64_t Elf64_Addr;
typedef uint16_t Elf64_Half;
typedef int16_t  Elf64_SHalf;
typedef uint64_t Elf64_Off;
typedef int32_t  Elf64_Sword;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;
typedef uint16_t Elf64_Versym;

#define EI_NIDENT 16
#define EM_AARCH64 183 /* ARM 64 bit */

/* These constants define the different elf file types */
#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff

/* This info is needed when parsing the symbol table */
#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2

#define STN_UNDEF 0

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4
#define STT_COMMON 5
#define STT_TLS 6

#define VER_FLG_BASE 0x1
#define VER_FLG_WEAK 0x2

#define ELF_ST_BIND(x) ((x) >> 4)
#define ELF_ST_TYPE(x) ((x) & 0xf)
#define ELF32_ST_BIND(x) ELF_ST_BIND(x)
#define ELF32_ST_TYPE(x) ELF_ST_TYPE(x)
#define ELF64_ST_BIND(x) ELF_ST_BIND(x)
#define ELF64_ST_TYPE(x) ELF_ST_TYPE(x)

/* The following are used with relocations */
#define ELF32_R_SYM(x) ((x) >> 8)
#define ELF32_R_TYPE(x) ((x) & 0xff)

#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffff)

typedef struct elf64_rel {
    Elf64_Addr  r_offset; /* Location at which to apply the action */
    Elf64_Xword r_info;   /* index and type of relocation */
} Elf64_Rel;


typedef struct elf64_rela {
    Elf64_Addr   r_offset; /* Location at which to apply the action */
    Elf64_Xword  r_info;   /* index and type of relocation */
    Elf64_Sxword r_addend; /* Constant addend used to compute value */
} Elf64_Rela;

typedef struct elf64_sym {
    Elf64_Word    st_name;  /* Symbol name, index in string tbl */
    unsigned char st_info;  /* Type and binding attributes */
    unsigned char st_other; /* No defined meaning, 0 */
    Elf64_Half    st_shndx; /* Associated section index */
    Elf64_Addr    st_value; /* Value of the symbol */
    Elf64_Xword   st_size;  /* Associated symbol size */
} Elf64_Sym;

typedef struct elf64_hdr {
    unsigned char e_ident[EI_NIDENT]; /* ELF "magic number" */
    Elf64_Half    e_type;
    Elf64_Half    e_machine;
    Elf64_Word    e_version;
    Elf64_Addr    e_entry; /* Entry point virtual address */
    Elf64_Off     e_phoff; /* Program header table file offset */
    Elf64_Off     e_shoff; /* Section header table file offset */
    Elf64_Word    e_flags;
    Elf64_Half    e_ehsize;
    Elf64_Half    e_phentsize;
    Elf64_Half    e_phnum;
    Elf64_Half    e_shentsize;
    Elf64_Half    e_shnum;
    Elf64_Half    e_shstrndx;
} Elf64_Ehdr;

/* These constants define the permissions on sections in the program
   header, p_flags. */
#define PF_R 0x4
#define PF_W 0x2
#define PF_X 0x1

/* These constants are for the segment types stored in the image headers */
#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_TLS 7           /* Thread local storage segment */
#define PT_LOOS 0x60000000 /* OS-specific */
#define PT_HIOS 0x6fffffff /* OS-specific */
#define PT_LOPROC 0x70000000
#define PT_HIPROC 0x7fffffff
#define PT_GNU_EH_FRAME (PT_LOOS + 0x474e550)
#define PT_GNU_STACK (PT_LOOS + 0x474e551)
#define PT_GNU_RELRO (PT_LOOS + 0x474e552)
#define PT_GNU_PROPERTY (PT_LOOS + 0x474e553)


typedef struct elf64_phdr {
    Elf64_Word  p_type;
    Elf64_Word  p_flags;
    Elf64_Off   p_offset; /* Segment file offset */
    Elf64_Addr  p_vaddr;  /* Segment virtual address */
    Elf64_Addr  p_paddr;  /* Segment physical address */
    Elf64_Xword p_filesz; /* Segment size in file */
    Elf64_Xword p_memsz;  /* Segment size in memory */
    Elf64_Xword p_align;  /* Segment alignment, file & memory */
} Elf64_Phdr;

/* sh_type */
#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11
#define SHT_NUM 12
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7fffffff
#define SHT_LOUSER 0x80000000
#define SHT_HIUSER 0xffffffff

/* sh_flags */
#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4
#define SHF_MERGE 0x10
#define SHF_STRINGS 0x20
#define SHF_INFO_LINK 0x40
#define SHF_LINK_ORDER 0x80
#define SHF_OS_NONCONFORMING 0x100
#define SHF_GROUP 0x200
#define SHF_TLS 0x400
#define SHF_RELA_LIVEPATCH 0x00100000
#define SHF_RO_AFTER_INIT 0x00200000
#define SHF_ORDERED 0x04000000
#define SHF_EXCLUDE 0x08000000
#define SHF_MASKOS 0x0ff00000
#define SHF_MASKPROC 0xf0000000

/* special section indexes */
#define SHN_UNDEF 0
#define SHN_LORESERVE 0xff00
#define SHN_LOPROC 0xff00
#define SHN_HIPROC 0xff1f
#define SHN_LIVEPATCH 0xff20
#define SHN_ABS 0xfff1
#define SHN_COMMON 0xfff2
#define SHN_HIRESERVE 0xffff

typedef struct elf64_shdr {
    Elf64_Word  sh_name;      /* Section name, index in string tbl */
    Elf64_Word  sh_type;      /* Type of section */
    Elf64_Xword sh_flags;     /* Miscellaneous section attributes */
    Elf64_Addr  sh_addr;      /* Section virtual addr at execution */
    Elf64_Off   sh_offset;    /* Section file offset */
    Elf64_Xword sh_size;      /* Size of section in bytes */
    Elf64_Word  sh_link;      /* Index of another section */
    Elf64_Word  sh_info;      /* Additional section information */
    Elf64_Xword sh_addralign; /* Section alignment */
    Elf64_Xword sh_entsize;   /* Entry size if section holds table */
} Elf64_Shdr;

#define EI_MAG0 0 /* e_ident[] indexes */
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_PAD 8

#define ELFMAG0 0x7f /* EI_MAG */
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ELFMAG "\177ELF"
#define SELFMAG 4

#define ELFCLASSNONE 0 /* EI_CLASS */
#define ELFCLASS32 1
#define ELFCLASS64 2
#define ELFCLASSNUM 3

#define ELFDATANONE 0 /* e_ident[EI_DATA] */
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define EV_NONE 0 /* e_version, EI_VERSION */
#define EV_CURRENT 1
#define EV_NUM 2

#define elfhdr elf64_hdr
// #define elf_phdr	elf64_phdr
#define elf_shdr elf64_shdr
#define elf_note elf64_note
#define elf_addr_t Elf64_Off
#define Elf_Half Elf64_Half
#define Elf_Word Elf64_Word


/*
 * AArch64 static relocation types.
 */

/* Miscellaneous. */
#define R_ARM_NONE 0
#define R_AARCH64_NONE 256

/* Data. */
#define R_AARCH64_ABS64 257
#define R_AARCH64_ABS32 258
#define R_AARCH64_ABS16 259
#define R_AARCH64_PREL64 260
#define R_AARCH64_PREL32 261
#define R_AARCH64_PREL16 262

/* Instructions. */
#define R_AARCH64_MOVW_UABS_G0 263
#define R_AARCH64_MOVW_UABS_G0_NC 264
#define R_AARCH64_MOVW_UABS_G1 265
#define R_AARCH64_MOVW_UABS_G1_NC 266
#define R_AARCH64_MOVW_UABS_G2 267
#define R_AARCH64_MOVW_UABS_G2_NC 268
#define R_AARCH64_MOVW_UABS_G3 269

#define R_AARCH64_MOVW_SABS_G0 270
#define R_AARCH64_MOVW_SABS_G1 271
#define R_AARCH64_MOVW_SABS_G2 272

#define R_AARCH64_LD_PREL_LO19 273
#define R_AARCH64_ADR_PREL_LO21 274
#define R_AARCH64_ADR_PREL_PG_HI21 275
#define R_AARCH64_ADR_PREL_PG_HI21_NC 276
#define R_AARCH64_ADD_ABS_LO12_NC 277
#define R_AARCH64_LDST8_ABS_LO12_NC 278

#define R_AARCH64_TSTBR14 279
#define R_AARCH64_CONDBR19 280
#define R_AARCH64_JUMP26 282
#define R_AARCH64_CALL26 283
#define R_AARCH64_LDST16_ABS_LO12_NC 284
#define R_AARCH64_LDST32_ABS_LO12_NC 285
#define R_AARCH64_LDST64_ABS_LO12_NC 286
#define R_AARCH64_LDST128_ABS_LO12_NC 299

#define R_AARCH64_MOVW_PREL_G0 287
#define R_AARCH64_MOVW_PREL_G0_NC 288
#define R_AARCH64_MOVW_PREL_G1 289
#define R_AARCH64_MOVW_PREL_G1_NC 290
#define R_AARCH64_MOVW_PREL_G2 291
#define R_AARCH64_MOVW_PREL_G2_NC 292
#define R_AARCH64_MOVW_PREL_G3 293

#define R_AARCH64_LDST128_ABS_LO12_NC 299

// GOT related relocations
#define R_AARCH64_GOT_LD_PREL19 309
#define R_AARCH64_LD64_GOTOFF_LO15 310
#define R_AARCH64_ADR_GOT_PAGE 311
#define R_AARCH64_LD64_GOT_LO12_NC 312
#define R_AARCH64_LD64_GOTPAGE_LO15 313

#define R_AARCH64_RELATIVE 1027


// 静态内存池定义
#define MAX_ELF 16
#define ELF_MEMORY_SIZE (64 * 1024) // 每个段最大64KB

typedef struct Elf_Load_Context {
    const uint8_t    *elf_data;
    size_t            elf_size;
    const Elf64_Ehdr *elf_hdr;
    const Elf64_Shdr *section_headers;
    const Elf64_Phdr *program_headers;
    const char       *shstrtab;
    const Elf64_Shdr *shstrtab_hdr;
    const Elf64_Shdr *symtab_hdr;
    const Elf64_Shdr *strtab_hdr;
    const Elf64_Sym  *symtab;
    const char       *strtab;
    const Elf64_Rela *rela;
    int               memory_pool_index;
    Elf64_Addr        load_sections[MAX_ELF];
    size_t            memory_size;
    int               result;
} Elf64_Ctx;

typedef struct {
    const uint8_t *elf_data;
    size_t         elf_size;
} ELF_WRAP;

// 加载 ELF 文件并执行 main 函数
int elf_load_and_run(const uint8_t *elf_data, size_t elf_size);
#endif // ELF_LOADER_H
