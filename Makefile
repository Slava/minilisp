all: prompt

prompt: prompt.c
	cc -std=c99 -Wall prompt.c -ledit -o prompt

