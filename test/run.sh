for f in  {2..6}
do
echo 
echo === forground $f ===
(taskset -c 0 ./test $f & taskset -c 2 ./test 4)
sleep 1
done
