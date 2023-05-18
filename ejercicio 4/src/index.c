#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

#define MAX_PANTALLAS 4
#define MAX_MESONEROS 20
#define MAX_MESAS 20
#define MAX_SUPERVISORES 2
#define MAX_PEDIDOS 100

// Declaración de semáforos y mutexes
HANDLE semCaja;
HANDLE semPantallas[MAX_PANTALLAS];
HANDLE semPedidoListo;
HANDLE semPlanilla;
HANDLE semDescanso;
HANDLE mutexCaja;
HANDLE mutexPedidos;
HANDLE mutexNumPedidosPorSupervisor[MAX_SUPERVISORES];
HANDLE mutexNumPedidosPorMesonero;
HANDLE mutexNumCobrosPorMesonero;
HANDLE mutexNumDescansosPorMesonero;

// Variables globales
int numPedidos = 0;
int numPedidosPorSupervisor[MAX_SUPERVISORES][2] = {{0, 0}, {0, 0}}; // [supervisor][turno]
int numPedidosPorMesonero[MAX_MESONEROS] = {0};
int numCobrosPorMesonero[MAX_MESONEROS] = {0};
int numDescansosPorMesonero[MAX_MESONEROS] = {0};
int turnoActual = 0;
int numMesonerosEnCaja = 0;
int numMesonerosDescansando = 0;

// Función para tomar un pedido de una mesa y registrarlo en una pantalla
void tomarPedido(int numMesa, int numMesonero) {
    WaitForSingleObject(mutexPedidos, INFINITE);
    int numPedido = ++numPedidos;
    numPedidosPorMesonero[numMesonero]++;
    ReleaseMutex(mutexPedidos);
    printf("Mesonero %d tomando pedido en mesa %d (pedido %d)\n", numMesonero, numMesa, numPedido);
    WaitForSingleObject(semCaja, INFINITE);
    WaitForSingleObject(mutexCaja, INFINITE);
    numMesonerosEnCaja++;
    printf("Mesonero %d en caja para cobrar (pedido %d)\n", numMesonero, numPedido);
    ReleaseMutex(mutexCaja);
    ReleaseSemaphore(semCaja, 1 ,NULL);
}

// Función para llevar un pedido a una mesa cuando está listo en la taquilla
void llevarPedido(int numPedido) {
    printf("Pedido %d listo para llevar a la mesa\n", numPedido);
    ReleaseSemaphore(semPedidoListo, 1, NULL);
}

// Función para cobrar a un cliente en la caja
void cobrar(int numMesonero) {
    WaitForSingleObject(mutexCaja, INFINITE);
    numMesonerosEnCaja--;
    numCobrosPorMesonero[numMesonero]++;
    ReleaseMutex(mutexCaja);
    printf("Mesonero %d cobrando en caja\n", numMesonero);
}

// Función para que un mesonero tome un descanso cada 10 pedidos
void tomarDescanso(int numMesonero) {
    WaitForSingleObject(mutexNumDescansosPorMesonero, INFINITE);
    ReleaseSemaphore(semDescanso, 1, NULL);
    printf("Mesonero %d tomando un descanso\n", numMesonero);
    numDescansosPorMesonero[numMesonero]++;
    Sleep(1000);
    ReleaseSemaphore(mutexNumDescansosPorMesonero, 1, NULL);
}

// Función para que un supervisor actualice la planilla con el número de pedidos de un mesonero
void actualizarPlanilla(int numMesonero) {
    WaitForSingleObject(mutexNumPedidosPorMesonero, INFINITE);
    int numPedidos = numPedidosPorMesonero[numMesonero];
    ReleaseMutex(mutexNumPedidosPorMesonero);
    WaitForSingleObject(semPlanilla, INFINITE);
    WaitForSingleObject(mutexNumPedidosPorSupervisor[turnoActual], INFINITE);
    numPedidosPorSupervisor[0][turnoActual] += numPedidos;
    numPedidosPorSupervisor[1][turnoActual] += numPedidos;
    ReleaseMutex(mutexNumPedidosPorSupervisor[turnoActual]);
    printf("Supervisor actualizando planilla con los pedidos del mesonero %d (turno %d)\n", numMesonero, turnoActual);
}

