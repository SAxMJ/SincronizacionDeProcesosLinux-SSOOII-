#include <stdlib.h>
#include <stdio.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include "falonso.h"

union semun {
    int val;
    struct semid_ds *buf;
    ushort_t *array;
};

volatile sig_atomic_t fin = 0;
int semaforo = -1, zonamemoria = -1;

struct sigaction accion_sigint;
sigset_t conjunto_vacio;

void manejadora()
{
    fin = 1;
    wait(NULL);

    if (semaforo != -1)
        semctl(semaforo, 0, IPC_RMID);
    if (zonamemoria != -1)
        shmctl(zonamemoria, IPC_RMID, NULL);
}

int comprobar_cruze_sig(int carril, int desp);
int comprobar_esta_cruze(int carril, int desp);
int comprobar_sig(int desp, int carril);

int main(int argc, char *argv[])
{
    union semun sem;
    struct sembuf sops[2];
    char *pzona;
    int desp = 0, desp_antes, desp_cambio, desp_cambio2, desp_act,
        carril = 0, carril_cambio, id, color, sig, contador = 0, cambio =
        0, cruze = 0, espera_sem = 0, vel, ncoches, ret;
    pid_t pidpadre;
    int colores[] =
        { NEGRO, ROJO, VERDE, AMARILLO, MAGENTA, CYAN, BLANCO };
    int cambioCarril[137][2], i;

    for (i = 0; i < 137; i++) {
        if (i >= 0 && i <= 13)
            cambioCarril[i][CARRIL_DERECHO] = i + 137;
        else if (i >= 14 && i <= 28)
            cambioCarril[i][CARRIL_DERECHO] = i + 1 + 137;
        else if (i >= 29 && i <= 60)
            cambioCarril[i][CARRIL_DERECHO] = i + 137;
        else if (i >= 61 && i <= 62)
            cambioCarril[i][CARRIL_DERECHO] = i - 1 + 137;
        else if (i >= 63 && i <= 65)
            cambioCarril[i][CARRIL_DERECHO] = i - 2 + 137;
        else if (i >= 66 && i <= 67)
            cambioCarril[i][CARRIL_DERECHO] = i - 3 + 137;
        else if (i == 68)
            cambioCarril[i][CARRIL_DERECHO] = i - 4 + 137;
        else if (i >= 69 && i <= 129)
            cambioCarril[i][CARRIL_DERECHO] = i - 5 + 137;
        else if (i == 130)
            cambioCarril[i][CARRIL_DERECHO] = i - 3 + 137;
        else if (i >= 131 && i <= 134)
            cambioCarril[i][CARRIL_DERECHO] = i - 2 + 137;
        else if (i >= 135 && i <= 136)
            cambioCarril[i][CARRIL_DERECHO] = i - 1 + 137;

        if (i >= 0 && i <= 15)
            cambioCarril[i][CARRIL_IZQUIERDO] = i;
        else if (i >= 16 && i <= 28)
            cambioCarril[i][CARRIL_IZQUIERDO] = i - 1;
        else if (i >= 29 && i <= 58)
            cambioCarril[i][CARRIL_IZQUIERDO] = i;
        else if (i >= 59 && i <= 60)
            cambioCarril[i][CARRIL_IZQUIERDO] = i + 1;
        else if (i >= 61 && i <= 62)
            cambioCarril[i][CARRIL_IZQUIERDO] = i + 2;
        else if (i >= 63 && i <= 64)
            cambioCarril[i][CARRIL_IZQUIERDO] = i + 4;
        else if (i >= 65 && i <= 125)
            cambioCarril[i][CARRIL_IZQUIERDO] = i + 5;
        else if (i == 126)
            cambioCarril[i][CARRIL_IZQUIERDO] = i + 4;
        else if (i >= 127 && i <= 128)
            cambioCarril[i][CARRIL_IZQUIERDO] = i + 3;
        else if (i >= 129 && i <= 133)
            cambioCarril[i][CARRIL_IZQUIERDO] = i + 2;
        else if (i >= 134 && i <= 136)
            cambioCarril[i][CARRIL_IZQUIERDO] = 136;
    }

    if (argc < 3) {
        printf("no hay suficientes argumentos\n");
        printf("\nUso: ./falonso [num_coches] [velocidad]\n");
        return 1;
    }
    ncoches = atoi(argv[1]);
    if (ncoches < 1 || ncoches > 40) {
        printf("numero de coches invalido, use un numero entre 1 y 40\n");
        return 1;
    }
    ret = atoi(argv[2]);
    if (ret != 1 && ret != 0) {
        printf("velocidad invalida, introduzca 0 o 1\n");
        return 1;
    }

    if (sigemptyset(&conjunto_vacio) == -1) {
        perror("sigemptyset: ");
        return 2;
    }

	//Metemos en la máscara el conjunto vacío
    accion_sigint.sa_handler = manejadora;
    accion_sigint.sa_mask = conjunto_vacio;
    accion_sigint.sa_flags = 0;
    if (sigaction(SIGINT, &accion_sigint, NULL) == -1) {
        perror("sigaction: ");
        return 2;
    }

    pidpadre = getpid();

	//Creamos los semáforos
    semaforo = semget(IPC_PRIVATE, 285, IPC_CREAT | 0600);
    if (semaforo == -1) {
        perror("semget: ");
        return 3;
    }

	//Inicialimamos los primeros semáforos a 0
    sem.val = 0;
    if (semctl(semaforo, 1, SETVAL, sem) == -1) {
        perror("semctl (1): ");
        kill(pidpadre, SIGINT);
        return 3;
    }
    if (semctl(semaforo, 2, SETVAL, sem) == -1) {
        perror("semctl (2): ");
        kill(pidpadre, SIGINT);
        return 3;
    }
    if (semctl(semaforo, 283, SETVAL, sem) == -1) {
        perror("semctl (283): ");
        kill(pidpadre, SIGINT);
        return 3;
    }
    if (semctl(semaforo, 284, SETVAL, sem) == -1) {
        perror("semctl (284): ");
        kill(pidpadre, SIGINT);
        return 3;
    }
	
	//Inicializamos los valores de los semaforos a 1
    sem.val = 1;
    for (i = 3; i < 278; i++) {
        if (semctl(semaforo, i, SETVAL, sem) == -1) {
            perror("semctl (i): ");
            kill(pidpadre, SIGINT);
            return 3;
        }
    }
    if (semctl(semaforo, 279, SETVAL, sem) == -1) {
        perror("semctl (279): ");
        kill(pidpadre, SIGINT);
        return 3;
    }
    if (semctl(semaforo, 280, SETVAL, sem) == -1) {
        perror("semctl (280): ");
        kill(pidpadre, SIGINT);
        return 3;
    }

    if (semctl(semaforo, 281, SETVAL, sem) == -1) {
        perror("semctl (281): ");
        kill(pidpadre, SIGINT);
        return 3;
    }
    if (semctl(semaforo, 282, SETVAL, sem) == -1) {
        perror("semctl (282): ");
        kill(pidpadre, SIGINT);
        return 3;
    }

		
	//Creamos la zona de memoria compartida
    zonamemoria = shmget(IPC_PRIVATE, 400, IPC_CREAT | 0600);
    if (zonamemoria == -1) {
        perror("shmget: ");
        kill(pidpadre, SIGINT);
        return 3;
    }
    pzona = (char *) shmat(zonamemoria, 0, 0);
    for (i = 301; i < 350; i++)
        pzona[i] = 0;

    inicio_falonso(ret, semaforo, pzona);
    luz_semAforo(HORIZONTAL, ROJO);
    luz_semAforo(VERTICAL, ROJO);

    for (i = 0; i < ncoches; i++) {
        if (getpid() == pidpadre) {
            desp++;
            switch (fork()) {
            case -1:
                perror("fork (i): ");
                kill(pidpadre, SIGINT);
                return 3;
            case 0:
                srand(100 * getpid());
                color = colores[rand() % 7];
                carril = i % 2;
                vel = 40 + rand() % 50;
                inicio_coche(&carril, &desp, color);
				//Si está en el carril derecho su posicion es la actual dada por el padre en desp
                if (!carril)
                    desp_act = desp;
				//Si está en el carril izquierdo su posición es la actual más 137 posiciones de memoria
                else
                desp_act = desp + 137;
			
				//Hacemos un wait para que los coches no empiecen a moverse hasta que el padre los avise
                sops[0].sem_num = desp_act + 3;
                sops[0].sem_op = -1;
                sops[0].sem_flg = 0;
                semop(semaforo, sops, 1);

				
                if (comprobar_esta_cruze(carril, desp)) {
                    sops[0].sem_num =
                        comprobar_esta_cruze(carril, desp) + 3;
                    sops[0].sem_op = -1;
                    sops[0].sem_flg = 0;
                    semop(semaforo, sops, 1);
                }

                id = i;
		
				//El padre a hecho tantos wait como coches hay
                sops[0].sem_num = 1;
                sops[0].sem_op = 1;
                sops[0].sem_flg = 0;
                semop(semaforo, sops, 1);
                sops[0].sem_num = 2;
                sops[0].sem_op = -1;
                sops[0].sem_flg = 0;
                semop(semaforo, sops, 1);
                break;
            }
        }
    }

    if (getpid() == pidpadre) {
        luz_semAforo(HORIZONTAL, AMARILLO);
        luz_semAforo(VERTICAL, AMARILLO);
        sops[0].sem_num = 1;
        sops[0].sem_op = -ncoches;
        sops[0].sem_flg = 0;
        semop(semaforo, sops, 1);
        sops[0].sem_num = 2;
        sops[0].sem_op = ncoches;
        sops[0].sem_flg = 0;
        semop(semaforo, sops, 1);
        luz_semAforo(HORIZONTAL, VERDE);
        luz_semAforo(VERTICAL, VERDE);

        while (!fin) {
			//El semáforo de la pista pone el semáforo 279 a wait para que los coches paren
            luz_semAforo(HORIZONTAL, AMARILLO);
            sops[0].sem_num = 279;
            sops[0].sem_op = -1;
            sops[0].sem_flg = 0;
            semop(semaforo, sops, 1);
            pausa();
            luz_semAforo(HORIZONTAL, ROJO);

            i = 0;
            while (i < 50 && !fin) {
                pausa();
                i++;
            }
            if (fin)
                break;

            luz_semAforo(HORIZONTAL, AMARILLO);
            luz_semAforo(VERTICAL, AMARILLO);
            pausa();
			//El semaforo horizontal que se había puesto rojo ahora se pone en verde y realiza un signal para que los coches vuelva a moverse
			//Y acto seguido el semáforo vertical se pone en rojo realizando previamente un wait para que los coches no se muevan
            luz_semAforo(HORIZONTAL, VERDE);
            sops[0].sem_num = 279;
            sops[0].sem_op = 1;
            sops[0].sem_flg = 0;
            semop(semaforo, sops, 1);
            sops[0].sem_num = 280;
            sops[0].sem_op = -1;
            sops[0].sem_flg = 0;
            semop(semaforo, sops, 1);
            luz_semAforo(VERTICAL, ROJO);

            i = 0;
            while (i < 50 && !fin) {
                pausa();
                i++;
            }
            if (fin)
                break;

			//El semáforo vertical se pone en verde y realizamos un signal para que continuen su desplazamiento
            luz_semAforo(VERTICAL, AMARILLO);
            pausa();
            luz_semAforo(VERTICAL, VERDE);
            sops[0].sem_num = 280;
            sops[0].sem_op = 1;
            sops[0].sem_flg = 0;
            semop(semaforo, sops, 1);
        }
        for (i = 0; i < ncoches; i++)
            contador += pzona[301 + i];
        fin_falonso(&contador);
    }

    while (!fin) {
        sig = comprobar_sig(desp, carril);
        desp_cambio = cambioCarril[desp][carril];
        carril_cambio = (carril == 0) ? 1 : 0;
        if (!carril)
            desp_antes = desp;
        else
            desp_antes = desp + 137;
        if (carril_cambio)
            desp_cambio2 = desp_cambio - 137;
        else
            desp_cambio2 = desp_cambio;

        cruze = 0;
        cambio = 0;
        espera_sem = 0;

        if (pzona[sig] != ' ' && pzona[desp_cambio] == ' '
            && pzona[comprobar_sig(desp_cambio2, carril_cambio)] == ' ') {
            cambio = 1;
            if (!carril) {
                if ((desp > 19 && desp < 27)
                    || (desp > 102 && desp < 109))
                    cambio = 0;
            } else if (carril) {
                if ((desp > 19 && desp < 26)
                    || (desp > 97 && desp < 104))
                    cambio = 0;
            }
        }

        if (!carril) {
            if (desp == 20) {
                sops[0].sem_num = 280;
                sops[0].sem_op = -1;
                sops[0].sem_flg = 0;
                semop(semaforo, sops, 1);
                espera_sem = 1;
            } else if (desp == 105) {
                sops[0].sem_num = 279;
                sops[0].sem_op = -1;
                sops[0].sem_flg = 0;
                semop(semaforo, sops, 1);
                espera_sem = 1;
            }
        } else {
            if (desp == 22) {
                sops[0].sem_num = 280;
                sops[0].sem_op = -1;
                sops[0].sem_flg = 0;
                semop(semaforo, sops, 1);
                espera_sem = 1;
            } else if (desp == 98) {
                sops[0].sem_num = 279;
                sops[0].sem_op = -1;
                sops[0].sem_flg = 0;
                semop(semaforo, sops, 1);
                espera_sem = 1;
            }
        }

        if (!cambio) {
            cruze = comprobar_esta_cruze(carril, desp);

            if (comprobar_cruze_sig(carril, desp) != 0) {
                sops[0].sem_num = comprobar_cruze_sig(carril, desp) + 3;
                sops[0].sem_op = -1;
                sops[0].sem_flg = 0;
                sops[1].sem_num = sig + 3;
                sops[1].sem_op = -1;
                sops[1].sem_flg = 0;
                semop(semaforo, sops, 2);
            } else {
                sops[0].sem_num = sig + 3;
                sops[0].sem_op = -1;
                sops[0].sem_flg = 0;
                semop(semaforo, sops, 1);
            }

            avance_coche(&carril, &desp, color);

            if (espera_sem) {
                if (!carril) {
                    if (desp_antes == 20) {
                        sops[0].sem_num = 280;
                        sops[0].sem_op = 1;
                        sops[0].sem_flg = 0;
                        semop(semaforo, sops, 1);
                    } else if (desp_antes == 105) {
                        sops[0].sem_num = 279;
                        sops[0].sem_op = 1;
                        sops[0].sem_flg = 0;
                        semop(semaforo, sops, 1);
                    }
                } else {
                    if (desp_antes - 137 == 22) {
                        sops[0].sem_num = 280;
                        sops[0].sem_op = 1;
                        sops[0].sem_flg = 0;
                        semop(semaforo, sops, 1);
                    } else if (desp_antes - 137 == 98) {
                        sops[0].sem_num = 279;
                        sops[0].sem_op = 1;
                        sops[0].sem_flg = 0;
                        semop(semaforo, sops, 1);
                    }
                }
            }

            if (cruze) {
                sops[0].sem_num = cruze + 3;
                sops[0].sem_op = 1;
                sops[0].sem_flg = 0;
                sops[1].sem_num = desp_antes + 3;
                sops[1].sem_op = 1;
                sops[1].sem_flg = 0;
                semop(semaforo, sops, 2);
            } else {
                sops[0].sem_num = desp_antes + 3;
                sops[0].sem_op = 1;
                sops[0].sem_flg = 0;
                semop(semaforo, sops, 1);
            }
        } else {
            sops[0].sem_num = desp_cambio + 3;
            sops[0].sem_op = -1;
            sops[0].sem_flg = 0;
            semop(semaforo, sops, 1);
            cambio_carril(&carril, &desp, color) == -1;
            sops[0].sem_num = desp_antes + 3;
            sops[0].sem_op = 1;
            sops[0].sem_flg = 0;
            semop(semaforo, sops, 1);
        }

        velocidad(vel, carril, desp);

        if (!cambio) {
            if (!carril && desp == 133)
                pzona[301 + id]++;
            else if (carril && desp == 131)
                pzona[301 + id]++;
        }
    }

    return 0;
}

