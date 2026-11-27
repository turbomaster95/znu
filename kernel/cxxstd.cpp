#include <cxx/basic>
#include <cxx/typeinfo>

__BEGIN_DECLS
#include <stddef.h>
#include <stdint.h>
#include <page.h>
#include <stdlib.h>

void* __dso_handle = nullptr;

int __cxa_atexit(void (*)(void*), void*, void*) {
    return 0;
}

int atexit(void (*)(void)) {
    return 0;
}

void __cxa_pure_virtual() {
    panic("C++: Somehow called a virtual function, eep!!");
}
__END_DECLS

namespace std {
type_info::~type_info() {}
}

namespace __cxxabiv1 {

extern "C" void* _ZTVN10__cxxabiv120__si_class_type_infoE[];

class __class_type_info : public std::type_info {
public:
    explicit __class_type_info(const char* n) : std::type_info(n) {}
    virtual ~__class_type_info();
};
__class_type_info::~__class_type_info() {}

class __si_class_type_info : public __class_type_info {
public:
    const __class_type_info* __base_type;
    explicit __si_class_type_info(const char* n, const __class_type_info* base)
        : __class_type_info(n), __base_type(base) {}
    virtual ~__si_class_type_info();
};
__si_class_type_info::~__si_class_type_info() {}

class __vmi_class_type_info : public __class_type_info {
public:
    unsigned int __flags;
    unsigned int __base_count;
    virtual ~__vmi_class_type_info();
};
__vmi_class_type_info::~__vmi_class_type_info() {}

struct vtable_prefix {
    ptrdiff_t whole_object;
    const __class_type_info* whole_type;
};

extern "C" void* __dynamic_cast(
    const void* sub,
    const __class_type_info* src,
    const __class_type_info* dst,
    ptrdiff_t src2dst_offset
) {
    if (!sub) return nullptr;

    const void* vptr = *reinterpret_cast<const void* const*>(sub);

    const auto* prefix = reinterpret_cast<const vtable_prefix*>(
        reinterpret_cast<const uint8_t*>(vptr) - sizeof(vtable_prefix)
    );

    const void* whole_obj = reinterpret_cast<const uint8_t*>(sub) + prefix->whole_object;
    const __class_type_info* current_type = prefix->whole_type;

    if (*current_type == *dst) {
        return const_cast<void*>(whole_obj);
    }

    while (current_type) {
        if (*current_type == *dst) {
            return const_cast<void*>(whole_obj);
        }

        static const __si_class_type_info dummy_si("", nullptr);
	const void* si_vptr = &_ZTVN10__cxxabiv120__si_class_type_infoE[2];
	const void* cur_vptr = *reinterpret_cast<const void* const*>(current_type);

        if (cur_vptr == si_vptr) {
            current_type = static_cast<const __si_class_type_info*>(current_type)->__base_type;
        } else {
            break;
        }
    }

    return nullptr;
}

extern "C" void __cxa_bad_cast() {
    panic("C++: Bad dynamic_cast on reference! eep!!");
}

}

void *operator new(size_t size) {
    void *ptr = kmalloc(size);

    if (!ptr) panic("C++: operator new failed");

    return ptr;
}

void *operator new[](size_t size) {
    void *ptr = kmalloc(size);

    if (!ptr) panic("C++: operator new[] failed");

    return ptr;
}

void operator delete(void *p) noexcept {
    kfree(p);
}

void operator delete(void *p, size_t) noexcept {
    kfree(p);
}

void operator delete[](void *p) noexcept {
    kfree(p);
}

void operator delete[](void *p, size_t) noexcept {
   kfree(p);
}

