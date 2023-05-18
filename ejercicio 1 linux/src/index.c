#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
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

pthread_mutex_t mutex_contadores = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_hilos_en_ejecucion = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_subtramo[NUM_SUBTRAMOS];
pthread_mutex_t mutex_hombrillo;

struct vehiculo_args {
    // int id;
    int subtramo_actual;
    int sentido_actual;
    pthread_t hilo;
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
    pthread_mutex_lock(&mutex_hombrillo);
    hombrillo++;
    if (hombrillo > hombrillo_max) {
        hombrillo_max = hombrillo;
    }
    pthread_mutex_unlock(&mutex_hombrillo);

    // esperar antes de entrar al siguiente subtramo
    // Sleep(rand() % 500);

    // entrar al siguiente subtramo
    pthread_mutex_lock(&mutex_subtramo[subtramo_siguiente]);
    pthread_mutex_lock(&mutex_contadores);
    subtramo[subtramo_actual]++;
    subtramo[subtramo_siguiente]++;
    sentido[subtramo_actual][sentido_actual]++;
    sentido[subtramo_siguiente][sentido_siguiente]++;
    hombrillo--;
    tiempo_promedio_espera += abs(hombrillo * TIEMPO_ESPERA_HOMBRILLO);
    vehiculos_totales++;
    pthread_mutex_unlock(&mutex_contadores);
    pthread_mutex_unlock(&mutex_subtramo[subtramo_siguiente]);

    // salir del hombrillo
    pthread_mutex_lock(&mutex_hombrillo);
    hombrillo--;
    pthread_mutex_unlock(&mutex_hombrillo);

    free(vehiculo_data); // liberar memoria

    //signal hilo
    pthread_mutex_lock(&mutex_hilos_en_ejecucion);
    hilos_en_ejecucion--;
    pthread_mutex_unlock(&mutex_hilos_en_ejecucion);
}

int main() {
    int i;
    srand(time(0)); // inicializar semilla para generar números aleatorios

    // inicializar semáforos y mutex
    pthread_mutex_t sem_subtramo[NUM_SUBTRAMOS] = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t sem_hombrillo = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mutex_contadores = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mutex_hilos_en_ejecucion = PTHREAD_MUTEX_INITIALIZER;

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
            pthread_mutex_lock(&mutex_hilos_en_ejecucion);
            hilos_en_ejecucion++;
            pthread_mutex_unlock(&mutex_hilos_en_ejecucion);
            pthread_create(&vehiculo_data->hilo, NULL, vehiculo, vehiculo_data);
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
    printf("Tiempo promedio de espera en el hombrillo: %d ms\n", abs(hombrillo * TIEMPO_ESPERA_HOMBRILLO) / vehiculos_totales);

    int max_tramo = subtramo[0];
    int max_tramo_i = 0;
    for (i = 1; i < NUM_SUBTRAMOS; i++) {
        if (max_tramo < subtramo[i]) {
            max_tramo = subtramo[i];
            max_tramo_i = i;
        }
    }
    printf("El Tramo mas recorrido fue el tramo: %d\n", max_tramo_i + 1);

    // liberar recursos
    for (i = 0; i < NUM_SUBTRAMOS; i++) {
        pthread_mutex_destroy(&mutex_subtramo[i]);
    }
    pthread_mutex_destroy(&mutex_hombrillo);
    pthread_mutex_destroy(&mutex_contadores);
    pthread_mutex_destroy(&mutex_hilos_en_ejecucion);

    return 0;
}

