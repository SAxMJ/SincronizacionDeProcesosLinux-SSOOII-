#include <stdlib.h>
#include <stdio.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "falonso.h"

union semun {
  int             val;
  struct semid_ds *buf;
  ushort_t        *array;
};

struct sigaction accion_nueva, accion_vieja;
sigset_t conjunto_vacio, conjunto_SIGINT, conjunto_viejo, conjunto_sin_SIGINT;
int contador;

void manejadora() {
  printf("SIGINT enviada\n");
}

int comprobar_cambio_carril(int carril, int desp);

int main() {
  int semaforo;
  struct sembuf sops[1];
  int zonamemoria;
  char *pzona;
  union semun sem;
  int carril = CARRIL_DERECHO;
  int desp = 10;
  int color, sig, contador = 0, i, sigcambio;
  pid_t pidpadre;
  int colores[] = {NEGRO, ROJO, VERDE, AMARILLO, MAGENTA, CYAN, BLANCO};

  sigemptyset(&conjunto_SIGINT);
  sigaddset(&conjunto_SIGINT, SIGINT);
  sigprocmask(SIG_BLOCK, &conjunto_SIGINT, &conjunto_viejo);
  conjunto_sin_SIGINT=conjunto_viejo;
  sigdelset(&conjunto_sin_SIGINT, SIGINT);
   
  sigemptyset(&conjunto_vacio);
  
  accion_nueva.sa_handler=manejadora;
  accion_nueva.sa_mask=conjunto_vacio;
  accion_nueva.sa_flags=0;
  sigaction(SIGINT, &accion_nueva, &accion_vieja);
  
  zonamemoria=shmget(IPC_PRIVATE, 400, IPC_CREAT | 0600);

  //1º) Creamos el semáforo
  semaforo=semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);

  //2º) Inicializamos con valor a 1
  sem.val = 1;
  semctl(semaforo, 0, SETVAL, sem);

  pzona = (char *) shmat(zonamemoria, 0, 0);
  pzona[350] = 1;
  
  inicio_falonso(1, semaforo, pzona);
  luz_semAforo(HORIZONTAL, VERDE);
  luz_semAforo(VERTICAL, VERDE);

  pidpadre = getpid();  

  for(i = 0; i < 12; i++) {
    if(getpid() == pidpadre){
      desp = desp + 2;
      switch(fork()) {
        case 0 :  srand(100*getpid());
                  color = colores[rand() % 8];
				  carril=rand () % (1-0+1) + 0;
                  inicio_coche(&carril, &desp, color);
                  //sleep(1);
                  break;
      }
    }
  }

//sleep(1);

  if(getpid() == pidpadre) {
    sigsuspend(&conjunto_sin_SIGINT);
    pzona[350] = 0;
    fin_falonso(&contador);
    semctl(semaforo, 0, IPC_RMID);
    shmctl(zonamemoria, IPC_RMID, NULL);
    sigaction(SIGINT, &accion_vieja, NULL);
    sigprocmask(SIG_SETMASK, &conjunto_viejo, NULL);
  }
  
  while(pzona[350]) {
    if(desp == 136)
      sig = 0;
    else if(desp == 273)
      sig = 137;
    else
      sig = desp+1;

    while(pzona[sig] != ' ') {
 
      sigcambio = comprobar_cambio_carril(carril, desp);



      if(pzona[sigcambio] == ' ' &&
        pzona[sigcambio + 1] == ' ' && pzona[sigcambio - 1]== ' ') {
        //sops[0].sem_num=1;
        //sops[0].sem_op=-1;
        //semop(semaforo, sops, 1);
        cambio_carril(&carril, &desp, color);
      } else {
     	
		//Wait
        sops[0].sem_num=0;
        sops[0].sem_op=-1; //espera a que desp+1 este libre
        sops[0].sem_flg=0;
        //printf("EL VALOR DEL SEMAFORO ES: %d\n", semctl(semaforo, 0, GETVAL));

		//AQUI HE AÑADIDO EL &SOPS[0], ANTES SOLO SOPS
		semop(semaforo,sops,1);
        //semop(semaforo,&sops[0], 1);
      }
    }

	avance_coche(&carril, &desp, color);
    //velocidad(99, carril, desp);
/*	
	sops[0].sem_num=0;
    sops[0].sem_op=-1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
    
   
    //printf("valor: %d\n", semctl(semaforo, 0, GETVAL));
    //velocidad(99, carril, desp);
    
	*/
	velocidad(rand() % 1 + 99, carril, desp);
	//Signal
	sops[0].sem_num=0;
    sops[0].sem_op=1;
	sops[0].sem_flg=0;
    semop(semaforo, sops, 1);

    
    //pausa();

	
  }

  return 0;
}

