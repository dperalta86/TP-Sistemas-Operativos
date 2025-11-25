## Módulo: Worker
El Módulo Worker en el contexto de nuestro trabajo práctico será el encargado de ejecutar las diferentes Queries, de una a la vez.

### Lineamiento e Implementación
Cada Worker al momento de iniciarse deberá poder recibir como parámetro el archivo de configuración y un identificador propio del Worker, el cuál se encuentra detallado más abajo
→ ./bin/worker [archivo_config] [ID Worker]

Al momento de iniciar el Worker, el mismo deberá conectarse al módulo Storage y una vez realizada la conexión deberá conectarse al módulo Master indicando su ID a fin de que el Master pueda identificarlo.
Para poder ejecutar las Queries el módulo Worker contará con una Memoria Interna y un Query Interpreter.

Worker
Query
Interpreter
Memoria
Interna
Master
Storage

### Query Interpreter
El componente de Query Interpreter, será el encargado de ir ejecutando cada una de las instrucciones de las Queries. Estas acciones podrán interactuar directamente sobre el componente de Memoria Interna y/o comunicarse con el Módulo de Storage.
Para esto, el Worker tendrá un path indicado en el archivo de configuración donde se encontrarán todas las Queries. Al recibir del Master la solicitud de ejecutar una nueva Query, deberá buscar el archivo en este Path para parsearlo a partir del PC indicado. De ahí en más, se deberán ejecutar una por una las instrucciones, respetando el retardo configurado.

Los parámetros de las instrucciones se separan por espacios. En el caso de los tags de los Files², siempre se separarán del nombre del File por ":".
El Query interpreter admite las siguientes instrucciones:

### Instrucciones
#### CREATE
Formato: `CREATE <NOMBRE_FILE>:<TAG>`

La instrucción CREATE solicitará al módulo Storage la creación de un nuevo File con el Tag recibido por parámetro y con tamaño 0.

#### TRUNCATE
Formato: `TRUNCATE <NOMBRE_FILE>: <TAG> <TAMAÑO>`

La instrucción TRUNCATE solicitará al módulo Storage la modificación del tamaño del File y Tag indicados, asignando el tamaño recibido por parámetro (deberá ser múltiplo del tamaño de bloque).

#### WRITE
Formato: `WRITE <NOMBRE_FILE>: <TAG> <DIRECCIÓN BASE> <CONTENIDO>`

La instrucción WRITE escribirá en la Memoria Interna los bytes correspondientes a partir de la dirección base del File:Tag. En caso de que la Memoria Interna no cuente con todas las páginas necesarias para satisfacer la operación, deberá solicitar el contenido faltante al módulo Storage.

#### READ
Formato: `READ <NOMBRE_FILE>: <TAG> <DIRECCIÓN BASE> <ΤΑΜΑΝΟ>`

La instrucción READ leerá de la Memoria Interna los bytes correspondientes a partir de la dirección base del File y Tag pasados por parámetro, y deberá enviar dicha información al módulo Master. En caso de que la Memoria Interna no cuente con todas las páginas necesarias para satisfacer la operación, deberá solicitar el contenido faltante al módulo Storage.

#### TAG
Formato: `TAG <NOMBRE_FILE_ORIGEN>: <TAG_ORIGEN>
<NOMBRE_FILE_DESTINO>: <TAG_DESTINO>`

La instrucción TAG solicitará al módulo Storage la creación un nuevo File:Tag a partir del File y Tag origen pasados por parámetro.
² Para diferenciar los archivos nativos de linux de los archivos que va a gestionar nuestro Filesystem, en todo el enunciado vamos a nombrarlos como "archivos" a los primeros y como "Files" a los segundos

#### COMMIT
Formato: `COMMIT <NOMBRE_FILE>:<TAG>`

La instrucción COMMIT, le indicará al Storage que no se realizarán más cambios sobre el File y Tag pasados por parámetro.

#### FLUSH
Formato: `FLUSH <NOMBRE_FILE>: <TAG>`

Persistirá todas las modificaciones realizadas en Memoria Interna de un File:Tag en el Storage.
Nota: Esta instrucción también deberá ser ejecutada implícitamente bajo las siguientes situaciones:
* Previo a la ejecución de un COMMIT.
* Previo a realizar el desalojo de la Query del Worker (para todos los File:Tag eventualmente modificados)

#### DELETE
Formato: `DELETE <NOMBRE_FILE>: <TAG>`

La instrucción DELETE solicitará al módulo Storage la eliminación del File:Tag correspondiente.

#### END
Formato: `END`

Esta instrucción da por finalizada la Query y le informa al módulo Master el fin de la misma.

### Ejemplo de archivo de Query
```
CREATE MATERIAS: BASE
TRUNCATE MATERIAS: BASE 1024
WRITE MATERIAS: BASE SISTEMAS_OPERATIVOS
FLUSH MATERIAS: BASE
COMMIT MATERIAS: BASE
READ MATERIAS: BASE 0 8
TAG MATERIAS: BASE MATERIAS:V2
DELETE MATERIAS: BASE
WRITE MATERIAS: V2 SISTEMAS OPERATIVOS_2
COMMIT MATERIAS: V2
END
```

