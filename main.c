#include "clok.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

// just a testing ground for the GC...
// and not a very good one; having a
// hard time coming up with test cases

typedef enum   Type   Type;
typedef struct Object Object;
typedef struct List   List;
typedef struct Root   Root;
typedef struct Int    Int;

enum Type
{
    TYPE_INT,
    TYPE_LIST,
    TYPE_ROOT
};

struct Object
{
    Type type;
};

struct Int
{
    Object obj;
    int    val;
};

struct List
{
    Object obj;
    void*  value;
    List*  next;
};

struct Root
{
    Object obj;
    Int*   int1;
    List*  list1;
    List*  list2;
    List*  list3;
    List*  list4;
};



Int*  intMake( ck_Pool* pool, void* owner, int val );
void  intExpire( Int* val );
void  intPreserve( ck_Pool* pool, Int* val );
List* listMake( ck_Pool* pool, void* owner, void* val, List* next );
void  listExpire( List* val );
void  listPreserve( ck_Pool* pool, List* val );
Root* rootMake( ck_Pool* pool );
void  rootExpire( Root* val );
void  rootPreserve( ck_Pool* pool, Root* val );
void* dAlloc( void* context, void* old, size_t size );
void  dFree( void* context, void* alloc );
void dExpire( void* context, void* alloc );
void dPreserve( void* context, void* alloc, ck_Pool* pool );

ck_Config config = { .quota    = 1000000,
                     .alloc    = &dAlloc,
                     .expire   = &dExpire,
                     .preserve = &dPreserve,
                     .context  = NULL };

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
        
        clock_t tstart = clock();
        ck_tick( pool );
        clock_t tend   = clock();
        unsigned usecs = (tend-tstart)*1000000000/CLOCKS_PER_SEC;
        printf( "Tick Time = %uus\n", usecs );
    }
    
    ck_freePool( pool );
}

void* dAlloc( void* context, void* old, size_t size )
{
    return realloc( old, size );
}

void dExpire( void* context, void* alloc )
{
    Object* obj = alloc;
    switch( obj->type )
    {
        case TYPE_INT:
            intExpire( (Int*) obj );
        break;
        case TYPE_LIST:
            listExpire( (List*) obj );
        break;
        case TYPE_ROOT:
            rootExpire( (Root*) obj );
        break;
    }
};

void dPreserve( void* context, void* alloc, ck_Pool* pool )
{
    Object* obj = alloc;
    switch( obj->type )
    {
        case TYPE_INT:
            intPreserve( pool, (Int*) obj );
        break;
        case TYPE_LIST:
            listPreserve( pool, (List*) obj );
        break;
        case TYPE_ROOT:
            rootPreserve( pool, (Root*) obj );
        break;
    }
}

Int* intMake( ck_Pool* pool, void* owner, int val )
{
    Int* i = ck_allocFat( pool, sizeof(*i), owner );
    assert(i);
    i->obj.type = TYPE_INT;
    i->val = val;
    
    printf( "Making Int 0x%llx\n", (unsigned long long) i );
    return i;
}


void intExpire( Int* val )
{
    printf( "Expiring Int 0x%llx\n", (unsigned long long) val );
}

void intPreserve( ck_Pool* pool, Int* val )
{
    printf( "Preserved Int 0x%llx\n", (unsigned long long) val );
}

List* listMake( ck_Pool* pool, void* owner, void* val, List* next )
{
    List* list = ck_allocFat( pool, sizeof(*list), owner );
    assert(list);
    list->obj.type = TYPE_LIST;
    list->value = val;
    list->next  = next;
    ck_ref( pool, val, list );
    ck_ref( pool, next, list );
    
    printf( "Making List 0x%llx\n", (unsigned long long) list );
    return list;
}

void listExpire( List* val )
{
    printf( "Expiring List 0x%llx\n", (unsigned long long) val );
}

void listPreserve( ck_Pool* pool, List* val )
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
    Root* root = ck_allocFat( pool, sizeof(*root), NULL );
    assert(root);
    root->obj.type = TYPE_ROOT;
    root->list1 = NULL;
    root->list2 = NULL;
    root->list3 = NULL; 
    root->list4 = NULL;
    
    printf( "Making Root 0x%llx\n", (unsigned long long) root );
    return root;
}

void rootExpire( Root* val )
{
    printf( "Expiring Root 0x%llx\n", (unsigned long long) val );
}

void rootPreserve( ck_Pool* pool, Root* val )
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

