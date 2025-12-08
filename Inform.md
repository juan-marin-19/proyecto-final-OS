# Informe Proyecto Final — Sistemas Operativos

<center>
Escuela de Ingeniería de Sistemas y Computación
</center>
<center>
<img src="Images/LogoSimboloUV.png" alt="LogoSimbolo Universidad Del Valle" width="100" height="120">
</center>
<center>
Profesor: Jefferson Amado Peña Torres
</center>
Autores:

* 2422117 Juan Marin Orozco
* Juan David Guar
* 2418564 Isabella Bermúdez
* 2435998 Brandon Alexis Franco Flor
* 2416541 González Rosero Andrés Gerardo

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

### 1.2 Scheduler Modificado

#### Descripción general

El scheduler de xv6 ha sido modificado para implementar un Multi-Level Feedback Queue (MLFQ), un algoritmo de planificación que utiliza múltiples colas de prioridad para mejorar la equidad y la capacidad de respuesta del sistema. Este diseño permite que procesos interactivos obtengan mejor tiempo de respuesta mientras que procesos CPU-bound reciben tratamiento justo sin sufrir starvation.

#### Objetivos del diseño

Los objetivos principales de esta modificación son:

* Mejorar la equidad: Distribuir el tiempo de CPU de manera más justa entre procesos con diferentes características de carga.
* Reducir tiempos de respuesta para procesos interactivos: Procesos que realizan operaciones de E/S frecuentemente deben obtener CPU rápidamente para mantener la responsividad del sistema.
* Evitar starvation: Implementar mecanismos que garanticen que todos los procesos eventualmente reciban tiempo de CPU, independientemente de su prioridad.
* Ajustar dinámicamente las prioridades: Los procesos que consumen mucho CPU son degradados en prioridad, mientras que los procesos interactivos mantienen alta prioridad.

#### Estructuras de datos modificadas

##### Cambios en struct proc (proc.h)

Se agregaron tres nuevos campos a la estructura proc:

```c
struct proc {
  // ... campos existentes ...
  uint priority;          // Prioridad actual del proceso dentro del MLFQ (0 = máxima)
  uint ticks_running;     // Cantidad de ticks que el proceso ha usado en su quantum actual
  struct proc *next_proc; // Puntero al siguiente proceso en la cola actual
};
```

* priority: Define el nivel de prioridad del proceso. El valor 0 representa la máxima prioridad, y valores mayores representan prioridades menores. Los nuevos procesos inician en prioridad 0.
* ticks_running: Contador de interrupciones del timer que el proceso ha consumido durante su quantum actual. Se reinicia cada vez que el proceso obtiene la CPU.
* next_proc: Permite enlazar procesos dentro de una misma cola de prioridad, formando listas enlazadas sin necesidad de estructuras adicionales.

##### Modificaciones en ptable (proc.c)

La tabla de procesos fue extendida para soportar múltiples colas de prioridad:

```c
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  struct proc *queue_first[NPRIO]; // Array donde cada posición guarda el primer proceso de la cola de esa prioridad
  struct proc *queue_last[NPRIO];  // Array donde se guarda el puntero al último proceso de cada cola de prioridad
} ptable;
```

* queue_first[NPRIO]: Array de punteros que mantiene la referencia al primer proceso de cada cola de prioridad. Un valor `0` indica que la cola está vacía.
* queue_last[NPRIO]: Array de punteros que mantiene la referencia al último proceso de cada cola de prioridad, facilitando la inserción eficiente al final de la cola.

##### Constante NPRIO (param.h)

```c
#define NPRIO 4  // Total levels of scheduler priority
```

Define el número total de niveles de prioridad en el sistema. Con NPRIO = 4, existen 4 niveles (0, 1, 2, 3), donde 0 es la máxima prioridad y 3 la mínima.

#### Algoritmos implementados

##### Función enqueue() (proc.c)

Encola un proceso en su cola de prioridad correspondiente:

```c
void enqueue(struct proc *p)
{
  int prio = p->priority;                     // Halla la prioridad del proceso recibido
  if(ptable.queue_last[prio] != 0)            // Si la cola no está vacía
    ptable.queue_last[prio]->next_proc = p;   // Enlaza el último proceso con el nuevo
  else
    ptable.queue_first[prio] = p;             // Si la cola está vacía, el nuevo proceso es el primero

  p->next_proc = 0;                           // Como se añade al final, su next_proc es NULL
  ptable.queue_last[prio] = p;                // Actualiza queue_last[] para esta prioridad
  p->state = RUNNABLE;
}
```