### Memoria Interna
El componente de la Memoria Interna estará implementado mediante un único malloc() que contendrá la información que el Query Interpreter necesite leer y/o escribir a lo largo de las diferentes ejecuciones.

El tamaño de la Memoria Interna vendrá delimitado por archivo de configuración y no se podrá cambiar en tiempo de ejecución.
Se utilizará un esquema de paginación simple a demanda con memoria virtual, donde el tamaño de página estará definido por el tamaño de bloque del Storage, este dato se debe obtener al momento de hacer el Handshake con el Storage. De esta manera, se deberá mantener una tabla de páginas por cada File:Tag de los que tenga al menos una página presente.
Para cada referencia lectura o escritura del Query Interpreter a una página de la Memoria Interna, se deberá esperar un tiempo definido por archivo de configuración (RETARDO_MEMORIA).

### Algoritmos de Reemplazo de páginas
En caso de que la Memoria Interna esté llena y deba cargar una nueva página, se deberá elegir una víctima para ser reemplazada. La elección de la víctima se podrá realizar mediante los algoritmos **LRU** o **CLOCK-M** definido por archivo de configuración y el mismo no se modificará en tiempo de ejecución. La víctima podrá ser una página correspondiente a cualquier File: Tag que se encuentre presente en la Memoria Interna del Worker.
Si la página víctima se encuentra modificada, se deberá previamente impactar el cambio en el módulo Storage, teniendo en cuenta que los números de página están directamente relacionados al número del bloque lógico dentro del File:Tag.

### Logs mínimos y obligatorios
* Recepción de Query: "## Query <QUERY_ID>: Se recibe la Query. El path de operaciones es: <PATH_QUERY>"
* Fetch Instrucción: "## Query <QUERY_ID>: FETCH Program Counter: <PROGRAM_COUNTER> <INSTRUCCIÓN>³".
* Instrucción realizada: "## Query <QUERY_ID>: Instrucción realizada: <INSTRUCCIÓN>⁴"
* Desalojo de Query: "## Query <QUERY_ID>: Desalojada por pedido del Master"
* Lectura/Escritura Memoria: "Query <QUERY_ID>: Acción: <LEER / ESCRIBIR> Dirección Física: <DIRECCION_FISICA> Valor: <VALOR LEIDO / ESCRITO>".
* Asignar Marco: "Query <QUERY_ID>: Se asigna el Marco: <NUMERO_MARCO> a la Página: <NUMERO_PAGINA> perteneciente al File: <FILE> Tag: <TAG>".
* Liberación de Marco: "Query <QUERY_ID>: Se libera el Marco: <NUMERO_MARCO> perteneciente al File: <FILE> Tag: <TAG>".
* Reemplazo Algoritmo: "## Query <QUERY_ID>: Se reemplaza la página <File1: Tag1>/<NUM_PAG1> por la <File2: Tag2><NUM_PAG2>"
* Bloque faltante en Memoria: "Query <QUERY_ID>: Memoria Miss File: <FILE> - Tag: <TAG> Pagina: <NUMERO_PAGINA>"
* Bloque ingresado en Memoria: "Query <QUERY_ID>: Memoria Add File: <FILE> Tag: <TAG> Pagina: <NUMERO_PAGINA> Marco: <NUMERO_MARCO>"
³ Los parámetros no se deben loguear
⁴ Los parámetros no se deben loguear

### Archivo de Configuración
| Campo | Tipo | Descripción |
| :--- | :--- | :--- |
| IP_MASTER | String | IP del módulo Master |
| PUERTO_MASTER | Numérico | Puerto del módulo Master |
| IP_STORAGE | String | IP del módulo Storage |
| PUERTO_STORAGE | Numérico | Puerto del módulo Storage |
| TAM_MEMORIA | Numérico | Tamaño expresado en bytes de la Memoria Interna. |
| RETARDO_MEMORIA | Numérico | Tiempo en milisegundos que deberá esperarse ante cada lectura y/o escritura en la Memoria. |
| ALGORITMO_REEMPLAZO | String | Algoritmo de reemplazo para las páginas. LRU/CLOCK-M |
| PATH_QUERIES | String | Path donde se encuentran los archivos de Queries |
| LOG_LEVEL | String | Nivel de detalle máximo a mostrar. Compatible con log level from string() |

### Ejemplo de Archivo de Configuración
```
IP_MASTER = 127.0.0.1 
PUERTO_MASTER = 9001 
IP_STORAGE = 127.0.0.1 
PUERTO_STORAGE = 9002 
TAM_MEMORIA = 4096 
RETARDO_MEMORIA = 1500 
ALGORITMO_REEMPLAZO = LRU 
PATH_SCRIPTS=/home/utnso/queries 
LOG_LEVEL INFO
```
