progenitor_organism.o: progenitor_organism.s
	nasm $< -o $@

elfbootstrap: empty.s
	nasm -f elf64 -o $@ $<

%.elf: %.o elfbootstrap
	objcopy --add-section sname=$< elfbootstrap $@

disassemble_%: %.elf
	objdump -D $<

environment: environment.c progenitor_organism.o
	gcc -g -lpthread -o $@ $<


