
all : test
test: ../my_vm.h
	gcc -g test.c -L../ -lmy_vm -g -m32 -o test -lm -lpthread -lrt
	gcc multi_test.c -L../ -lmy_vm -m32 -o mtest -lpthread -lm -lrt

clean:
	rm -rf test mtest op.txt