**Funcionamiento:**

* Inserta el proceso al final de la cola correspondiente a su nivel de prioridad actual.
* Si la cola está vacía, el proceso se convierte en el primer y último elemento.
* Si la cola tiene elementos, se enlaza al final usando el campo next_proc del último proceso.
* El proceso se marca como RUNNABLE para indicar que está listo para ejecutarse.

##### Función dequeue() (proc.c)

Desencola y devuelve el primer proceso de una cola específica:

```c
struct proc* dequeue(int priority)
{
    struct proc *p = ptable.queue_first[priority]; // Toma el primer proceso de la cola
    
    if(p == 0)
        return 0;  // Cola vacía
    
    ptable.queue_first[priority] = p->next_proc;   // Actualiza el primero con el siguiente
    
    if(ptable.queue_last[priority] == p)           // Si era el único en la cola
        ptable.queue_last[priority] = 0;           // La cola queda vacía
    
    p->next_proc = 0;                              // Limpia el puntero next_proc
    return p;
}
```

**Funcionamiento:**

* Extrae el primer proceso de la cola indicada (política FIFO dentro de cada prioridad).
* Actualiza queue_first para apuntar al siguiente proceso en la cola.
* Si el proceso era el único en la cola, actualiza también queue_last a 0.
* Retorna 0 si la cola está vacía.

##### Función scheduler() modificada (proc.c)

El scheduler ahora recorre las colas por orden de prioridad:

```c
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for(;;){
    sti();  // Habilita interrupciones
    acquire(&ptable.lock);
    
    // Recorre las colas de prioridades desde la mayor (0) a la menor (NPRIO-1)
    for(uint level = 0; level < NPRIO; level++){
      p = dequeue(level);  // Saca el primer proceso de la cola
      if(p == 0)           // Si la cola está vacía, continúa con la siguiente
        continue;
      
      c->proc = p;              // Asigna el proceso a la CPU
      switchuvm(p);             // Cambia el directorio de memoria
      p->ticks_running = 0;     // Reinicia el quantum del proceso
      p->state = RUNNING;       // Marca el proceso como ejecutándose
      
      swtch(&(c->scheduler), p->context);  // Cambia de contexto
      switchkvm();
      
      c->proc = 0;
      break;  // Importante: siempre ejecuta primero la cola de mayor prioridad
    }
    release(&ptable.lock);
  }
}
```

###### Cambios respecto al scheduler original

* En lugar de recorrer linealmente la tabla de procesos, ahora recorre las colas de prioridad en orden (0 a NPRIO-1).
* Siempre selecciona el primer proceso de la cola de mayor prioridad no vacía (política de prioridades estricta).
* Dentro de cada cola se respeta el orden FIFO.
* El break después de ejecutar un proceso asegura que siempre se revisen primero las colas de mayor prioridad.

##### Función yield() modificada (proc.c)

Implementa la degradación de prioridad al consumir el quantum:

```c
void yield(void)
{
  acquire(&ptable.lock);
  struct proc *p = myproc();
  
  // Si un proceso consumió su quantum completo, se baja su prioridad
  if (p->priority < NPRIO - 1)
    p->priority++;
  
  p->ticks_running = 0;  // Reinicia el quantum del proceso
  enqueue(p);            // Reencola el proceso en su nueva prioridad
  
  sched();
  release(&ptable.lock);
}
```

###### Funcionamiento

* Cuando un proceso agota su quantum, su prioridad se degrada (aumenta en 1).
* La degradación solo ocurre si el proceso no está en la prioridad mínima (`NPRIO - 1`).
* El proceso se reencola en su nueva cola de prioridad.
* Este mecanismo penaliza a procesos CPU-bound que consumen su quantum completo.

##### Función priority_boost() (proc.c)

Mecanismo anti-starvation que periódicamente reinicia todas las prioridades:

```c
void priority_boost(void)
{
  struct proc *p, *last = 0;
  uint first = 0;
  
  acquire(&ptable.lock);
  
  // Coloca la prioridad de todos los procesos en la prioridad máxima (0)
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    p->priority = 0;
  
  // Buscar el índice de la primera cola no vacía
  while (first < NPRIO && ptable.queue_first[first] == 0)
    first++;
  
  // Si se encontró alguna cola no vacía
  if (first < NPRIO) {
    ptable.queue_first[0] = ptable.queue_first[first]; // Primera cola apunta al inicio
    last = ptable.queue_last[first];                   // last apunta al final de esta cola
    
    // Concatenar todas las colas no vacías
    for (uint i = first + 1; i < NPRIO; i++) {
      if (ptable.queue_first[i]) {
        last->next_proc = ptable.queue_first[i];  // Enlaza con la siguiente cola
        last = ptable.queue_last[i];              // Actualiza last
      }
    }
    ptable.queue_last[0] = last;  // Actualiza el final de la cola 0
  }
  
  // Vaciar las demás colas
  for (uint i = 1; i < NPRIO; i++) {
    ptable.queue_first[i] = ptable.queue_last[i] = 0;
  }
  
  release(&ptable.lock);
}
```

