# disktest.asm - Disk sector summation
# Reads sectors 0-7, sums element-wise, writes result to sector 8
# sector8[i] = sector0[i] + sector1[i] + ... + sector7[i]
#
# Memory: 0x200-0x5FF buffers for sectors 0-7, 0x600 for result
# Disk regs: 14=cmd (1=read,2=write), 15=sector, 16=buffer, 17=status
#
# Registers: $s0=sector, $s1=buffer addr, $s2=word index
#            $t0=loaded word, $t1=running sum, $t2=result buffer base (0x600)

main:
    # constants
    add $a2, $zero, $imm, 0x200     # base buffer address
    add $a1, $zero, $imm, 128       # words per sector
    add $a0, $zero, $imm, 8         # number of sectors

    # Phase 1: Read sectors 0-7 from disk
    add $s0, $zero, $zero, 0        # sector = 0
    add $s1, $a2, $zero, 0          # buffer = 0x200

read_sector_loop:
    bge $imm, $s0, $a0, read_done   # done if sector >= 8

wait_disk_read:
    in $v0, $zero, $imm, 17         # read diskstatus
    bne $imm, $v0, $zero, wait_disk_read  # poll until ready

    # issue read command
    out $s0, $zero, $imm, 15        # set sector number
    out $s1, $zero, $imm, 16        # set buffer address
    add $v0, $zero, $imm, 1         # cmd = 1 (read)
    out $v0, $zero, $imm, 14        # issue command

wait_read_complete:
    in $v0, $zero, $imm, 17         # read diskstatus
    bne $imm, $v0, $zero, wait_read_complete  # poll until done

    add $s0, $s0, $imm, 1           # sector++
    add $s1, $s1, $imm, 128         # buffer += 128
    beq $imm, $zero, $zero, read_sector_loop

read_done:
    # Phase 2: Sum element-wise across all 8 sectors
    add $t2, $a2, $imm, 0x400       # result buffer at 0x600
    add $s2, $zero, $zero, 0        # word index i = 0

sum_word_loop:
    bge $imm, $s2, $a1, sum_done    # done if i >= 128

    add $t1, $zero, $zero, 0        # sum = 0
    add $s0, $zero, $zero, 0        # sector = 0
    add $s1, $a2, $s2, 0            # addr = 0x200 + i (word i in sector 0)

sum_sector_loop:
    bge $imm, $s0, $a0, store_sum   # done if sector >= 8

    lw $t0, $s1, $zero, 0           # load word from current sector
    add $t1, $t1, $t0, 0            # sum += word
    add $s1, $s1, $imm, 128         # next sector buffer (+128)
    add $s0, $s0, $imm, 1           # sector++
    beq $imm, $zero, $zero, sum_sector_loop

store_sum:
    add $v0, $t2, $s2, 0            # result addr = 0x600 + i
    sw $t1, $v0, $zero, 0           # store sum
    add $s2, $s2, $imm, 1           # i++
    beq $imm, $zero, $zero, sum_word_loop

sum_done:
    # Phase 3: Write result to sector 8
wait_disk_write:
    in $v0, $zero, $imm, 17         # read diskstatus
    bne $imm, $v0, $zero, wait_disk_write  # poll until ready

    add $v0, $zero, $imm, 8         # sector = 8
    out $v0, $zero, $imm, 15        # set sector number
    out $t2, $zero, $imm, 16        # set buffer (0x600)
    add $v0, $zero, $imm, 2         # cmd = 2 (write)
    out $v0, $zero, $imm, 14        # issue command

wait_write_complete:
    in $v0, $zero, $imm, 17         # read diskstatus
    bne $imm, $v0, $zero, wait_write_complete  # poll until done

    halt $zero, $zero, $zero, 0