int comprobar_cambio_carril(int carril, int desp) {
  int cambioCarril[137][2], i , j;

  for(i = 0; i<137; i++) {
    j = i + 137;

    if(i >= 0 && i <= 13)
      cambioCarril[i][CARRIL_IZQUIERDO] = j;
    else if(i >= 14 && i <= 28)
      cambioCarril[i][CARRIL_IZQUIERDO] = j + 1;
    else if(i >= 29 && i <= 60)
      cambioCarril[i][CARRIL_IZQUIERDO] = j;
    else if(i >= 61 && i <= 62)
      cambioCarril[i][CARRIL_IZQUIERDO] = j - 1;
    else if(i >= 63 && i <= 65)
      cambioCarril[i][CARRIL_IZQUIERDO] = j - 2;
    else if(i >= 66 && i <= 67)
      cambioCarril[i][CARRIL_IZQUIERDO] = j - 3;
    else if(i == 68)
      cambioCarril[i][CARRIL_IZQUIERDO] = j - 4;
    else if(i >= 69 && i <= 129)
      cambioCarril[i][CARRIL_IZQUIERDO] = j - 5;
    else if(i == 130)
      cambioCarril[i][CARRIL_IZQUIERDO] = j - 3;
    else if(i >= 131 && i <= 134)
      cambioCarril[i][CARRIL_IZQUIERDO] = j - 2;
    else if(i >= 135 && i <= 136)
      cambioCarril[i][CARRIL_IZQUIERDO] = j - 1;

    if(i >= 0 && i <= 15)
      cambioCarril[i][CARRIL_DERECHO] = i;
    else if(i >= 16 && i <= 28)
      cambioCarril[i][CARRIL_DERECHO] = i - 1;
    else if(i >= 29 && i <= 58)
      cambioCarril[i][CARRIL_DERECHO] = i;
    else if(i >= 59 && i <= 60)
      cambioCarril[i][CARRIL_DERECHO] = i + 1;
    else if(i >= 61 && i <= 62)
      cambioCarril[i][CARRIL_DERECHO] = i + 2;
    else if(i >= 63 && i <= 64)
      cambioCarril[i][CARRIL_DERECHO] = i + 4;
    else if(i >= 65 && i <= 125)
      cambioCarril[i][CARRIL_DERECHO] = i + 5;
    else if(i == 126)
      cambioCarril[i][CARRIL_DERECHO] = i + 4;
    else if(i >= 127 && i <= 128)
      cambioCarril[i][CARRIL_DERECHO] = i + 3;
    else if(i >= 129 && i <= 133)
      cambioCarril[i][CARRIL_DERECHO] = i + 2;
    else if(i >= 134 && i <= 136)
      cambioCarril[i][CARRIL_DERECHO] = 136;
  }

  if(carril == CARRIL_DERECHO) {
    return cambioCarril[desp][CARRIL_IZQUIERDO];
  } else if(carril == CARRIL_IZQUIERDO){
    return cambioCarril[desp][CARRIL_DERECHO];
  } else {
    return -1;
  }
}