// Función que será ejecutada por cada hilo de mesonero
DWORD WINAPI mesoneroFunction(LPVOID lpParam) {
    int numMesonero = *(int*)lpParam;
    int numMesa = numMesonero % MAX_MESAS + 1;

    while (numPedidos < MAX_PEDIDOS) {
        // Tomar un pedido de una mesa
        tomarPedido(numMesa, numMesonero);

        // Registrar el pedido en una pantalla
        int numPantalla = numMesonero % MAX_PANTALLAS;
        WaitForSingleObject(semPantallas[numPantalla], INFINITE);
        printf("Mesonero %d registrando pedido en pantalla %d (pedido %d)\n", numMesonero, numPantalla, numPedidos);
        // Sleep(100); // Simular tiempo de registro en pantalla
        ReleaseSemaphore(semPantallas[numPantalla], 1, NULL);

        // Llevar el pedido a la mesa cuando está listo en la taquilla
        WaitForSingleObject(semPedidoListo, INFINITE);
        printf("Mesonero %d llevando pedido a la mesa (pedido %d)\n", numMesonero, numPedidos);
        ReleaseSemaphore(semPedidoListo, 1, NULL);

        // Cobrar en caja
        cobrar(numMesonero);

        // Tomar un descanso cada 10 pedidos
        if (numPedidos % 10 == 0) {
            tomarDescanso(numMesonero);
        }

        // Actualizar la planilla cada vez que se cambia de turno
        if (numPedidos % (MAX_PEDIDOS / 2) == 0) {
            actualizarPlanilla(numMesonero);
        }
    }
    printf("Mesonero %d Saliendo\n", numMesonero);
    return 0;
}

// Función que será ejecutada por cada hilo de supervisor
DWORD WINAPI supervisorFunction(LPVOID lpParam) {
    int numSupervisor = *(int*)lpParam;

    while (numPedidos < MAX_PEDIDOS) {
        // Esperar a que un mesonero avise para tomar un descanso
        printf("Supervisor %d Esperando a que un mesonero tome descanso\n", numSupervisor);
        WaitForSingleObject(semDescanso, INFINITE);
        numMesonerosDescansando++;

        // Llenar la planilla con el número de pedidos de un mesonero
        WaitForSingleObject(mutexNumPedidosPorSupervisor[turnoActual], INFINITE);
        int numPedidos = numPedidosPorSupervisor[numSupervisor][turnoActual];
        
        printf("Supervisor %d llenando planilla con los pedidos del turno %d (pedidos: %d)\n", numSupervisor, turnoActual, numPedidos);
        // Sleep(500); // Simular tiempo de llenado de planilla

        // Cambiar de turno si todos los mesoneros descansaron
        if (numMesonerosDescansando == MAX_MESONEROS) {
            numMesonerosDescansando = 0;
            turnoActual = (turnoActual + 1) % 2;
            printf("Supervisor %d cambiando de turno (turno %d)\n", numSupervisor, turnoActual);
            // Sleep(2000); // Simular tiempo de cambio de turno
        }
        ReleaseMutex(mutexNumPedidosPorSupervisor[turnoActual]);
    }
    printf("Supervisor %d saliendo de turno (turno %d)\n", numSupervisor, turnoActual);
    return 0;
}

