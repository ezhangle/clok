/*
 * MIT License
 * 
 * Copyright (c) 2017 Ray Stubbs
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef clok_h
#define clok_h
#include <stddef.h>

typedef struct ck_Pool  ck_Pool;
typedef struct ck_CbSet ck_CbSet;
typedef struct ck_Config ck_Config;

struct ck_CbSet
{
    void (*expire)( ck_Pool* pool, void* block );
    void (*preserve)( ck_Pool* pool, void* block );
};

struct ck_Config
{
    size_t   quota;
    void*    (*alloc)( void* context, size_t size );
    void     (*free)( void* context, void* alloc );
    void*    context;
};

ck_Pool*
ck_makePool( ck_Config const* config );

void
ck_freePool( ck_Pool* pool );

void*
ck_allocSlim( ck_Pool* pool, unsigned size, void*    owner );

void* 
ck_allocFat( ck_Pool* pool, unsigned size,
             void* owner, ck_CbSet const* cbset );

void*
ck_ref( ck_Pool* pool, void* alloc, void* owner );

void
ck_tick( ck_Pool* pool );

void
ck_step( ck_Pool* pool );

size_t
ck_avail( ck_Pool* pool );

size_t
ck_used( ck_Pool* pool );

#endif
