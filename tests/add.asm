[BITS 16]
add    al, 3
add    ax, 1024
add    cx, ax
mov    al, 0
out    0xFF, al