int main() {
    // Inicialización de semáforos y mutexes
    semCaja = CreateSemaphore(NULL, 2, 2, NULL);
    for (int i = 0; i < MAX_PANTALLAS; i++) {
        semPantallas[i] = CreateSemaphore(NULL, 1, 1, NULL);
    }
    semPedidoListo = CreateSemaphore(NULL, 1, MAX_PEDIDOS, NULL);
    semPlanilla = CreateSemaphore(NULL, 1, 1, NULL);
    semDescanso = CreateSemaphore(NULL, 0, MAX_MESONEROS, NULL);
    mutexCaja = CreateMutex(NULL, FALSE, NULL);
    mutexPedidos = CreateMutex(NULL, FALSE, NULL);
    for (int i = 0; i < MAX_SUPERVISORES; i++) {
        mutexNumPedidosPorSupervisor[i] = CreateMutex(NULL, FALSE, NULL);
    }
    mutexNumPedidosPorMesonero = CreateMutex(NULL, FALSE, NULL);
    mutexNumCobrosPorMesonero = CreateMutex(NULL, FALSE, NULL);
    mutexNumDescansosPorMesonero = CreateMutex(NULL, FALSE, NULL);

    // Creación de hilos de mesoneros
    HANDLE hMesoneros[MAX_MESONEROS];
    int numMesoneros[MAX_MESONEROS];
    for (int i = 0; i < MAX_MESONEROS; i++) {
        numMesoneros[i] = i;
        hMesoneros[i] = CreateThread(NULL, 0, mesoneroFunction, &numMesoneros[i], 0,NULL);
    }

    // Creación de hilos de supervisores
    HANDLE hSupervisores[MAX_SUPERVISORES];
    int numSupervisores[MAX_SUPERVISORES];
    for (int i = 0; i < MAX_SUPERVISORES; i++) {
        numSupervisores[i] = i;
        hSupervisores[i] = CreateThread(NULL, 0, supervisorFunction, &numSupervisores[i], 0, NULL);
    }
    
    // Esperar a que todos los hilos terminen
    while(numPedidos < MAX_PEDIDOS){
        Sleep(1000);
    }


    // Cerrar los semáforos, mutexes y hilos
    CloseHandle(semCaja);
    for (int i = 0; i < MAX_PANTALLAS; i++) {
        CloseHandle(semPantallas[i]);
    }
    CloseHandle(semPedidoListo);
    CloseHandle(semPlanilla);
    CloseHandle(semDescanso);
    CloseHandle(mutexCaja);
    CloseHandle(mutexPedidos);
    for (int i = 0; i < MAX_SUPERVISORES; i++) {
        CloseHandle(mutexNumPedidosPorSupervisor[i]);
        CloseHandle(hSupervisores[i]);
    }
    for (int i = 0; i < MAX_MESONEROS; i++){
        CloseHandle(hMesoneros[i]);
    }
    CloseHandle(mutexNumPedidosPorMesonero);
    CloseHandle(mutexNumCobrosPorMesonero);
    CloseHandle(mutexNumDescansosPorMesonero);

    // Imprimir resultados
    int numPedidosPorTurno[MAX_SUPERVISORES] = {0};
    int numPedidosMaxPorMesonero = 0;
    int numCobrosMaxPorMesonero = 0;
    int numPedidosMaxMesonero = 0;
    for (int i = 0; i < MAX_MESONEROS; i++) {
        if (numPedidosPorMesonero[i] > numPedidosMaxPorMesonero) {
            numPedidosMaxPorMesonero = numPedidosPorMesonero[i];
            numPedidosMaxMesonero = i;
        }
        if (numCobrosPorMesonero[i] > numCobrosMaxPorMesonero) {
            numCobrosMaxPorMesonero = numCobrosPorMesonero[i];
        }
    }
    for (int i = 0; i < MAX_SUPERVISORES; i++) {
        for (int j = 0; j < 2; j++) {
            numPedidosPorTurno[i] += numPedidosPorSupervisor[i][j];
        }
    }
    printf("Número total de pedidos contabilizados por cada supervisor:\n");
    for (int i = 0; i < MAX_SUPERVISORES; i++) {
        printf("Supervisor %d: %d\n", i, numPedidosPorTurno[i]);
    }
    printf("Número total de pedidos entregados por cada mesonero:\n");
    for (int i = 0; i < MAX_MESONEROS; i++) {
        printf("Mesonero %d: %d\n", i, numPedidosPorMesonero[i]);
    }
    printf("Número total de cobros procesados por cada mesonero:\n");
    for (int i = 0; i < MAX_MESONEROS; i++) {
        printf("Mesonero %d: %d\n", i, numCobrosPorMesonero[i]);
    }
    printf("Número total de descansos tomados por cada mesonero:\n");
    for (int i = 0; i < MAX_MESONEROS; i++) {
        printf("Mesonero %d: %d\n", i, numDescansosPorMesonero[i]);
    }
    printf("El mesonero con más pedidos entregados es el número %d con %d pedidos.\n", numPedidosMaxMesonero, numPedidosMaxPorMesonero);
    printf("El número máximo de cobros procesados por un mesonero en un turno es %d.\n", numCobrosMaxPorMesonero);

    return 0;
}