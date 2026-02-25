# sort.asm - Bubble sort 16 numbers at 0x100-0x10F
#
# Registers:
#   $s0: outer counter (i)    $s1: inner counter (j)    $s2: base addr (0x100)
#   $t0: arr[j]               $t1: arr[j+1]             $t2: current address
#   $a0: outer limit (15)     $a1: inner limit (15-i)

main:
    add $s2, $zero, $imm, 0x100     # base address
    add $a0, $zero, $imm, 15        # outer limit = n-1
    add $s0, $zero, $zero, 0        # i = 0

# Outer loop: each pass bubbles largest unsorted element up
outer_loop:
    bge $imm, $s0, $a0, end_sort    # done when i >= 15
    sub $a1, $a0, $s0, 0            # inner limit = 15 - i
    add $s1, $zero, $zero, 0        # j = 0

# Inner loop: compare adjacent pairs, swap if out of order
inner_loop:
    bge $imm, $s1, $a1, next_i      # done with this pass when j >= limit
    add $t2, $s2, $s1, 0            # addr of arr[j]
    lw $t0, $t2, $zero, 0           # load arr[j]
    lw $t1, $t2, $imm, 1            # load arr[j+1]
    ble $imm, $t0, $t1, no_swap     # skip if already in order

    # swap arr[j] and arr[j+1]
    sw $t1, $t2, $zero, 0
    sw $t0, $t2, $imm, 1

no_swap:
    add $s1, $s1, $imm, 1           # j++
    beq $imm, $zero, $zero, inner_loop

next_i:
    add $s0, $s0, $imm, 1           # i++
    beq $imm, $zero, $zero, outer_loop

end_sort:
    halt $zero, $zero, $zero, 0

# Test data at 0x100-0x10F (unsorted)
.word 0x100 42
.word 0x101 17
.word 0x102 89
.word 0x103 3
.word 0x104 56
.word 0x105 99
.word 0x106 12
.word 0x107 67
.word 0x108 33
.word 0x109 8
.word 0x10A 71
.word 0x10B 25
.word 0x10C 44
.word 0x10D 1
.word 0x10E 88
.word 0x10F 50

# Expected sorted output: 1, 3, 8, 12, 17, 25, 33, 42, 44, 50, 56, 67, 71, 88, 89, 99
