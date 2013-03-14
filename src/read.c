/*
 * read.c
 *
 * Read and parse S-exp
 *
 * Copyright (C) 2013-03-13 liutos <mat.liutos@gmail.com>
 */
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

lisp_object_t make_fixnum(int value) {
  lisp_object_t fixnum = malloc(sizeof(struct lisp_object_t));
  fixnum->type = FIXNUM;
  fixnum->values.fixnum.value = value;
  return fixnum;
}

lisp_object_t make_eof_object(void) {
  lisp_object_t eof_object = malloc(sizeof(struct lisp_object_t));
  eof_object->type = EOF_OBJECT;
  return eof_object;
}

lisp_object_t read_fixnum(char c, FILE *stream) {
  int number = c - '0';
  int digit;
  while (isdigit(digit = fgetc(stream))) {
    number = number * 10 + digit - '0';
  }
  return make_fixnum(number);
}

void read_comment(FILE *stream) {
  int c = fgetc(stream);
  while (c != '\n' && c != EOF)
    c = fgetc(stream);
}

lisp_object_t read_object(FILE *stream) {
  int c = fgetc(stream);
  switch (c) {
    case EOF: return make_eof_object();
    case ' ':
    case '\t':
    case '\n':
    case '\r': return read_object(stream);
    case ';': read_comment(stream); return read_object(stream);
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return read_fixnum(c, stream);
    default :
      fprintf(stderr, "unexpected token '%c'\n", c);
      exit(1);
  }
}
