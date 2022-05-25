set terminal png
set output "jit.png"
plot "perftest-1-base.txt", "perftest-2-patch.txt"
