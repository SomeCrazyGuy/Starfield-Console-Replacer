#include "item_array.h"

#include "main.h"

#include <vector>


struct ItemVector {
        const char* name;
        unsigned char* data;
        uint32_t count;
        uint32_t capacity;
        uint32_t elim_size;
        //padding: 4 bytes
};


static std::vector<ItemVector> Items{};


static ItemArrayHandle ItemArrayCreate(const char* name, uint32_t element_size) {
        auto ret = Items.size();
        ItemVector obj;
        obj.capacity = 0;
        obj.count = 0;
        obj.elim_size = element_size;
        obj.name = name;
        obj.data = nullptr;
        Items.push_back(obj);
        return (uint32_t)ret;
}


static const char* ItemArrayName(ItemArrayHandle handle) {
        ASSERT(handle < Items.size());
        return Items[handle].name;
}


static uint32_t ItemArrayCount(ItemArrayHandle handle) {
        ASSERT(handle < Items.size());
        return Items[handle].count;
}


static void* ItemArrayData(ItemArrayHandle handle) {
        ASSERT(handle < Items.size());
        return (void*)Items[handle].data;
}


static void* ItemArrayAt(ItemArrayHandle handle, uint32_t index) {
        ASSERT(handle < Items.size());
        auto& item = Items[handle];
        ASSERT(item.data != nullptr);
        ASSERT(item.count > index);
        return (void*)(item.data + (item.elim_size * index));
}


static void ItemArrayClear(ItemArrayHandle handle) {
        ASSERT(handle < Items.size());
        Items[handle].count = 0;
}


static void ItemArrayFree(ItemArrayHandle handle) {
        ASSERT(handle < Items.size());
        auto& item = Items[handle];
        item.count = 0;
        item.capacity = 0;
        free(item.data);
        item.data = nullptr;
}


// Return the power of 2 that is larger than n
// via bit-twiddling hacks
static inline uint32_t bit_ceil(uint32_t n) {
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n++;
        return n;
}


static void ItemArrayAppend(ItemArrayHandle handle, const void* elements, uint32_t count) {
        ASSERT(handle < Items.size());
        auto& item = Items[handle];

        auto needed_capacity = bit_ceil(count + item.count);

        if (needed_capacity > item.capacity) {
                item.data = (unsigned char*)realloc(item.data, (item.elim_size * needed_capacity));
                ASSERT(item.data != nullptr);
                item.capacity = needed_capacity;
        }

        auto end = item.count;
        item.count += count;

        memcpy(ItemArrayAt(handle, end), elements, (count * item.elim_size));
}


static void ItemArrayPushBack(ItemArrayHandle handle, const void* element) {
        ItemArrayAppend(handle, element, 1);
}


static constexpr struct item_array_api_t ItemArray {
        ItemArrayCreate,
        ItemArrayName,
        ItemArrayCount,
        ItemArrayData,
        ItemArrayAt,
        ItemArrayPushBack,
        ItemArrayAppend,
        ItemArrayClear,
        ItemArrayFree
};


extern const struct item_array_api_t* GetItemArrayAPI() {
        return &ItemArray;
}