#include "rcu.h"
#include "test_structs.h"

int main(void) {
  int *p = 0, *v = 0;
  char *new_begin = 0;
  struct string *s = {0};

  rcu_assign_pointer(p, v);
  rcu_assign_pointer(s->begin, new_begin);
  return 0;
}
