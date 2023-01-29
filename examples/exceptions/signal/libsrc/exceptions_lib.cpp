/*
 * This is the source code of SpecNet project
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: specnet.messenger@gmail.com
 */

#define BUILDING_LIB

#include "exceptions_lib.h"

#include <stdexcept>

bool f3SIGSEGV()
{
  int *p = (int *)0xdeadbeef;
  *p += 10;  /* CRASH here!! */
  return *p == 100500;
}

bool f2SIGSEGV()
{
    return f3SIGSEGV();
}

bool f1SIGSEGV()
{
    return f2SIGSEGV();
}

bool createSIGSEGV()
{
    f1SIGSEGV();
    return true;
}


void f1SIGABRT()
{
    throw std::runtime_error("Oh, runtime_error error");
}

bool createSIGABRT()
{
    f1SIGABRT();
    return true;
}
