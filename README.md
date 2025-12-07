# TP - Sistemas Operativos

[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

> Sistema distribuido que simula la coordinación entre múltiples componentes: planificación de procesos, gestión de memoria virtual con paginación a demanda, y un filesystem concurrente. Desarrollado como trabajo práctico de Sistemas Operativos en UTN FRBA.

## 📋 Contexto Académico

Este proyecto fue desarrollado como trabajo práctico integrador de la materia **Sistemas Operativos** de la Universidad Tecnológica Nacional - Facultad Regional Buenos Aires (UTN FRBA), durante el segundo cuatrimestre de 2025.

**⚠️ Aviso Importante**: Este repositorio se comparte con fines demostrativos y educativos. Si sos estudiante de la materia, te recomiendo usar este proyecto solo como referencia para comprender conceptos y si tenés dudas consulta con los ayudante de la cátedra.

## 🎯 Objetivo del Proyecto

Desarrollar una solución que permita la simulación de un sistema distribuido donde se deben:
- Planificar y ejecutar queries de manera concurrente
- Gestionar memoria mediante paginación virtual a demanda
- Administrar un filesystem propio con acceso concurrente
- Coordinar la comunicación entre múltiples procesos distribuidos

## 🏗️ Arquitectura del Sistema

El sistema está compuesto por cuatro módulos principales que interactúan mediante sockets:

```
┌─────────────────┐
│  Query Control  │ (múltiples instancias)
└────────┬────────┘
         │
         v
    ┌────────┐
    │ Master │ (planificador central)
    └───┬────┘
        │
    ┌───┴───────────┐
    │               │
┌───v────┐    ┌────v───┐    ┌────────┐
│ Worker │    │ Worker │ ...│ Worker │
└───┬────┘    └────┬───┘    └────┬───┘
    │              │             │
    └──────────┬───┴─────────────┘
               v
          ┌─────────┐
          │ Storage │ (filesystem)
          └─────────┘
```

### Módulos

#### 🎮 Query Control
- Cliente que envía queries al sistema
- Se conecta al Master y espera resultados
- Puede haber múltiples instancias simultáneas
- Cada query tiene una prioridad asociada

#### 🧠 Master
- Planificador central del sistema
- Recibe y encola queries de los Query Control
- Implementa algoritmos de planificación:
  - **FIFO**: First In, First Out
  - **Prioridades**: Con aging y desalojo
- Distribuye queries a Workers disponibles
- Reenvía resultados a los Query Control correspondientes

#### ⚙️ Worker
- Ejecuta las queries asignadas por el Master
- Componentes internos:
  - **Query Interpreter**: Ejecuta instrucciones secuencialmente
  - **Memoria Interna**: Gestión de memoria virtual con paginación
- Implementa algoritmos de reemplazo de páginas:
  - **LRU** (Least Recently Used)
  - **CLOCK-M** (Clock Modificado)
- Se comunica con Storage para operaciones de archivo

#### 💾 Storage
- Sistema de archivos propio (filesystem)
- Servidor multihilo que atiende peticiones concurrentes
- Persiste datos entre ejecuciones
- Maneja operaciones de lectura/escritura de forma segura

## 🔧 Tecnologías y Conceptos Implementados

### Sistemas Operativos
- **Planificación de procesos**: FIFO, prioridades con aging
- **Memoria virtual**: Paginación a demanda
- **Algoritmos de reemplazo**: LRU, CLOCK-M
- **Concurrencia**: Manejo de múltiples hilos y sincronización
- **Sistemas de archivos**: Diseño e implementación de FS custom

### Programación en C
- Gestión manual de memoria (`malloc`, `free`)
- Comunicación mediante **sockets TCP**
- **Serialización** de mensajes para comunicación entre procesos
- **Hilos POSIX** (`pthread`) para concurrencia
- **Mutex y semáforos** para sincronización

### Bibliotecas Utilizadas
- `so-commons-library`: Biblioteca provista por la cátedra
- `pthread`: POSIX threads para concurrencia
- Sockets UNIX para comunicación entre procesos
- Bibliotecas estándar de C

## 📦 Estructura del Proyecto

```
.
├── README.md
├── LICENSE
├── docs/
│   ├── enunciado.pdf              # Enunciado completo del TP
│   ├── arquitectura.md            # Documentación técnica detallada
│   ├── implementacion.md          # Decisiones de diseño y algoritmos
│   └── diagramas/
│       └── arquitectura-general.png
├── pruebas-catedra/               # Suite de pruebas provista por la cátedra
└── src/
    ├── query-control/             # Código del módulo Query Control
    ├── master/                    # Código del módulo Master
    ├── worker/                    # Código del módulo Worker
    └── storage/                   # Código del módulo Storage
```

## 🚀 Compilación y Ejecución

### Requisitos Previos
- GCC (GNU Compiler Collection)
- Make
- Biblioteca `so-commons-library`
- Sistema operativo Linux/UNIX

### Compilación

```bash
# Compilar módulo específico (ejemplo master)
cd master
make
```

### Ejecución

El sistema debe iniciarse en el siguiente orden para respetar las dependencias:

```bash
# 1. Iniciar Storage
cd storage
./bin/storage <archivo_config>

# 2. Iniciar Master
cd master
./bin/master <archvo_config>

# 3. Iniciar Workers (pueden ser múltiples instancias)
cd worker
./bin/worker <archivo_config> <id>

# 4. Enviar queries mediante Query Control
cd query_control
./query_control <archivo_config> <archivo_query> <prioridad>
```

### Configuración

Cada módulo cuenta con su propio archivo de configuración donde se especifican parámetros como:
- IPs y puertos de conexión
- Algoritmos de planificación/reemplazo
- Tamaños de memoria y páginas
- Rutas de archivos y logs

## 📊 Pruebas

El proyecto incluye las pruebas oficiales provistas por la cátedra en el directorio `pruebas-catedra/`. El sistema ha sido validado y **aprobó exitosamente todas las pruebas**, así como la defensa oral del trabajo práctico.

## 📚 Documentación Adicional

Para información técnica detallada sobre la implementación, fundamentos teóricos y decisiones de diseño, consultar:

- [**Arquitectura Detallada**](docs/arquitectura.md): Descripción profunda de cada módulo y sus interacciones
- [**Implementación**](docs/implementacion.md): Algoritmos, estructuras de datos y desafíos técnicos
- [**Enunciado Original**](docs/enunciado.pdf): Especificación completa del trabajo práctico

## 👥 Equipo de Desarrollo

Obviamente este proyecto no fue desarrollado solo por mi, fue desarrolado de manera colaborativa junto a:
- Nicolás
- Carlos
- David
- Agustín

Sin la **colaboración y dedicación** de todo el equipo, este trabajo no hubiera sido posible. Cada miembro contribuyó significativamente al éxito del proyecto.

## 📄 Licencia

Este proyecto está bajo la Licencia MIT. Ver el archivo [LICENSE](LICENSE) para más detalles.

## 🙏 Créditos

- **UTN FRBA - Cátedra de Sistemas Operativos**: Por el enunciado, las pruebas y la biblioteca `so-commons-library`
- **Equipo docente**: Por el acompañamiento durante el desarrollo del trabajo práctico

---

**Nota**: El enunciado y pruebas fueron provistos por la cátedra de Sistemas Operativos de UTN FRBA. Este repositorio contiene la implementación realizada por el equipo de desarrollo durante el 2° cuatrimestre de 2025.
