all: prompt

prompt: prompt.c mpc.c
	cc -std=c99 -Wall prompt.c mpc.c -ledit -lm -o prompt

clean:
	rm prompt *.o