int comprobar_cruze_sig(int carril, int desp)
{
    if (desp == 20 && carril == CARRIL_DERECHO)
        return 108;
    else if (desp == 22 && carril == CARRIL_DERECHO)
        return 101 + 137;
    else if (desp == 105 && carril == CARRIL_DERECHO)
        return 33 + 137;
    else if (desp == 107 && carril == CARRIL_DERECHO)
        return 31;
    else if (desp == 22 && carril == CARRIL_IZQUIERDO)
        return 106;
    else if (desp == 24 && carril == CARRIL_IZQUIERDO)
        return 99 + 137;
    else if (desp == 98 && carril == CARRIL_IZQUIERDO)
        return 35 + 137;
    else if (desp == 100 && carril == CARRIL_IZQUIERDO)
        return 33;
    else
        return 0;
}

//Las posiciones del cruce son equivalentes, pero no iguales, por lo tanto si la posicion del coche es 
//la que tiene al bajar verticalmente por el cruce, devolvemos la posición del cruce equivalente como
//si el coche estuviese pasando de manera horizontal, de esta forma los coches pueden hacer una comprobación
//y pueden esquivarse o esperar
int comprobar_esta_cruze(int carril, int desp)
{
    if (desp == 21 && carril == CARRIL_DERECHO)
        return 108;
    else if (desp == 23 && carril == CARRIL_DERECHO)
        return 101 + 137;
    else if (desp == 106 && carril == CARRIL_DERECHO)
        return 33 + 137;
    else if (desp == 108 && carril == CARRIL_DERECHO)
        return 31;
    else if (desp == 23 && carril == CARRIL_IZQUIERDO)
        return 106;
    else if (desp == 25 && carril == CARRIL_IZQUIERDO)
        return 99 + 137;
    else if (desp == 99 && carril == CARRIL_IZQUIERDO)
        return 35 + 137;
    else if (desp == 101 && carril == CARRIL_IZQUIERDO)
        return 33;
    else
        return 0;
}

//Comprobamos la posicion de memoria siguiente, si esta en el carril derecho y en la posición 136, la siguiente posición será la 0
//Si está en la posicion 136 y en el carril izquierdo devolvemos la posición 137
//Si no es última posicion del circuito y está en el carril derecho su posición es la siguiente
//Si por el contrario está en el carril izquierdo su desplazamiento son 138 posiciones de memoria más
int comprobar_sig(int desp, int carril)
{
    if (desp == 136 && !carril)
        return 0;
    else if (desp == 136)
        return 137;
    else if (!carril)
        return desp + 1;
    else
        return desp + 138;
}