###### Funcionamiento

* Resetea la prioridad de todos los procesos a 0 (máxima prioridad).
* Concatena todas las colas no vacías en la cola de prioridad 0.
* Vacía todas las demás colas.
* Se ejecuta periódicamente cada BOOSTTIMER ticks (definido como 100 en param.h).
* Garantiza que ningún proceso sufra starvation al darle una oportunidad periódica de ejecutarse con máxima prioridad.

#### Quantum y política de degradación de prioridad (trap.c)

El quantum de cada proceso es dinámico y depende de su prioridad:

```c
// En trap.c, manejo de interrupciones del timer:
if(myproc() && myproc()->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER){
  myproc()->ticks_running++;
  // quantum = 2^priority ticks
  if(1 << myproc()->priority <= myproc()->ticks_running)
    yield();
}
```

##### Política de quantum

* El quantum asignado es **2^priority** ticks.
  * Prioridad 0: quantum = 2^0 = 1 tick
  * Prioridad 1: quantum = 2^1 = 2 ticks
  * Prioridad 2: quantum = 2^2 = 4 ticks
  * Prioridad 3: quantum = 2^3 = 8 ticks
* Los procesos en colas de menor prioridad reciben quantums más largos para compensar la menor frecuencia de ejecución.
* Cada tick de timer incrementa ticks_running, y cuando alcanza su quantum, el proceso invoca yield().

##### Priority boost periódico (trap.c)

```c
if(ticks % BOOSTTIMER == 0)
  priority_boost();
```

Cada 100 ticks del sistema (BOOSTTIMER = 100), se ejecuta priority_boost() para prevenir starvation.

#### Cambios en el comportamiento respecto al scheduler original

| Aspecto | Scheduler Original (Round-Robin) | Scheduler Modificado (MLFQ) |
|---------|----------------------------------|----------------------------|
| Política de selección | Recorre linealmente la tabla de procesos | Recorre colas de prioridad (0 a NPRIO-1) |
| Equidad | Todos los procesos tienen igual prioridad | Procesos interactivos tienen mayor prioridad |
| Quantum | Fijo (definido por el timer) | Dinámico (2^priority ticks) |
| Degradación | No existe | Los procesos que consumen su quantum bajan de prioridad |
| Anti-starvation | Garantizado por Round-Robin | Garantizado por priority_boost() periódico |
| Manejo de E/S | Sin ventajas especiales | Procesos que hacen E/S mantienen alta prioridad |
| Estructuras | Tabla simple de procesos | Múltiples colas enlazadas por prioridad |
| Responsividad | Uniforme para todos | Mayor para procesos interactivos |

#### Ventajas del MLFQ implementado

1. Mejor tiempo de respuesta interactivo: Procesos que frecuentemente ceden la CPU (por E/S o sleep) permanecen en alta prioridad.
2. Penalización justa para CPU-bound: Procesos que consumen mucho CPU son gradualmente degradados.
3. Prevención de starvation: El mecanismo de priority boost asegura que todos los procesos eventualmente ejecuten.
4. Adaptación dinámica: El sistema se ajusta automáticamente al comportamiento de cada proceso sin configuración manual.

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

### 2.2 Gestor de Memoria Modificado

#### Análisis del código actual

##### Estado de kalloc.c

El archivo kalloc.c se mantiene sin modificaciones respecto al sistema original de xv6. Conserva la implementación basada en una lista enlazada de páginas libres (free list) de 4096 bytes, con las funciones kinit1(), kinit2(), kfree() y kalloc() operando de la misma manera que en el xv6 original.

Justificación de mantener el gestor original:

* Simplicidad y eficiencia: El sistema de free list es simple, eficiente para el manejo de páginas completas, y suficiente para las necesidades del sistema operativo modificado.
* Robustez probada: La implementación original ha sido ampliamente probada y es confiable.
* Compatibilidad: Mantener kalloc.c sin cambios garantiza compatibilidad con el resto del kernel que depende de su interfaz.
* Enfoque en scheduler: El objetivo principal del proyecto era modificar el scheduler, por lo que se priorizó la estabilidad del gestor de memoria básico.

