# Guía de Instrucciones - xv6 Modificado

Esta guía contiene las instrucciones para compilar, ejecutar y probar el sistema operativo xv6 modificado.

---

## Requisitos Previos

### Software Necesario

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential gdb qemu-system-x86

# Fedora/RHEL
sudo dnf install gcc make gdb qemu-system-x86

# macOS (Homebrew)
brew install qemu gcc make gdb
```

### Verificar Instalación

```bash
qemu-system-i386 --version
gcc --version
make --version
```

---

## Compilación

### 1. Clonar el Repositorio

```bash
git clone https://github.com/juan-marin-19/proyecto-final-OS.git
cd proyecto-final-OS
```

### 2. Compilar el Sistema

```bash
make clean
make
```

### 3.  Verificar Archivos Generados

```bash
ls -lh kernel xv6.img fs.img
```

Deberías ver:

- `kernel` - Kernel compilado
- `xv6.img` - Imagen del disco de arranque
- `fs.img` - Sistema de archivos

---

## Ejecución

### Ejecutar en QEMU

```bash
make qemu
```

**Opciones adicionales:**

```bash
make qemu-nox           # Sin interfaz gráfica
make qemu CPUS=4        # Con 4 CPUs
make qemu QEMUOPTS="-m 512"  # Con 512MB de RAM
```

### Salir de QEMU

```python
Ctrl + A, luego X
```

---

## Ejecutar Pruebas

### 1. Prueba del Scheduler

```bash
scheddif
```

Esta prueba crea 3 procesos con diferentes perfiles:

- Proceso CPU-bound (consume CPU intensivamente)
- Proceso interactivo (simula E/S)
- Proceso yieldy (cede CPU frecuentemente)

**Salida esperada:**

``` bash
hog 1 tick 0
io  2 burst 0
yld 3 step 0
yld 3 step 1
... 
scheddiff finished
```

Los procesos interactivos y yieldy deben aparecer con mayor frecuencia que el proceso CPU-bound.

---

### 2. Prueba de Memoria

```bash
memdif
```

Esta prueba:

- Solicita 1 GiB de memoria virtual con sbrk()
- Toca solo 2 páginas (8 KB físicos)
- Verifica lazy allocation
- Libera la memoria

**Salida esperada:**

```bash
memdiff: brk inicial 12288
memdiff: sbrk(1073741824) OK, brk ahora 1073754112
memdiff: toque 2 páginas, no deberíamos morir si hay lazy alloc
memdiff: brk tras liberar 12288
memdiff: done
```

En xv6 original, sbrk(1GiB) falla. Con lazy allocation, solo asigna las páginas necesarias.

---

### 3. Pruebas Generales del Sistema

```bash
usertests
```

Ejecuta una suite completa de pruebas del sistema.

---

## Depuración con GDB

### Iniciar xv6 con GDB

Terminal 1:

```bash
make qemu-gdb
```

Terminal 2:

```bash
gdb kernel
```

### Comandos Útiles

```gdb
target remote localhost:26000
break scheduler
break trap
break allocuvm
continue
print ptable. proc[0]
print *myproc()
bt
```

---

## Modificar Parámetros

### Cambiar Número de Prioridades

En `param.h`:

```c
#define NPRIO 4  // Cambiar según necesidad
```

### Cambiar Frecuencia de Priority Boost

En `param.h`:

```c
#define BOOSTTIMER 100  // En ticks
```

### Cambiar Quantum Base

En `trap.c`:

```c
// Original: quantum = 2^priority
if(1 << myproc()->priority <= myproc()->ticks_running)
    yield();
```

Recompilar después de cambios:

```bash
make clean && make
```

---

## Solución de Problemas

### Error: "qemu-system-i386: command not found"

```bash
sudo apt-get install qemu-system-x86
```

### Error: "kernel panic"

```bash
git checkout -- . 
make clean && make
```

### QEMU no responde

``` python
Ctrl + A, luego X
```

---

## Comandos Útiles de xv6

```bash
ls                 # Listar archivos
cat README         # Ver contenido
echo hello         # Imprimir
grep world README  # Buscar texto
mkdir test         # Crear directorio
rm test            # Eliminar
wc README          # Contar palabras/líneas
```

---

## Archivos Importantes

### Scheduler MLFQ

- `proc.c` - Implementación principal
- `proc.h` - Estructuras de procesos
- `trap.c` - Manejo de interrupciones del timer
- `param.h` - Constantes (NPRIO, BOOSTTIMER)

### Lazy Allocation

- `trap.c` - Manejador de page faults
- `vm.c` - Funciones de memoria virtual
- `kalloc.c` - Asignador de páginas físicas

### Pruebas

- `scheddif.c` - Prueba de scheduler
- `memdif.c` - Prueba de memoria
- `usertests.c` - Suite de pruebas

---

## Recursos

- **Documentación xv6:** https://pdos.csail.mit.edu/6.828/2012/xv6.html
- **Libro xv6:** https://pdos.csail.mit.edu/6.828/2012/xv6/book-rev7.pdf
- **QEMU Documentation:** https://www.qemu.org/docs/master/