set terminal png
set output "jit.png"
plot "1-base.txt", "2-patch.txt", "3-patch-and-interval-tree.txt"
