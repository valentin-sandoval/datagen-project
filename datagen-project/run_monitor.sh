# Script: run_monitor.sh

GENERADORES=4           # cantidad de procesos generadores
REGISTROS=50000         # cantidad total de registros
ARCHIVO="tests/salida_${REGISTROS}.csv"

function build_project() {
    echo "Compilando proyecto..."
    make clean && make
    if [ $? -ne 0 ]; then
        echo "Error en la compilación."
        exit 1
    fi
    echo "Compilación completada."
}

function run_program() {
    echo "Ejecutando datagen (${GENERADORES} generadores, ${REGISTROS} registros)..."
    ./bin/datagen -n "$GENERADORES" -m "$REGISTROS" -o "$ARCHIVO" &
    PID=$!
    echo "PID del proceso coordinador: $PID"
}

function monitor_progress() {
    echo "Monitoreando crecimiento del archivo (Ctrl+C para salir)..."
    echo "--------------------------------------------"
    watch -n 1 "date +%T && wc -l $ARCHIVO 2>/dev/null || echo 'Esperando creación del archivo...'"
}

function check_processes() {
    echo
    echo "Procesos activos:"
    ps -o pid,ppid,cmd -C datagen || echo "No hay procesos datagen activos."
}

function check_ipc() {
    echo
    echo "Recursos IPC activos en /dev/shm:"
    ls -lh /dev/shm | egrep 'shm_rec|sem\.' || echo "No se detectan recursos IPC activos."
}

function validate_output() {
    echo
    echo "Verificando integridad del CSV generado..."
    ./verify.awk "$ARCHIVO"
}

function cleanup() {
    echo
    echo "🧹 Limpiando proyecto y recursos IPC..."
    make clean
    echo "Limpieza completa."
}


build_project
run_program
sleep 1
check_processes
check_ipc
monitor_progress
validate_output
cleanup
