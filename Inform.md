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
UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE.

##### struct proc

PCB del proceso, contiene:

* `sz`, `pgdir`, `kstack`
* `state`, `pid`, `parent`
* `tf`, `context`
* `chan`, `killed`
* `ofile[]`, `cwd`
* `name`

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

**Descripción del funcionamiento:**

* Habilita interrupciones (`sti()`).
* Recorre la tabla de procesos en orden fijo.
* Selecciona el primer proceso en estado `RUNNABLE`.
* Lo ejecuta hasta que ocurra:

  * interrupción de reloj,
  * `yield()`,
  * `sleep()`,
  * `exit()`.
* Luego retorna al scheduler y continúa desde el siguiente proceso.

---

## 1.2 Scheduler Modificado

> **[Contenido pendiente: aquí debe describirse el diseño del scheduler modificado, sus objetivos, estructuras, algoritmos y cambios realizados en `proc.c` y `proc.h`.]**

---

## 2. Gestor de Memoria

### 2.1 Gestor de Memoria Original

El gestor de memoria de xv6 se encuentra en `kalloc.c` y se basa en una **lista enlazada de páginas libres** (free list). Cada página tiene tamaño **4096 bytes**.

### Estructura global `kmem`

```c
struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;
```

### Nodo de la lista

```c
struct run {
  struct run *next;
};
```

### Funciones principales

#### `kinit1()` y `kinit2()`

Inicializan el sistema de memoria en dos fases durante el boot.

#### `freerange()`

```c
void freerange(void *vstart, void *vend) {
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
```

#### `kfree()`

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

### Limitaciones del sistema original

* No permite tamaños variables (solo páginas completas).
* Alta fragmentación interna.
* No realiza coalescencia.
* No hay política de asignación más allá de “pop de la freelist”.

---

## 2.2 Gestor de Memoria Modificado

> **[Contenido pendiente: aquí debe incluirse la descripción del gestor de memoria modificado, su motivación, diseño, estructuras, algoritmos implementados y cambios respecto a `kalloc.c`.]**

---

## 3. Pruebas

### 3.1 Scripts de Pruebas

> **[Contenido pendiente: aquí deben incluirse los scripts utilizados para probar el scheduler modificado y el gestor de memoria modificado, tanto programas de usuario como scripts del host.]**

---

### 3.2 Resultados

> **[Contenido pendiente: aquí deben incluirse los resultados obtenidos en las pruebas, comparaciones entre sistema original y modificado, métricas, tablas y análisis.]**
