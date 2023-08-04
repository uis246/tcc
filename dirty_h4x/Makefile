CC ?= gcc

all: bin/merger bin/grid
bin:
	@mkdir -p bin

bin/merger: merger/main.c bin
	$(CC) -Os -s merger/main.c -o bin/merger -lpng

bin/grid: grid/main.c bin
	$(CC) -Os -s grid/main.c -o bin/grid -lpng

