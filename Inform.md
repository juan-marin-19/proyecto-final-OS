# Informe Proyecto Final — Sistemas Operativos

<center>
Escuela de Ingeniería de Sistemas y Computación
</center>
<center>
<img src="Images/LogoSimboloUV.png" alt="LogoSimbolo Universidad Del Valle" width="100" height="120">
</center>
<center>
Profesor Juan Amado Peña Torres
</center>
Autores:

* 2422117 Juan Marin Orozco
* Juan David Guar
* 202418564 Isabella Bermúdez
* 2435998 Brandon Alexis Franco Flor
* 202416541 González Rosero Andrés Gerardo

<center>
Diciembre de 2025
</center>

---

## 1. Scheduler

### 1.1 Scheduler de XV6 Original

#### Descripción general

xv6 utiliza un algoritmo de planificación Round-Robin, con quantum definido por el timer del LAPIC (~100 Hz, un tick cada ~10 ms). El scheduler está implementado principalmente en:

* proc.c
* proc.h

y depende también de:

* trap.c
* lapic.c
* switch.s

#### Estructuras relevantes (proc.h)

##### struct cpu

Contiene:

* apicid: Identificador del LAPIC
* proc: Proceso actualmente ejecutándose en esta CPU
* scheduler: Contexto del scheduler
* started: Indicador de si la CPU ha arrancado

##### struct context

Registros guardados en un cambio de contexto:

* EBX: Puntero a datos o estructuras
* ESI: Puntero de origen en operaciones de cadena
* EDI: Puntero de destino en operaciones de cadena
* EBP: Puntero base para la pila
* EIP: Contador de instrucción

##### enum procstate

Estados posibles:

* UNUSED: Entrada de tabla no usada
* EMBRYO: Proceso en creación
* SLEEPING: Proceso dormido
* RUNNABLE: Proceso listo para correr
* RUNNING: Proceso en ejecución
* ZOMBIE: Proceso terminado, esperando recolección

##### struct proc

PCB (Process Control Block) del proceso, contiene:

* uint sz : Tamaño total de la memoria del proceso, en bytes
* pde_t\* pgdir: Puntero a la tabla de páginas del proceso
* char *kstack: Direccion base de la pila del kernel de este proceso
* enum procstate state: Estado actual del proceso.
* ini pid:  PID único del proceso
* struct proc *parent: puntero al proceso padre
* struct trapframe *tf:  trapframe guardodo en la pila del kernel
* struct context \*context: puntero al contexto de kernel guardado, esencial para el cambio de contexto.
* void \*chan:  recurso esperado cuando el proceso está en SLEEPING.
* int killed: Indica si se hizo kill() al proceso
* struct file \*ofile[NOFILE]: Array de archivos abiertos
* struct inode \*cwd: Directorio del trabajo actual
* char name[16]: Nombre del proceso.

#### Implementación del Scheduler

```c
void scheduler(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for(;;){
    sti();

    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      c->proc = 0;
    }
    release(&ptable.lock);
  }
}
```

##### Descripción del funcionamiento

* Habilita interrupciones (sti()).
* Recorre la tabla de procesos en orden fijo.
* Selecciona el primer proceso en estado RUNNABLE.
* Lo ejecuta hasta que ocurra:

  * interrupción de reloj,
  * yield(),
  * sleep(),
  * exit().
* Luego retorna al scheduler y continúa desde el siguiente proceso.

## 1.2 Scheduler Modificado

> **[Contenido pendiente: aquí debe describirse el diseño del scheduler modificado, sus objetivos, estructuras, algoritmos y cambios realizados en `proc.c` y `proc.h`.]**

---

## 2. Gestor de Memoria

### 2.1 Gestor de Memoria Original

El gestor de memoria de xv6 se encuentra en `kalloc.c` y se basa en una **lista enlazada de páginas libres** (free list). Cada página tiene tamaño **4096 bytes**.

### Estructura de datos global (kalloc.c)

```c
struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;
```

>Esta estructura mantiene:
>* lock : Spinlock para sincronización en sistemas multiprocesador.
>* use_lock freelist : Flag que indica si debe usar el lock.
>* freelist: Puntero al inicio de la lista de páginas libre.

### Nodo de la lista

```c
struct run {
  struct run *next;
};
```

>Cada página física libre (4096 bytes) almacena en sus primeros bytes un puntero al siguiente nodo, formando una lista simplemente enlazada

### Funciones principales

#### kinit1() y kinit2()

Inicializan el sistema de memoria en dos fases durante el boot, kinit1() inicializa memoria suficiente para arrancar (4MB), mientras que kinit2() inicializa el resto de la memoria física disponible.

#### freerange()

```c
void freerange(void *vstart, void *vend) {
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
```

Divide un rango dado en páginas, marcando un rango continuo de memoria como libre y las agrega a las listas libres mediante kfree().

#### kfree()

```c
void kfree(char *v) {
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);

  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;

  if(kmem.use_lock)
    release(&kmem.lock);
}
```

Libera una página física, validando la dirección, llenándola con 1s para detectar usos indebidos y agregándola al inicio de la lista de páginas libres.

### Limitaciones del sistema original

* No permite tamaños variables (solo páginas completas).
* Alta fragmentación interna.
* No realiza coalescencia.
* No hay política de asignación más allá de “pop de la freelist”, es decir, no reduce la fragmentación ni optimiza el uso de memoria.

## 2.2 Gestor de Memoria Modificado

> **[Contenido pendiente: aquí debe incluirse la descripción del gestor de memoria modificado, su motivación, diseño, estructuras, algoritmos implementados y cambios respecto a `kalloc.c`.]**

---

## 3. Pruebas

### 3.1 Scripts de Pruebas

Para realizar las pruebas de rendimiento y funcionalidad del sistema operativo modificado, se han desarrollado, añadido al sistema y ejecutado varios scripts de prueba. A continuación se describen los scripts implementados:

#### scheddif

```c
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
```

Este script crea tres tipos de procesos para evaluar el scheduler:

* CPU-bound: Consume CPU intensivamente.
* Interactivo: Simula procesos que alternan entre CPU y E/S.
* Yieldy: Cede la CPU frecuentemente.

Espera a que todos los procesos terminen y reporta la finalización.
Este script permite observar cómo el scheduler maneja diferentes cargas de trabajo y evaluar su rendimiento.

#### memdif

```c
#include "types.h"
#include "stat.h"
#include "user.h"

#define BIG (64*1024*1024)  // 64 MiB

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
```

Este script prueba la asignación y liberación de memoria dinámica:

* Solicita 64 MiB de memoria usando sbrk().
* Toca solo las dos primeras páginas para verificar la asignación perezosa (lazy allocation).
* Intenta liberar la memoria asignada y verifica el estado del puntero de programa (brk).

Permite evaluar el comportamiento del gestor de memoria modificado en términos de asignación y liberación eficiente.

### 3.2 Resultados

> **[Contenido pendiente: aquí deben incluirse los resultados obtenidos en las pruebas, comparaciones entre sistema original y modificado, métricas, tablas y análisis.]**
