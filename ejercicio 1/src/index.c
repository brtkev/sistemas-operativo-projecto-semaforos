#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <process.h>
#include <time.h>

#define NUM_SUBTRAMOS 4
#define CAP_SUBTRAMO_1 4
#define CAP_SUBTRAMO_2 2
#define CAP_SUBTRAMO_3 1
#define CAP_SUBTRAMO_4 3
#define CAP_HOMBRILLO 20
#define NUM_HORAS_DIA 24
#define VEHICULOS_POR_HORA 1500
#define TIEMPO_ESPERA_HOMBRILLO 500 // en milisegundos

int subtramo[NUM_SUBTRAMOS] = {0}; // contador de vehículos por subtramo
int sentido[NUM_SUBTRAMOS][2] = {0}; // contador de vehículos por subtramo y sentido
int hombrillo = 0; // contador de vehículos en el hombrillo
int hombrillo_max = 0; // número máximo de vehículos en el hombrillo
int tiempo_promedio_espera = 0; // tiempo promedio de espera en el hombrillo
int vehiculos_totales = 0; // contador de vehículos totales
int hilos_en_ejecucion = 0; // contador de hilos en ejecución

HANDLE sem_subtramo[NUM_SUBTRAMOS]; // semáforos para controlar el acceso a los subtramos
HANDLE sem_hombrillo; // semáforo para controlar el acceso al hombrillo
HANDLE mutex_contadores; // mutex para controlar el acceso a los contadores
HANDLE mutex_hilos_en_ejecucion; // mutex para controlar el acceso al contador de hilos en ejecución

struct vehiculo_args {
    // int id;
    int subtramo_actual;
    int sentido_actual;
};

void vehiculo(void* args) {
    struct vehiculo_args* vehiculo_data = (struct vehiculo_args*) args;
    //  int id = vehiculo_data->id;
    int subtramo_actual = vehiculo_data->subtramo_actual;
    int sentido_actual = vehiculo_data->sentido_actual;
    int subtramo_siguiente, sentido_siguiente;

    // determinar siguiente subtramo y sentido
    if (subtramo_actual == 1) {
        subtramo_siguiente = 2;
        sentido_siguiente = sentido_actual;
    } else if (subtramo_actual == 2) {
        subtramo_siguiente = 3;
        if (sentido_actual == 1) {
            sentido_siguiente = 0;
        } else {
            sentido_siguiente = 1;
        }
    } else if (subtramo_actual == 3) {
        subtramo_siguiente = 4;
        sentido_siguiente = sentido_actual;
    } else {
        subtramo_siguiente = 0;
        sentido_siguiente = 0;
    }

    // esperar antes de entrar al hombrillo
    // Sleep(rand() % TIEMPO_ESPERA_HOMBRILLO);
    // printf("Esperando Hombrillo...\n");

    // entrar al hombrillo
    WaitForSingleObject(sem_hombrillo, INFINITE);
    hombrillo++;
    if (hombrillo > hombrillo_max) {
        hombrillo_max = hombrillo;
    }
    ReleaseSemaphore(sem_hombrillo, 1, NULL);

    // esperar antes de entrar al siguiente subtramo
    // Sleep(rand() % 500);

    // entrar al siguiente subtramo
    WaitForSingleObject(sem_subtramo[subtramo_siguiente], INFINITE);
    WaitForSingleObject(mutex_contadores, INFINITE);
    subtramo[subtramo_actual]++;
    subtramo[subtramo_siguiente]++;
    sentido[subtramo_actual][sentido_actual]++;
    sentido[subtramo_siguiente][sentido_siguiente]++;
    hombrillo--;
    tiempo_promedio_espera += abs(hombrillo * TIEMPO_ESPERA_HOMBRILLO);
    vehiculos_totales++;
    ReleaseMutex(mutex_contadores);
    ReleaseSemaphore(sem_subtramo[subtramo_siguiente], 1, NULL);

    // salir del hombrillo
    WaitForSingleObject(sem_hombrillo, INFINITE);
    hombrillo--;
    ReleaseSemaphore(sem_hombrillo, 1, NULL);

    free(vehiculo_data); // liberar memoria

    //signal hilo
    WaitForSingleObject(mutex_hilos_en_ejecucion, INFINITE);
    hilos_en_ejecucion--;
    ReleaseMutex(mutex_hilos_en_ejecucion);
}

