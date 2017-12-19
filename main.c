#include "clok.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// just a testing ground for the GC...
// and not a very good one; having a
// hard time coming up with test cases

typedef struct List List;
typedef struct Root Root;
typedef struct Int  Int;

struct Int
{
    int val;
};

struct List
{
    void* value;
    List* next;
};

struct Root
{
    Int*  int1;
    List* list1;
    List* list2;
    List* list3;
    List* list4;
};



Int*  intMake( ck_Pool* pool, void* owner, int val );
void  intExpire( ck_Pool* pool, void* val );
void  intPreserve( ck_Pool* pool, void* val );
List* listMake( ck_Pool* pool, void* owner, void* val, List* next );
void  listExpire( ck_Pool* pool, void* val );
void  listPreserve( ck_Pool* pool, void* val );
Root* rootMake( ck_Pool* pool );
void  rootExpire( ck_Pool* pool, void* val );
void  rootPreserve( ck_Pool* pool, void* val );
void* dAlloc( void* context, size_t size );
void  dFree( void* context, void* alloc );


ck_CbSet intCallbacks  = { &intExpire,  &intPreserve };
ck_CbSet listCallbacks = { &listExpire, &listPreserve };
ck_CbSet rootCallbacks = { &rootExpire, &rootPreserve };

ck_Config config = { 1000000, &dAlloc, &dFree, NULL };

int main( void )
{
    ck_Pool* pool   = ck_makePool( &config );
    unsigned  count  = 0;
    Root*     root   = rootMake( pool );
    
    //root->list1 = listMake( pool, root, 0, NULL );
    while( count++ < 100000 )
    {
        root->int1    = intMake( pool, root, count );
        root->list1   = listMake( pool, root, root->int1, NULL );
        root->list2   = listMake( pool, root, root->int1, root->list1 );
        root->list1->next = root->list2;
        ck_ref( pool, root->list2, root->list1 );
        ck_step( pool );
    }
    
    //ck_freePool( pool );
}

void* dAlloc( void* context, size_t size )
{
    return malloc(size);
}

void dFree( void* context, void* alloc )
{
    return free(alloc);
}

Int* intMake( ck_Pool* pool, void* owner, int val )
{
    Int* i = ck_allocFat( pool, sizeof(*i),
                            owner, &intCallbacks );
    assert(i);
    i->val = val;
    
    printf( "Making Int 0x%llx\n", (unsigned long long) i );
    return i;
}


void intExpire( ck_Pool* pool, void* val )
{
    printf( "Expiring Int 0x%llx\n", (unsigned long long) val );
}

void intPreserve( ck_Pool* pool, void* val )
{
    printf( "Preserved Int 0x%llx\n", (unsigned long long) val );
}

List* listMake( ck_Pool* pool, void* owner, void* val, List* next )
{
    List* list = ck_allocFat( pool, sizeof(*list),
                                owner, &listCallbacks );
    assert(list);
    list->value = val;
    list->next  = next;
    ck_ref( pool, val, list );
    ck_ref( pool, next, list );
    
    printf( "Making List 0x%llx\n", (unsigned long long) list );
    return list;
}

void listExpire( ck_Pool* pool, void* val )
{
    printf( "Expiring List 0x%llx\n", (unsigned long long) val );
}

void listPreserve( ck_Pool* pool, void* val )
{
    List* list = val;
    ck_ref( pool, list->value, list );
    ck_ref( pool, list->next, list );
    printf( "Preserved List 0x%llx\n", (unsigned long long) val );
    printf( "    -> %llx\n", (unsigned long long) list->value );
    printf( "    -> %llx\n", (unsigned long long) list->next );
}

Root* rootMake( ck_Pool* pool )
{
    Root* root = ck_allocFat( pool, sizeof(*root),
                               NULL, &rootCallbacks );
    assert(root);
    root->list1 = NULL;
    root->list2 = NULL;
    root->list3 = NULL; 
    root->list4 = NULL;
    
    printf( "Making Root 0x%llx\n", (unsigned long long) root );
    return root;
}

void rootExpire( ck_Pool* pool, void* val )
{
    printf( "Expiring Root 0x%llx\n", (unsigned long long) val );
}

void rootPreserve( ck_Pool* pool, void* val )
{
    Root* root = val;
    ck_ref( pool, root->int1, root );
    ck_ref( pool, root->list1, root );
    ck_ref( pool, root->list2, root );
    ck_ref( pool, root->list3, root );
    ck_ref( pool, root->list4, root );
    
    printf( "Preserving Root 0x%llx\n", (unsigned long long) val );  
    printf( "    -> %llx\n", (unsigned long long) root->list1 );
    printf( "    -> %llx\n", (unsigned long long) root->list2 );
    printf( "    -> %llx\n", (unsigned long long) root->list3 );
    printf( "    -> %llx\n", (unsigned long long) root->list4 );
    printf( "    -> %llx\n", (unsigned long long) root->int1 );
}

