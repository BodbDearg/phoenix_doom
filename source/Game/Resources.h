#pragma once

#include "Base/Macros.h"
#include <cstddef>
#include <cstdint>

struct Resource;

BEGIN_NAMESPACE(Resources)

void init() noexcept;
void shutdown() noexcept;
    
const Resource* get(const uint32_t num) noexcept;
std::byte* getData(const uint32_t num) noexcept;

const Resource* load(const uint32_t num) noexcept;
std::byte* loadData(const uint32_t num) noexcept;

void free(const uint32_t num) noexcept;
void release(const uint32_t num) noexcept;

END_NAMESPACE(Resources)
