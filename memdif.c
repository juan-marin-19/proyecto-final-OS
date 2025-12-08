#include "types.h"
#include "stat.h"
#include "user.h"

#define BIG (1024*1024*1024)  // 512 MiB

int
main(void)
{
  int sz0 = (int)sbrk(0);
  printf(1, "memdiff: brk inicial %d\n", sz0);

  char *p = sbrk(BIG);
  if (p == (char*)-1) {
    printf(1, "memdiff: sbrk(%d) FALLÓ (asignación eager)\n", BIG);
    exit();
  }
  printf(1, "memdiff: sbrk(%d) OK, brk ahora %d\n", BIG, (int)sbrk(0));

  // Tocar solo las dos primeras páginas
  p[0] = 'A';
  p[4096] = 'B';
  printf(1, "memdiff: toque 2 páginas, no deberíamos morir si hay lazy alloc\n");

  // Intentar liberar
  if (sbrk(-BIG) == (char*)-1)
    printf(1, "memdiff: free falló\n");
  else
    printf(1, "memdiff: brk tras liberar %d\n", (int)sbrk(0));

  printf(1, "memdiff: done\n");
  exit();
}