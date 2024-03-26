#include "marsio.h"

struct mr_instance * mr_instance = NULL;

int main(int argc, char * argv)
{
    mr_instance = marsio_create();
}