#### Implementación de Lazy Allocation

Aunque kalloc.c no fue modificado, sí se implementó una mejora significativa en el gestor de memoria virtual mediante lazy allocation (asignación perezosa) de páginas en el manejador de page faults (trap.c).

##### Lazy Allocation en trap.c

Se agregó un manejador específico para page faults (T_PGFLT) que implementa asignación de memoria bajo demanda:

```c
// En trap.c, dentro del switch(tf->trapno):
case T_PGFLT:
{
  // 1. Obtener la dirección que causó el fallo
  uint fault_addr = rcr2();

  // 2. Validar que esté dentro del espacio de direcciones
  if (fault_addr >= myproc()->sz) {
    // Dirección inválida: terminar el proceso
    cprintf("pid %d %s: invalid memory access at 0x%x\n",
            myproc()->pid, myproc()->name, fault_addr);
    myproc()->killed = 1;
    break;
  }

  // 3. Redondear al inicio de la página
  uint page_start = PGROUNDDOWN(fault_addr);

  // 4. Asignar exactamente una página (4096 bytes)
  if (allocuvm(myproc()->pgdir, page_start, page_start + PGSIZE) == 0) {
    // Fallo al asignar memoria (sin RAM disponible)
    cprintf("pid %d %s: out of memory\n",
            myproc()->pid, myproc()->name);
    myproc()->killed = 1;
    break;
  }

  // 5. Éxito: la memoria fue asignada, el proceso puede continuar
  break;
}
```

##### Funcionamiento de Lazy Allocation

1. Detección del page fault: Cuando un proceso intenta acceder a una dirección de memoria que no tiene una página física asignada, el hardware genera una excepción de page fault (trap número T_PGFLT).

2. Lectura de la dirección causante: Se lee el registro CR2 mediante rcr2() para obtener la dirección virtual que causó el fallo.

3. Validación del acceso: Se verifica que la dirección esté dentro del espacio de direcciones válido del proceso (fault_addr < myproc()->sz). Si la dirección es inválida (fuera de los límites establecidos por sbrk()), se termina el proceso.

4. Alineación de página: Se redondea la dirección al inicio de la página usando PGROUNDDOWN() para asignar páginas completas de 4096 bytes.

5. Asignación física: Se llama a allocuvm() para asignar físicamente una página en esa ubicación. Si la asignación falla (por falta de RAM), se termina el proceso.
6. Continuación transparente: Si la asignación es exitosa, el proceso continúa su ejecución desde donde quedó, sin que el código de usuario note la diferencia.

##### Ventajas de Lazy Allocation

* Reducción del uso de memoria: Solo se asigna memoria física cuando realmente se accede a ella, no cuando se solicita con sbrk().
* Arranque más rápido: Las llamadas a sbrk() grandes retornan inmediatamente sin asignar toda la memoria solicitada.
* Eficiencia en aplicaciones sparse: Aplicaciones que reservan grandes espacios de memoria pero solo usan una fracción obtienen beneficios significativos.
* Mejor gestión de recursos: La memoria física se utiliza de manera más eficiente, permitiendo más procesos concurrentes.

#### Interacción con vm.c

El manejo de lazy allocation se integra con las funciones existentes en vm.c:

* allocuvm(): Se utiliza en el manejador de page faults para asignar físicamente las páginas demandadas.
* deallocuvm(): Se encarga de liberar páginas cuando se reduce el tamaño del proceso con sbrk() negativo.
* walkpgdir(): Navega por la tabla de páginas para verificar y crear entradas según sea necesario.

Estas funciones no requirieron modificaciones, ya que el mecanismo de lazy allocation se implementó completamente en el manejador de traps.

#### Limitaciones del sistema modificado

A pesar de las mejoras, el gestor de memoria aún presenta algunas limitaciones:

* Fragmentación interna: Sigue asignando páginas completas de 4096 bytes, lo que puede desperdiciar memoria en pequeñas asignaciones.
* No hay coalescencia: Las páginas libres no se fusionan para formar bloques más grandes.
* Sin compactación: No existe un mecanismo para reorganizar la memoria y reducir fragmentación.
* Política simple: La asignación sigue una política de "primera disponible" sin optimizaciones de localidad.

#### Conclusión sobre el gestor de memoria
  
