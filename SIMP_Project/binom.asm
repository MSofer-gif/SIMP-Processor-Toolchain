# binom.asm - Recursive Binomial Coefficient
# Calculates C(n,k) = C(n-1,k-1) + C(n-1,k)
# Input: n at 0x100, k at 0x101 | Output: result at 0x102

# Registers: $a0/$a1 = n/k args, $s0/$s1/$s2 = saved across calls

main:
    add $sp, $zero, $imm, 0xFFF     # init stack pointer
    lw $a0, $zero, $imm, 0x100      # load n
    lw $a1, $zero, $imm, 0x101      # load k
    jal $ra, $imm, $zero, binom     # call binom(n, k)
    sw $v0, $zero, $imm, 0x102      # store result
    halt $zero, $zero, $zero, 0

# binom(n, k) - recursive binomial coefficient
binom:
    # prologue - save $ra and $s0-$s2
    sub $sp, $sp, $imm, 4
    sw $ra, $sp, $imm, 3
    sw $s0, $sp, $imm, 2
    sw $s1, $sp, $imm, 1
    sw $s2, $sp, $imm, 0

    add $s0, $a0, $zero, 0          # save n
    add $s1, $a1, $zero, 0          # save k

    # base case: k == 0 -> return 1
    bne $imm, $a1, $zero, check_n_eq_k
    add $v0, $zero, $imm, 1
    beq $imm, $zero, $zero, binom_epilogue

check_n_eq_k:
    # base case: n == k -> return 1
    bne $imm, $a0, $a1, recursive_step
    add $v0, $zero, $imm, 1
    beq $imm, $zero, $zero, binom_epilogue

recursive_step:
    # first call: binom(n-1, k-1)
    sub $a0, $s0, $imm, 1
    sub $a1, $s1, $imm, 1
    jal $ra, $imm, $zero, binom
    add $s2, $v0, $zero, 0          # save first result

    # second call: binom(n-1, k)
    sub $a0, $s0, $imm, 1
    add $a1, $s1, $zero, 0          # restore k
    jal $ra, $imm, $zero, binom

    add $v0, $s2, $v0, 0            # combine: result1 + result2

binom_epilogue:
    # epilogue - restore and return
    lw $s2, $sp, $imm, 0
    lw $s1, $sp, $imm, 1
    lw $s0, $sp, $imm, 2
    lw $ra, $sp, $imm, 3
    add $sp, $sp, $imm, 4
    beq $ra, $zero, $zero, 0        # return (jump to $ra)

# test data
.word 0x100 5       # n = 5
.word 0x101 2       # k = 2
# expected: C(5,2) = 10 at 0x102
