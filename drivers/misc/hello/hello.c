#include <stdint.h>
#include <kernel/module.h>
#include <kernel/kapi.h>

MODULE_NAME("hello");
MODULE_DESCRIPTION("Test module");
MODULE_AUTHOR("znu dev");
MODULE_LICENSE("MIT");
MODULE_VERSION("1.0");
MODULE_ALIAS(hello);

static int hello_init(void)
{
    debugln("hello1 loaded!");
    return 0;
}

static void hello_exit(void)
{
    debugln("hello1 unloading");
}

module_init(hello_init);
module_exit(hello_exit);