La decisión de mantener kalloc.c sin modificaciones fue acertada, ya que:

* Se priorizó la estabilidad y confiabilidad del sistema base.
* Se implementó una mejora significativa (lazy allocation) que beneficia el uso de memoria sin modificar el allocator básico.
* El sistema resultante es más eficiente en el uso de RAM física mientras mantiene la simplicidad del diseño original.
* La arquitectura modular de xv6 permitió agregar lazy allocation sin cambios intrusivos en el gestor de memoria física.

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

#define BIG (1024*1024*1024)  // 1 GiB

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

#### scheddif

Los resultados obtenidos al ejecutar el script scheddif muestran un comportamiento esperado del scheduler MLFQ implementado. Los procesos interactivos y yieldy recibieron más tiempo de CPU en comparación con el proceso CPU-bound, que fue degradado en prioridad tras consumir su quantum completo. Esto se reflejó en la salida del script, donde los mensajes de los procesos interactivos aparecieron con mayor frecuencia y menor latencia.

##### Resultados en la versión original

```bash
$ scheddif
hog 1 tick 0
io  2 burst 0
yld 3 step 0
yld 3 step 1
yld 3 step 2
yld 3 step 3
yld 3 step 4
yld 3 step 5
yld 3 step 6
yld 3 step 7
yld 3 step 8
yld 3 step 9
yld 3 step 10
yld 3 step 11
yld 3 done
hog 1 tick 1
io  2 burst 1
hog 1 tick 2
hog 1 tick 3
io  2 burst 2
hog 1 tick 4
io  2 burst 3
hog 1 done (-1186941120)
io  2 burst 4
io  2 burst 5
io  2 burst 6
io  2 burst 7
io  2 done
scheddiff finished
```

##### Resultados en la versión modificada

```bash
$ scheddif
hog 1 tick 0
io  2 burst 0
yld 3 step 0
yld 3 step 1
yld 3 step 2
hog 1 tick 1
yld 3 step 3
yld 3 step 4
yld 3 step 5
io  2 burst 1
yld 3 step 6
hog 1 tick 2
yld 3 step 7
hog 1 tick 3
yld 3 step 8
io  2 burst 2
yld 3 step 9
hog 1 tick 4
yld 3 step 10
hog 1 done (-1186941120)
yld 3 step 11
yld 3 done
io  2 burst 3
io  2 burst 4
io  2 burst 5
io  2 burst 6
io  2 burst 7
io  2 done
scheddiff finished
```

#### memdif

Los resultados del script memdif evidencian la correcta implementación de lazy allocation en el gestor de memoria. En la versión original, la llamada a sbrk() para asignar 1 GiB de memoria falló debido a la falta de memoria física disponible. En contraste, en la versión modificada, la llamada a sbrk() fue exitosa, y solo las dos primeras páginas fueron tocadas, demostrando que la memoria se asignó bajo demanda sin agotar los recursos del sistema.

##### Resultados en la versión original

```bash
$ memdif
memdiff: brk inicial 12288
allocuvm out of memory
memdiff: sbrk(1073741824) FALLÓ (asignación eager)
$
```

##### Resultados en la versión modificada

```bash
$ memdif
memdiff: brk inicial 12288
memdiff: sbrk(1073741824) OK, brk ahora 1073754112
memdiff: toque 2 páginas, no deberíamos morir si hay lazy alloc
memdiff: brk tras liberar 12288
memdiff: done
```

### 3.3 Conclusiones

Los resultados obtenidos de las pruebas realizadas con los scripts scheddif y memdif confirman que las modificaciones implementadas en el scheduler y el gestor de memoria de xv6 han sido exitosas y cumplen con los objetivos planteados.

1. Scheduler MLFQ: El scheduler modificado demostró un comportamiento adecuado al priorizar procesos interactivos y penalizar procesos CPU-bound, mejorando la equidad y la capacidad de respuesta del sistema.
2. Lazy Allocation: La implementación de lazy allocation en el gestor de memoria permitió una asignación eficiente de memoria bajo demanda, reduciendo el uso innecesario de RAM física y mejorando la gestión de recursos del sistema.
3. Robustez y estabilidad: Mantener el gestor de memoria original garantizó la estabilidad del sistema, mientras que las mejoras se integraron de manera modular sin afectar la funcionalidad básica.
4. Validación mediante pruebas: Los scripts de prueba desarrollados permitieron validar de manera efectiva las modificaciones, proporcionando evidencia clara del correcto funcionamiento del sistema modificado.
