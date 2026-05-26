#include <stdint.h>
#include <kernel/module.h>
#include <kernel/kapi.h>

static int my_number = 42;
static bool my_bool = false;
static char *my_string = NULL;

MODULE_NAME("hello2");
MODULE_DESCRIPTION("Advanced test module with exports and params");
MODULE_AUTHOR("znu dev");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_DEPENDS("hello");           /* string, not token */
MODULE_ALIAS(testmod);             /* BARE TOKEN — no quotes! */
MODULE_ALIAS(example);             /* BARE TOKEN — no quotes! */

MODULE_PARAM_INT(my_number, "A number parameter");
MODULE_PARAM_BOOL(my_bool, "A boolean parameter");
MODULE_PARAM_CHARP(my_string, "A string parameter");

int hello2_add(int a, int b)
{
    return a + b + my_number;
}
EXPORT_SYMBOL(hello2_add);

static int hello2_init(void)
{
    debugln("hello2 loaded! my_number=%d my_bool=%s my_string=%s",
            my_number,
            my_bool ? "true" : "false",
            my_string ? my_string : "(null)");
    return 0;
}

static void hello2_exit(void)
{
    debugln("hello2 unloading");
}

module_init(hello2_init);
module_exit(hello2_exit);
