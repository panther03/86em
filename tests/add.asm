[BITS 16]
add    al, 3
jz fail
add    ax, 1024
jc fail
add    cx, ax

mov al, 0 
out 0xFF, al

fail:
mov    al, 1
out    0xFF, al