int main() {
    int i;
    srand(time(0)); // inicializar semilla para generar números aleatorios

    // inicializar semáforos y mutex
    sem_subtramo[0] = CreateSemaphore(NULL, CAP_SUBTRAMO_1, CAP_SUBTRAMO_1, NULL);
    sem_subtramo[1] = CreateSemaphore(NULL, CAP_SUBTRAMO_2, CAP_SUBTRAMO_2, NULL);
    sem_subtramo[2] = CreateSemaphore(NULL, CAP_SUBTRAMO_3, CAP_SUBTRAMO_3, NULL);
    sem_subtramo[3] = CreateSemaphore(NULL, CAP_SUBTRAMO_4, CAP_SUBTRAMO_4, NULL);
    sem_hombrillo = CreateSemaphore(NULL, CAP_HOMBRILLO, CAP_HOMBRILLO, NULL);
    mutex_contadores = CreateMutex(NULL, FALSE, NULL);
    mutex_hilos_en_ejecucion = CreateMutex(NULL, FALSE, NULL);

    // generar vehículos
    for (i = 1; i <= NUM_HORAS_DIA; i++) {
        int j;
        for (j = 0; j < VEHICULOS_POR_HORA; j++) {

            //genera subtramos y sentidos iniciales
            int subtramo_inicial = (rand() % (NUM_SUBTRAMOS + 1)) - 1;
            int sentido_inicial = rand() % 2;

            //generar nuevo vehiculo
            struct vehiculo_args* vehiculo_data = (struct vehiculo_args*) malloc(sizeof(struct vehiculo_args));
            // vehiculo_data->id = i * VEHICULOS_POR_HORA + j;
            vehiculo_data->subtramo_actual = subtramo_inicial;
            vehiculo_data->sentido_actual = sentido_inicial;

            //crear y wait nuevo hilo 
            WaitForSingleObject(mutex_hilos_en_ejecucion, INFINITE);
            hilos_en_ejecucion++;
            ReleaseMutex(mutex_hilos_en_ejecucion);
            _beginthread(vehiculo, 0, vehiculo_data);
        }
        printf("hour number: %d\n", i);
        Sleep(500); // esperar un segundo antes de generar los vehículos de la siguiente hora
    }

    // esperar a que terminen todos los vehículos
    while (hilos_en_ejecucion > 0) {
        Sleep(1000);
    }

    // imprimir resultados
    printf("Vehículos totales: %d\n", vehiculos_totales);
    printf("Contador de vehículos por subtramo:\n");
    for (i = 0; i < NUM_SUBTRAMOS; i++) {
        printf("Subtramo %d: %d\n", i + 1, subtramo[i]);
    }
    printf("Contador de vehículos por subtramo y sentido:\n");
    for (i = 0; i < NUM_SUBTRAMOS; i++) {
        printf("Subtramo %d, sentido 0: %d\n", i + 1, sentido[i][0]);
        printf("Subtramo %d, sentido 1: %d\n", i + 1, sentido[i][1]);
    }
    printf("Número máximo de vehículos en el hombrillo: %d\n", hombrillo_max);
    printf("Tiempo promedio de espera en el hombrillo: %d ms\n", abs(hombrillo * tiempo_promedio_espera / vehiculos_totales));

    int max_tramo = subtramo[0];
    int max_tramo_i = 0;
    for (i = 1; i < NUM_SUBTRAMOS; i++){
        if( max_tramo < subtramo[i]){
            max_tramo = subtramo[i];
            max_tramo_i = i;
        }
    }
    printf("El Tramo mas recorrido fue el tramo: %d", max_tramo_i + 1);

    // liberar recursos
    for (i = 0; i < NUM_SUBTRAMOS; i++) {
        CloseHandle(sem_subtramo[i]);
    }
    CloseHandle(sem_hombrillo);
    CloseHandle(mutex_contadores);

    return 0;
}