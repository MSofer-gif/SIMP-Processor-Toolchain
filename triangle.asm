# triangle.asm - Draw filled right triangle on 256x256 monitor
#
# Vertices loaded from memory:
#   0x100: A (top-left)      0x101: B (right angle)      0x102: C (bottom-right)
#   Address format: y * 256 + x
#
# Algorithm: For each row from y_A to y_B, draw horizontal line from
#            x_A to interpolated x on hypotenuse AC
#
# Registers:
#   $s0 = x_A, $s1 = y_A, $s2 = y_B, $a0 = x_C
#   $a1 = delta_x, $a2 = delta_y
#   $t0 = current y, $t1 = current x, $t2 = x_right for this row

main:
    # Load and extract vertex coordinates
    lw $v0, $zero, $imm, 0x100      # vertex A
    and $s0, $v0, $imm, 0xFF        # x_A = lower 8 bits
    srl $s1, $v0, $imm, 8
    and $s1, $s1, $imm, 0xFF        # y_A = bits 8-15

    lw $v0, $zero, $imm, 0x101      # vertex B
    srl $s2, $v0, $imm, 8
    and $s2, $s2, $imm, 0xFF        # y_B (x_B = x_A since AB vertical)

    lw $v0, $zero, $imm, 0x102      # vertex C
    and $a0, $v0, $imm, 0xFF        # x_C (y_C = y_B since BC horizontal)

    # Interpolation params
    sub $a1, $a0, $s0, 0            # delta_x = x_C - x_A
    sub $a2, $s2, $s1, 0            # delta_y = y_B - y_A
    beq $imm, $a2, $zero, end_program   # no height = nothing to draw

    add $t0, $s1, $zero, 0          # y = y_A

# Row loop: draw each scanline from y_A to y_B
row_loop:
    bgt $imm, $t0, $s2, end_program # done if y > y_B

    # Calculate x_right via linear interpolation
    # x_right = x_A + (delta_x * (y - y_A)) / delta_y
    sub $v0, $t0, $s1, 0            # y - y_A
    mul $v0, $a1, $v0, 0            # delta_x * (y - y_A)

    # Division by repeated subtraction (no div instruction in SIMP)
    add $t2, $zero, $zero, 0        # quotient = 0

divide_loop:
    blt $imm, $v0, $a2, divide_done
    sub $v0, $v0, $a2, 0
    add $t2, $t2, $imm, 1
    beq $imm, $zero, $zero, divide_loop

divide_done:
    add $t2, $s0, $t2, 0            # x_right = x_A + quotient
    add $t1, $s0, $zero, 0          # x = x_A (start of line)

# Column loop: draw pixels from x_A to x_right
col_loop:
    bgt $imm, $t1, $t2, next_row

    # Write pixel to monitor
    sll $gp, $t0, $imm, 8           # addr = y * 256
    add $gp, $gp, $t1, 0            # addr += x
    out $gp, $zero, $imm, 20        # monitoraddr
    add $v0, $zero, $imm, 255
    out $v0, $zero, $imm, 21        # monitordata = white
    add $v0, $zero, $imm, 1
    out $v0, $zero, $imm, 22        # monitorcmd = write

    add $t1, $t1, $imm, 1           # x++
    beq $imm, $zero, $zero, col_loop

next_row:
    add $t0, $t0, $imm, 1           # y++
    beq $imm, $zero, $zero, row_loop

end_program:
    halt $zero, $zero, $zero, 0

# Test data: triangle from (20,10) to (20,60) to (80,60)
.word 0x100 0x0A14  # A: (20, 10)
.word 0x101 0x3C14  # B: (20, 60)
.word 0x102 0x3C50  # C: (80, 60)
