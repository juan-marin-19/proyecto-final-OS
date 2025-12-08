#include "types.h"
#include "stat.h"
#include "user.h"

#define N (3)

static void cpu_hog(int id) {
  volatile uint x = 0;
  for (int i = 0; i < 150000000; i++) {
    x += i;
    if ((i % 30000000) == 0)
      printf(1, "hog %d tick %d\n", id, i/30000000);
  }
  printf(1, "hog %d done (%d)\n", id, x);
}

static void interactive(int id) {
  for (int i = 0; i < 8; i++) {
    printf(1, "io  %d burst %d\n", id, i);
    sleep(20);          // simula E/S corta
  }
  printf(1, "io  %d done\n", id);
}

static void yieldy(int id) {
  for (int i = 0; i < 12; i++) {
    printf(1, "yld %d step %d\n", id, i);
    yield();            // cede CPU rápido
  }
  printf(1, "yld %d done\n", id);
}

int
main(void)
{
  int pid;

  // Crea tres perfiles: CPU-bound, interactivo, yieldy
  if ((pid = fork()) == 0) { cpu_hog(1); exit(); }
  if ((pid = fork()) == 0) { interactive(2); exit(); }
  if ((pid = fork()) == 0) { yieldy(3); exit(); }

  // Padre espera
  while (wait() >= 0) {}
  printf(1, "scheddiff finished\n");
  exit();
}