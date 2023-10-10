#ifndef ITEMARRAY_STB_H
#define ITEMARRAY_STB_H

#include <stdint.h>
#include <stdlib.h>

#define BETTERAPI_ENABLE_ASSERTIONS
#ifdef BETTERAPI_ENABLE_ASSERTIONS
#include <stdio.h>
#include <Windows.h>
#define ASSERT(X) do { if((X)) break; char msg[1024]; snprintf(msg, sizeof(msg), "FILE: %s\nFUNC: %s:%u\n%s", __FILE__, __func__, __LINE__, #X); MessageBoxA(NULL, msg, "Assertion Failure", 0); exit(EXIT_FAILURE); }while(0)
#endif // BETTERAPI_ENABLE_ASSERTIONS


typedef struct item_array_t {
        char* data;
        uint32_t count;
        uint32_t capacity;
        uint32_t element_size;
} ItemArray;


inline static ItemArray ItemArray_Create(uint32_t element_size) {
        ASSERT(element_size > 0);
        ItemArray ret = {};
        ret.element_size = element_size;
        return ret;
}


inline static void ItemArray_Clear(ItemArray* items) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        items->count = 0;
}


inline static uint32_t ItemArray_Count(ItemArray* items) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        return items->count;
}


inline static void ItemArray_Free(ItemArray* items) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        free(items->data);
        items->count = 0;
        items->data = NULL;
}


inline static void* ItemArray_At(ItemArray* items, uint32_t index) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        ASSERT(items->data != NULL);
        ASSERT(items->count > index);
        return (void*)(items->data + (items->element_size * index));
}


inline static void ItemArray_Append(ItemArray* items, void* items_array, uint32_t items_count) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        ASSERT(items->count > 0);

        uint32_t capacity_needed = items_count + items->count;

        if (capacity_needed > items->capacity) {
                uint32_t n = capacity_needed;
                
                /* find the power of 2 larger than n for a 32-bit n */
                n--;
                n |= n >> 1;
                n |= n >> 2;
                n |= n >> 4;
                n |= n >> 8;
                n |= n >> 16;
                n++;

                ASSERT(n > items->capacity);
                items->data = (char*)realloc(items->data, n);
                ASSERT(items->data != NULL);

                items->capacity = n;
        }

        char* end = (items->data + (items->element_size * items->count));
        items->count += items_count;
        memcpy(end, items_array, (items->element_size * items_count));
}


inline static void ItemArray_PushBack(ItemArray* items, void* item) {
        ItemArray_Append(items, item, 1);
}

#endif //ITEMARRAY_STB_H
