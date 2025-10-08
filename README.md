# datagen-projectTP – Generador de Datos con Procesos (SHM + Semáforos POSIX)

Carrera: Ing. en Informática — Materia: Sistemas Operativos
Lenguaje: C (GNU/Linux) — IPC: Memoria compartida POSIX + Semáforos POSIX nombrados

1) ¿Qué hace?

Un coordinador asigna IDs y escribe un CSV. N generadores piden IDs de a 10, crean registros aleatorios y los envían uno por vez al coordinador mediante SHM sincronizada con semáforos.

IDs deben ser correlativos y sin duplicados (verificados con verify.awk).
Se debe poder monitorear: procesos, SHM, semáforos y crecimiento del CSV.

2) Estructura del proyecto
datagen-project/
├── src/
│   └── datagen.c          # código fuente (coordinador + generadores)
├── bin/                   # ejecutable (se crea con make)
├── tests/                 # CSV de prueba (salidas)
├── Makefile               # compila, run, verify, clean
├── verify.awk             # valida IDs (duplicados/correlativos)
├── run_monitor.sh         # script: compila + ejecuta + monitorea + valida + limpia
├── README.md              # este archivo
└── .gitignore             # NO subir binarios ni CSV


Importante: la entrega en MIeL debe ser un ZIP sin binarios (make clean antes de comprimir).

3) Requisitos del sistema
sudo apt update
sudo apt install -y build-essential gawk htop


gcc (C11), make, gawk

Linux con soporte para POSIX SHM y semáforos POSIX (-pthread -lrt)

4) Compilar

Desde la raíz del proyecto:

cd datagen-project
make


Salida esperada:

Compilación completa -> bin/datagen

5) Ejecutar (modo “manual”)

Generar 100 registros con 4 generadores:

./bin/datagen -n 4 -m 100 -o tests/salida_100.csv


Ver primeras líneas y cantidad:

head tests/salida_100.csv
wc -l tests/salida_100.csv      # 101 (100 + encabezado)


Validación con AWK:

chmod +x verify.awk              # 1 sola vez si hace falta
./verify.awk tests/salida_100.csv
# Sin duplicados: SI
# Correlativos: SI

6) Monitoreo (en vivo)

Abrí otra terminal en paralelo mientras corre una corrida grande:

./bin/datagen -n 4 -m 50000 -o tests/salida_50000.csv &
ps -o pid,ppid,cmd -C datagen                   # procesos coordinador + generadores
ls -lh /dev/shm | egrep 'shm_rec|sem\.'         # SHM + semáforos POSIX activos
watch -n 1 "wc -l tests/salida_50000.csv"       # crecimiento del CSV
htop                                            # uso de CPU/memoria (opcional)


Si ps no muestra nada o watch dice “No such file”, es porque el programa ya terminó o el archivo aún no existe. Lanzá la ejecución antes y con & (background).

7) Script automático (recomendado): run_monitor.sh

Ejecuta todo: compila, corre, muestra PID, lista IPC, abre watch, valida y limpia.

chmod +x run_monitor.sh
./run_monitor.sh


Configuración dentro del script:

GENERADORES=4
REGISTROS=50000
ARCHIVO="tests/salida_${REGISTROS}.csv"

8) Limpieza

Borra ejecutables y CSV generados:

make clean


Verificá que no queden IPC del programa:

ls -lh /dev/shm | egrep 'shm_rec|sem\.'
# (no debería listar nada del programa al finalizar bien)

9) Ayuda y validación de parámetros

El programa muestra ayuda si faltan parámetros o con -h:

./bin/datagen -h
# Uso: ./bin/datagen -n <generadores> -m <total_registros> -o <salida.csv>

10) Entrega en MIeL (ZIP sin binarios)

Limpiar:

make clean


Crear ZIP (desde datagen-project/):

zip -r TP_SO_Datagen.zip src Makefile verify.awk run_monitor.sh README.md tests .gitignore


Subir TP_SO_Datagen.zip a MIeL.

Checklist de corrección:

 Compila con make sin warnings críticos

 Corre y genera CSV con encabezado + registros

 verify.awk → “Sin duplicados: SI / Correlativos: SI”

 Se evidencia concurrencia e IPC (ps, /dev/shm, watch)

 make clean deja el repo sin binarios ni CSV

11) Troubleshooting rápido

ps -C datagen no muestra nada: el proceso ya terminó. Ejecutá con & y monitoreá enseguida.

watch ... No such file: el CSV aún no existe. Ejecutá primero el binario (en background) y luego watch.

./verify.awk: Permission denied: chmod +x verify.awk o awk -f verify.awk tests/salida_100.csv.

Quedaron IPC colgados (corte con kill -9):

ls -lh /dev/shm | egrep 'shm_rec|sem\.'
# y borrar con cuidado usando sudo si fuese necesario

12) Notas de diseño (para defensa)

Buffer de 1 elemento en SHM → sem_empty(1) / sem_full(0) (productor–consumidor 1x1).

sem_mx_rec protege la región crítica del buffer.

sem_mx_ids serializa la asignación de IDs y el salto por bloques de 10.

El coordinador escribe el CSV en el orden recibido (no es necesario ordenar).

SIGINT/SIGTERM limpian recursos (sem_unlink, shm_unlink) para no dejar basura en /dev/shm.