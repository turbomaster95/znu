#include <kernel/module.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void modparam_show_all(module_t *mod)
{
    debugln("[modparam] Parameters for %s:", mod->name);
    for (uint32_t i = 0; i < mod->param_count; i++) {
        mod_param_t *p = &mod->params[i];
        switch (p->type) {
        case PARAM_TYPE_INT:
            debugln("  %s (int) = %d  [%s]", p->name, *(int*)p->addr, p->desc);
            break;
        case PARAM_TYPE_BOOL:
            debugln("  %s (bool) = %s  [%s]", p->name, *(bool*)p->addr ? "true" : "false", p->desc);
            break;
        case PARAM_TYPE_STRING:
        case PARAM_TYPE_CHARP:
            debugln("  %s (str) = %s  [%s]", p->name, *(char**)p->addr ? *(char**)p->addr : "(null)", p->desc);
            break;
        }
    }
}
