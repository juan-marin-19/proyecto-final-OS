# Proyecto Final - Sistemas Operativos
### Modificaciones a xv6: Scheduler MLFQ y Lazy Allocation

<p align="center">
  <img src="Images/LogoSimboloUV.png" alt="Logo Universidad Del Valle" width="100" height="120">
</p>

<p align="center">
  <strong>Universidad del Valle</strong><br>
  Escuela de Ingeniería de Sistemas y Computación<br>
  Profesor: Jefferson Amado Peña Torres<br>
  Diciembre de 2025
</p>

---

## Autores

- **Juan Marin Orozco** - 2422117
- **Juan David Guar**
- **Isabella Bermúdez** - 2418564
- **Brandon Alexis Franco Flor** - 2435998
- **Andrés Gerardo González Rosero** - 2416541

---

## Descripción del Proyecto

Este proyecto implementa modificaciones al sistema operativo educativo **xv6**, enfocándose en dos componentes:

### Scheduler Multi-Level Feedback Queue (MLFQ)

Reemplazo del scheduler Round-Robin original por un algoritmo MLFQ de 4 niveles de prioridad que:

- Mejora la equidad en la distribución de CPU
- Reduce tiempos de respuesta para procesos interactivos
- Implementa mecanismos anti-starvation mediante priority boost
- Ajusta dinámicamente las prioridades según el comportamiento de los procesos

### Lazy Allocation

Implementación de asignación perezosa de memoria que:

- Asigna memoria física solo cuando se accede a ella
- Reduce el consumo de RAM física
- Mejora el rendimiento en aplicaciones con grandes reservas de memoria
- Maneja page faults de manera transparente

---

## Video de Sustentación

[Enlace al video en YouTube](Link_del_video)

---

## Documentación

### Informe Técnico Completo

Para información detallada sobre la implementación, estructuras de datos, algoritmos y resultados de las pruebas, consulta el [Informe Técnico](./Inform.md).

**Contenido del informe:**

- Scheduler (original y modificado)
- Gestor de Memoria (kalloc.c y lazy allocation)
- Pruebas y Resultados

### Instrucciones de Compilación y Ejecución

Para compilar, ejecutar y probar el sistema modificado, consulta la [Guía de Instrucciones](./Instructions.md)

---

## Estructura del Proyecto

``` bash
proyecto-final-OS/
├── README.md              # Este archivo
├── INSTRUCTIONS.md        # Guía de compilación y ejecución
├── Inform.md              # Informe técnico completo
├── Images/                # Recursos gráficos
├── proc.c                 # Implementación del scheduler MLFQ
├── proc. h                 # Estructuras de procesos modificadas
├── trap.c                 # Manejo de page faults y lazy allocation
├── kalloc.c               # Gestor de memoria física
├── vm.c                   # Gestor de memoria virtual
├── param.h                # Parámetros del sistema (NPRIO, BOOSTTIMER)
├── scheddif.c             # Prueba de scheduler
├── memdif. c               # Prueba de memoria
└── ...                     # Archivos originales de xv6
```

---

## Características Principales

### Scheduler MLFQ

- 4 niveles de prioridad (0 = máxima, 3 = mínima)
- Quantum dinámico: 2^priority ticks
- Degradación automática de procesos CPU-bound
- Priority boost cada 100 ticks (anti-starvation)
- Colas FIFO por nivel de prioridad

### Lazy Allocation

- Asignación de memoria bajo demanda
- Manejo transparente de page faults
- Validación de direcciones virtuales
- Integración con sbrk()
- Soporte para aplicaciones sparse

---

## Pruebas Implementadas

**scheddif**: Evalúa el comportamiento del scheduler con tres tipos de procesos (CPU-bound, interactivo, yieldy).

**memdif**: Prueba la asignación y liberación de memoria con solicitudes grandes (1 GiB).

---

## Resultados Destacados

| Métrica | xv6 Original | xv6 Modificado |
|---------|--------------|----------------|
| Tiempo respuesta (procesos I/O) | Uniforme | Mejorado ~40% |
| Uso de memoria (sparse apps) | Falla con 1GiB | Éxito |
| Equidad CPU | Round-Robin simple | MLFQ dinámico |
| Starvation | Imposible | Prevenido (boost) |

---

## Referencias

- [xv6: A simple, Unix-like teaching operating system](https://pdos.csail.mit.edu/6.828/2012/xv6.html)
- Arpaci-Dusseau, R. H., & Arpaci-Dusseau, A. C.  (2018). *Operating Systems: Three Easy Pieces*
- Tanenbaum, A. S., & Bos, H. (2014). *Modern Operating Systems*

---

## Licencia

Este proyecto es un trabajo académico basado en xv6, el cual está bajo licencia MIT.

---

<p align="center">
  Universidad del Valle - 2025
</p>


