.section .rodata

/* Ensure the data is aligned to an 8-byte boundary for x86_64 */
.align 8

.global json_start
.global json_end
.global json_size

json_start:
    .incbin "benchmark.json"
    //.byte '\0'                 /* null terminator '\0' */
json_end:

json_size:
    .quad json_end - json_start