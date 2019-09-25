#include "id.h"

identifier generate_id() {
    static identifier id = 0x0;
    return ++id;
}

