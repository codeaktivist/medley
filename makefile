# Using GNU Compiler
CC=gcc

# build medley
medley: medley.c
	@CC -o medley medley.c