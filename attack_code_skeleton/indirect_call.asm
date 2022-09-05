BITS 64


section .text
global indirect_call

indirect_call:
  mov rax, rdi
  call [rax]
  ret
