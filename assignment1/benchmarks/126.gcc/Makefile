BENCHMARK=126.gcc

.PHONY: validate all build compile run exec compare setup clear clean clobber

validate all:
	runspec $(BENCHMARK)

build compile:
	runspec -D -a build $(BENCHMARK)

run exec:
	runspec -a run $(BENCHMARK)

compare: 
	runspec -a compare $(BENCHMARK)

setup:	clear
	runspec -a setup $(BENCHMARK)

clear:	
	-rm -rf run/*

clean:  clear
	make -C src clean
	-rm -rf exe/base/* exe/peak/*
	
clobber:  clean
	-rm -rf result/*

