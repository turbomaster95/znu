extern "C" {
 #include <stddef.h>
 #include <page.h>
 #include <stdlib.h>
}

namespace __cxxabiv1 {
    struct __class_type_info {
        virtual ~__class_type_info() {}
    };
    struct __si_class_type_info : public __class_type_info {
        virtual ~__si_class_type_info() {}
    };
}

void *operator new(size_t size) { return kmalloc(size); }
void *operator new[](size_t size) { return kmalloc(size); }
void operator delete(void *p) noexcept { kfree(p); }
void operator delete(void *p, size_t) noexcept { kfree(p); }
void operator delete[](void *p) noexcept { kfree(p); }
void operator delete[](void *p, size_t) noexcept { kfree(p); }

extern "C" void __cxa_pure_virtual() {
    panic("C++: Somehow called a virtual function, eep!!");
}
