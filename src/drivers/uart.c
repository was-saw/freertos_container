#include "xil_printf.h"
void uart_puts(const char *str) {
    xil_printf(str);
}

void uart_puthex(uint64_t v) {
    xil_printf("%lx", v);
}

void uart_putchar(uint8_t c) {
    xil_printf("%c", c);    
}

void uart_putcharex(uint64_t v) {
    xil_printf("%lx", v);
}