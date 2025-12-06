# Informe: Planificador con Prioridades, Aging y Desalojo

## Escenario de Prueba
- Queries: 4 (AGING_1, AGING_2, AGING_3, AGING_4)
- Workers: 2
- Prioridades iniciales: Query 0 (4), Query 1 (3), Query 2 (5), Query 3 (1)
- Algoritmo: PRIORITY con desalojo
- Tiempo de aging: 5000ms (Nota: Menor número = Mayor prioridad)


## 1. Desalojo por prioridad
### Caso: Query 0 desalojada por Query 3

```bash
[INFO] 03:06:49:623 MASTER: ## Se desaloja la Query id: 0 (<PRIORIDAD: 4>) del Worker <WORKER_ID: 1>
[INFO] 03:06:50:616 worker_1: ## Query 0: Desalojada por pedido del Master - PC=1
[DEBUG] 03:06:50:617 MASTER: Query ID=0, PC=1
[INFO] 03:06:50:617 MASTER: ## Query ID=0 desalojada exitosamente - PC guardado: 1 - Vuelta a READY
[INFO] 03:06:50:617 MASTER: [try_dispatch] Asignada Query ID=3 al Worker ID=1
```

✅ Resultado: Query 0 (prioridad 4) es correctamente desalojada por Query 3 (prioridad 1, mejor). El PC se preserva en 1 y Query 3 toma el worker inmediatamente.

## 2. Aging Funciona Correctamente

```bash
[INFO] 03:06:53:125 MASTER: ##<QUERY_ID: 2> Cambio de prioridad: <PRIORIDAD_ANTERIOR: 5> - <PRIORIDAD_NUEVA: 4>
[INFO] 03:06:55:626 MASTER: ##<QUERY_ID: 0> Cambio de prioridad: <PRIORIDAD_ANTERIOR: 4> - <PRIORIDAD_NUEVA: 3>
[INFO] 03:06:58:127 MASTER: ##<QUERY_ID: 2> Cambio de prioridad: <PRIORIDAD_ANTERIOR: 4> - <PRIORIDAD_NUEVA: 3>
[INFO] 03:07:00:628 MASTER: ##<QUERY_ID: 0> Cambio de prioridad: <PRIORIDAD_ANTERIOR: 3> - <PRIORIDAD_NUEVA: 2>
```

✅ Resultado: El aging se aplica cada 5 segundos exactos, decrementando la prioridad de queries en READY state. Las prioridades nunca bajan de 0.

## 3. Desalojo Inmediato Tras Aging
```bash
[INFO] 03:07:00:628 MASTER: ##<QUERY_ID: 0> Cambio de prioridad: <PRIORIDAD_ANTERIOR: 3> - <PRIORIDAD_NUEVA: 2>
[INFO] 03:07:00:629 MASTER: ## Se desaloja la Query id: 1 (<PRIORIDAD: 3>) del Worker <WORKER_ID: 2>
```

✅ Resultado: Inmediatamente después de que Query 0 mejora su prioridad a 2 (mejor que Query 1 con 3), el master desaloja Query 1. Reacción instantánea del planificador.

## 4. Program Counter (PC) Preservado
### Query 0 - Múltiples desalojos  
```bash
[INFO] 03:06:50:616 worker_1: ## Query 0: Desalojada - PC=1
[INFO] 03:07:02:201 worker_2: ## Query 0: Se recibe la Query con PC=1
[INFO] 03:07:10:225 worker_2: ## Query 0: Desalojada - PC=3
[INFO] 03:07:21:263 worker_2: ## Query 0: Se recibe la Query con PC=3
```

### Query 1 - Reanudación correcta
```bash
[INFO] 03:07:02:200 worker_2: ## Query 1: Desalojada - PC=9
[INFO] 03:07:18:448 worker_1: ## Query 1: FETCH Program Counter: 9 READ METROID_SERIES:V1 576 26
```

✅ Resultado: El PC se guarda correctamente en cada desalojo y las queries reanudan exactamente desde donde fueron interrumpidas.

## 5. Sin Solicitudes de Desalojo Repetidas
```bash
[INFO] 03:07:00:629 MASTER: ## Se desaloja la Query id: 1 del Worker 2
[DEBUG] 03:07:02:200 MASTER: Recibido OP_WORKER_EVICT_RES en socket 6
[INFO] 03:07:02:201 MASTER: ## Query ID=1 desalojada exitosamente
```

✅ Resultado: Una sola solicitud de desalojo por query. El flag `preemption_pending` previene solicitudes duplicadas hasta recibir `OP_WORKER_EVICT_RES`.

## 6. Worker Liberado y Reasignado Inmediatamente
```bash
[DEBUG] 03:06:50:617 MASTER: Worker ID=1 liberado y movido a IDLE
[INFO] 03:06:50:617 MASTER: [try_dispatch] Asignada Query ID=3 al Worker ID=1
[INFO] 03:06:50:617 MASTER: [send_query_to_worker] Query ID=3 enviada al Worker ID=1 (socket=5)
```

✅ Resultado: Al recibir `OP_WORKER_EVICT_RES`, el master libera el worker y ejecuta `try_dispatch()` automáticamente, asignando la siguiente query de mayor prioridad sin demora.

## Conclusión
El planificador con prioridades, aging y desalojo funciona correctamente:

✅ Desalojos se realizan solo cuando es necesario (mejor prioridad en READY)  
✅ Program Counter preservado y restaurado correctamente  
✅ Aging aplicado cada 5 segundos exactos  
✅ Ready queue reordenada tras cada cambio de prioridad  
✅ Sin solicitudes duplicadas de desalojo  
✅ Workers reutilizados inmediatamente tras desalojo  
✅ Queries ejecutan en orden correcto de prioridad  

Estado final: Todas las queries completan su ejecución respetando el algoritmo de planificación por prioridades con aging y desalojo.