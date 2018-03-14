CC = gcc
CFLAGS = -fsanitize=address

ALL:
	cppcheck packer.c
	$(CC) -o PeterPacker packer.c $(CFLAGS)
