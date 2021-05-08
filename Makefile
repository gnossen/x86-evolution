progenitor_organism.o: progenitor_organism.s
	nasm $< -o $@

progenitor_organism.elf: progenitor_organism.s
	nasm -f elf64 $< -o $@

environment: environment.c progenitor_organism.o
	gcc -g -o $@ $<
