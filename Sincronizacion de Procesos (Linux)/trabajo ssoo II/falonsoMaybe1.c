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

int main(int argc, char *argv[]) {
  int semaforo,semaforoInicio;
  struct sembuf sops[1];
  struct sembuf sopsInicio[1];
  int zonamemoria;
  char *pzona;
  union semun sem;
  union semun semInicio;
  int carril = CARRIL_DERECHO;
  int desp = 10;
  int color, sig, contador = 0, i, sigcambio,nProcesos;
  pid_t pidpadre;
  int colores[] = {NEGRO, ROJO, VERDE, AMARILLO, MAGENTA, CYAN, BLANCO};
  int nCoches;
  int nMax,nMin;
  int veloC;

	
	
  //COMPROBACIÓN DE ARGUMENTOS
  if(argc!=3){
  perror("Solo se puede introducir un argumento para el numero de coches y otro para la velocidad\n");
  return 0;
  }else{
	nCoches=numeroDeCoches(argv);
	if(nCoches!=1&&nCoches!=2&&nCoches!=3&&nCoches!=4&&nCoches!=5&&nCoches!=6&&nCoches!=7&&nCoches!=8&&nCoches!=9
	   &&nCoches!=10&&nCoches!=11&&nCoches!=12&&nCoches!=13&&nCoches!=14&&nCoches!=15&&nCoches!=16&&nCoches!=17&&nCoches!=18
	   &&nCoches!=19&&nCoches!=20){
	printf("El numero de cohes introducido no es correcto (MIN=1 & MAX=20)\n");
	return 0;
	}

	if(strcmp(argv[2],"1")==0){
	nMin=1;
	nMax=99;
	}else if(strcmp(argv[2],"0")==0){
	 nMin=100;
	 nMax==100;
	 }else{
	  printf("Es necesario que el argumento de la velocidad sea 1 (para normal) 0 (para rapido)\n");
	  return 0;
	  }
	}
  

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

  if((zonamemoria=shmget(IPC_PRIVATE, 400, IPC_CREAT | 0600))==-1){
  perror("No se pudo crear la zona de memoria compartida");
  return 0;
  }

  if((semaforo=semget(IPC_PRIVATE, 1, IPC_CREAT | 0600))==-1){
  perror("No se pudo  crear el primer semaforo");
  return 0;
  }

  if((semaforoInicio=semget(IPC_PRIVATE, 1, IPC_CREAT |0600))==-1){
  perror("No se pudo crear el segundo semaforo");
  return 0;
  }

  //sem.val = 1;
  //semctl(semaforo, 0, SETVAL, sem);
  sem.val = 1;
  if((semctl(semaforo, 1, SETVAL, sem))==-1){
  perror("Error al iniciar el primer semaforo");
  //return 0;
  }

  semInicio.val= 1;
  if((semctl(semaforoInicio, 1, SETVAL, sem))==-1){
  perror("Error al iniciar el segundo semaforo");
  //return 0;
  }

  
  pzona = (char *) shmat(zonamemoria, 0, 0);
  pzona[350] = 1;
  inicio_falonso(1, semaforo, pzona);
  luz_semAforo(HORIZONTAL, VERDE);
  luz_semAforo(VERTICAL, VERDE);

  pidpadre = getpid();

  for(i = 0; i < nCoches; i++) {
    if(getpid() == pidpadre){
      desp = desp + 8;
		
      switch(fork()) {

		case -1:  perror("Se ha producido un error al crear uno de los coches");
				  return 0;

        case 0 :  srand(100*getpid());
          	  color = colores[rand() % 7];
		  veloC=rand () % (nMax-nMin+1) + nMin;
                  inicio_coche(&carril, &desp, color);
                  break;
      }
    }
  }

   sleep(2);
	//SEMAFORO DEL PADRE
  

  if(getpid() == pidpadre) {
    sigsuspend(&conjunto_sin_SIGINT);
    pzona[350] = 0;
    fin_falonso(&contador);
    semctl(semaforo, 0, IPC_RMID);
    semctl(semaforoInicio, 0, IPC_RMID);
    shmctl(zonamemoria, IPC_RMID, NULL);
    sigaction(SIGINT, &accion_vieja, NULL);
    sigprocmask(SIG_SETMASK, &conjunto_viejo, NULL);
  }

  //sleep(1);

  while(pzona[350]) {
    if(desp == 136)
      sig = 0;
    else if(desp == 273)
      sig = 137;
    else
      sig = desp+1;

	


//patata
    while(pzona[sig] != ' ' || (comprobar_cruze(carril, desp) != 0 
          && pzona[comprobar_cruze(carril, desp)] != ' ')) {
      sigcambio = comprobar_cambio_carril(carril, desp);
      if(pzona[sigcambio] == ' ') {
        sops[0].sem_num=1;
        sops[0].sem_op=-1;
        semop(semaforo, sops, 1);
        cambio_carril(&carril, &desp, color);
        sops[0].sem_num=1;
        sops[0].sem_op=1;
        semop(semaforo, sops, 1);
        //break;
        if(sigcambio == 136)
          sig = 0;
        else if(sigcambio == 273)
          sig = 137;
        else
          sig = sigcambio+1;
      } else {
        sops[0].sem_num=1;
        sops[0].sem_op=0; //espera a que desp+1 este libre
        sops[0].sem_flg=0;
        //printf("valor: %d\n", semctl(semaforo, 0, GETVAL));
        semop(semaforo, sops, 1);
      }
    }

    
    sops[0].sem_num=1;
    sops[0].sem_op=-1;
    sops[0].sem_flg=0;

    semop(semaforo, sops, 1);

    avance_coche(&carril, &desp, color);
   
    sops[0].sem_num=1;
    sops[0].sem_op=1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);

    velocidad(veloC, carril, desp);
    
    if(carril == CARRIL_DERECHO && desp == 133)
      pzona[351]++;
    else if(carril == CARRIL_IZQUIERDO && desp == 131)
      pzona[351]++;
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


int comprobar_cruze(int carril, int desp) {
  if(desp == 21 && carril == CARRIL_DERECHO)
    return 108;
  else if(desp == 105 && carril == CARRIL_DERECHO)
    return 23+137;
  else if(desp == 107 && carril == CARRIL_DERECHO)
    return 21;
  else if(desp == 22+137 && carril == CARRIL_IZQUIERDO)
    return 106;
  else if(desp == 24+137 && carril == CARRIL_IZQUIERDO)
    return 99+137;
  else if(desp == 98+137 && carril == CARRIL_IZQUIERDO)
    return 25+137;
  else if(desp == 100+137 && carril == CARRIL_IZQUIERDO)
    return 23;
  else if(desp == 25 && carril == CARRIL_IZQUIERDO)
	return 99;
  else
    return 0;
}


int numeroDeCoches (char *argv[]){

	int nCoches;

	if(strcmp(argv[1],"1")==0){
	nCoches=1;
	}else if(strcmp(argv[1],"2")==0){
	nCoches=2;
	}else if(strcmp(argv[1],"3")==0){
	nCoches=3;
	}else if(strcmp(argv[1],"4")==0){
	nCoches=4;
	}else if(strcmp(argv[1],"5")==0){
	nCoches=5;
	}else if(strcmp(argv[1],"6")==0){
	nCoches=6;
	}else if(strcmp(argv[1],"7")==0){
	nCoches=7;
	}else if(strcmp(argv[1],"8")==0){
	nCoches=8;
	}else if(strcmp(argv[1],"9")==0){
	nCoches=9;
	}else if(strcmp(argv[1],"10")==0){
	nCoches=10;
	}else if(strcmp(argv[1],"11")==0){
	nCoches=11;
	}else if(strcmp(argv[1],"12")==0){
	nCoches=12;
	}else if(strcmp(argv[1],"13")==0){
	nCoches=13;
	}else if(strcmp(argv[1],"14")==0){
	nCoches=14;
	}else if(strcmp(argv[1],"15")==0){
	nCoches=15;
	}else if(strcmp(argv[1],"16")==0){
	nCoches=16;
	}else if(strcmp(argv[1],"17")==0){
	nCoches=17;
	}else if(strcmp(argv[1],"18")==0){
	nCoches=18;
	}else if(strcmp(argv[1],"19")==0){
	nCoches=19;
	}else if(strcmp(argv[1],"20")==0){
	nCoches=20;
	}
return nCoches;
}
