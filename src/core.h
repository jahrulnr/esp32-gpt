#pragma once
#include <WiFiClientSecure.h>

static WiFiClientSecure wifiClient;

struct GPTSpiAllocator : ArduinoJson::Allocator {
    uint32_t getMemoryType() const {
        return MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT;
    }

    /**
     * @brief Allocate memory from SPI RAM with fallback to internal RAM
     * @param size Size of memory to allocate
     * @return Pointer to allocated memory
     */
    void* allocate(size_t size) override {
        return heap_caps_malloc(size, getMemoryType());
    }

    /**
     * @brief Deallocate memory previously allocated
     * @param pointer Pointer to memory to free
     */
    void deallocate(void* pointer) override {
        heap_caps_free(pointer);
    }

    /**
     * @brief Reallocate memory block to different size
     * @param ptr Pointer to memory to reallocate
     * @param new_size New size for memory block
     * @return Pointer to reallocated memory
     */
    void* reallocate(void* ptr, size_t new_size) override {
        return heap_caps_realloc(ptr, new_size, getMemoryType());
    }

    /**
     * @brief Get singleton instance of the allocator
     * @return SpiAllocator* Pointer to the singleton allocator
     */
    static GPTSpiAllocator* instance() {
        static GPTSpiAllocator instance;
        return &instance;
    }
};

class GPTSpiJsonDocument : public ArduinoJson::JsonDocument {
public:
    /**
     * @brief Construct a new SpiJsonDocument
     */
    GPTSpiJsonDocument() : ArduinoJson::JsonDocument(GPTSpiAllocator::instance()) {
        // In ArduinoJson 7, the capacity is managed dynamically
    }

    /**
     * @brief Construct from a JsonVariant
     * @param src The JsonVariant to copy
     */
    GPTSpiJsonDocument(const ArduinoJson::JsonVariant& src) 
        : ArduinoJson::JsonDocument(GPTSpiAllocator::instance()) {
        this->set(src);
    }
    
    /**
     * @brief Get the capacity of the document (for backward compatibility)
     * @return The capacity in bytes
     */
    size_t capacity() const {
        return this->size();
    }
};