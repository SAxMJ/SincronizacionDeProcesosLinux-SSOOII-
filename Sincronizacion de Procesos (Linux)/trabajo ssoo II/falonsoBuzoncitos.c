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
int salBuc=1;


void manejadora() {
  printf("SIGINT enviada\n");
  salBuc=0;
}

int comprobar_cambio_carril(int carril, int desp);
int comprobar_cruze(int carril, int desp);
int esta_cruze(int carril, int desp);
int comprobar_sig(int desp, int carril);
int comprobar_ant(int desp, int carril);

int main(int argc, char *argv[]) {
  int semaforo, buzon;
  struct sembuf sops[1];
  int zonamemoria;
  char *pzona, id;
  union semun sem;
  int carril = CARRIL_DERECHO;
  int desp = 10, desp_antes, desp_cambio;
  int color, sig, contador = 0, i, cambio = 0, vel, ncoches;
  pid_t pidpadre;
  int colores[] = {NEGRO, ROJO, VERDE, AMARILLO, MAGENTA, CYAN, BLANCO};
  int tmp, entra, cruze;
  int posSemV;

  struct tipo_mensaje {
	long pid_destino; //En pid_destino almacenamos el pid del proceso en cuestión
	long tipo;
  } m;
  
  if(argc < 3) {
    printf("no hay suficientes argumentos\n");
    return 1;
  }
  ncoches = atoi(argv[1]);

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

  semaforo=semget(IPC_PRIVATE, 3, IPC_CREAT | 0600);

  //sem.val = 1;
  //semctl(semaforo, 0, SETVAL, sem);
  sem.val = 0;
  if(semctl(semaforo, 1, SETVAL, sem) == -1) {
    perror("semctl: ");
    return -1;
  }
  sem.val = 0;
  if(semctl(semaforo, 2, SETVAL, sem) == -1) {
    perror("semctl: ");
    return -1;
  }
  sem.val=0;
  if(semctl(semaforo, 2, SETVAL, sem) == -1) {
    perror("semctl: ");
    return -1;
  }

  zonamemoria=shmget(IPC_PRIVATE, 800, IPC_CREAT | 0600);
  pzona = (char *) shmat(zonamemoria,0, 0);
  pzona[350] = 1;
  pzona[351] = 0;
  for(i = 399; i < 800; i++)
    pzona[i] = -1;

  inicio_falonso(1, semaforo, pzona);
  luz_semAforo(HORIZONTAL, VERDE);
  luz_semAforo(VERTICAL, VERDE);

  buzon=msgget(IPC_PRIVATE, IPC_CREAT | 0600);

  pidpadre = getpid();
    
  for(i = 0; i < 12; i++) {
    if(getpid() == pidpadre){
      desp += 2;
      switch(fork()) {
        case 0 :  srand(100*getpid());
                  color = colores[rand() % 7];
                  //carril = i % 2;
                  //if(i == 14) desp = 9;
                  if(atoi(argv[2]))
                    vel =rand () % (99-1+1) + 1;
                  else
                    vel = 100;
                  //if(i == 0) vel = 9;
                  inicio_coche(&carril, &desp, color);
                  id = i;
                  if(!carril)
                    pzona[comprobar_ant(desp, carril)+400] = id;
                  else
                  pzona[comprobar_ant(desp, carril)+400+137] = id;
                  sops[0].sem_num=1;
                  sops[0].sem_op=1;
                  sops[0].sem_flg=0;
                  semop(semaforo, sops, 1);
                  sops[0].sem_num=2;
                  sops[0].sem_op=-1;
                  sops[0].sem_flg=0;
                  semop(semaforo, sops, 1);
                  break;
      }
    }
  }

  if(getpid() == pidpadre) {
    sops[0].sem_num=1;
    sops[0].sem_op=-18;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
    sops[0].sem_num=2;
    sops[0].sem_op=18;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);




	sigprocmask(SIG_SETMASK, &conjunto_sin_SIGINT,&conjunto_SIGINT);
	while(salBuc==1){
	if(salBuc==0){
	break;
	}

	luz_semAforo(VERTICAL,VERDE);
	luz_semAforo(HORIZONTAL, ROJO);
	

	sleep(5);
		

	luz_semAforo(VERTICAL,AMARILLO);
	sleep(1);
	sops[0].sem_num=0;
    sops[0].sem_op=1;
    sops[0].sem_flg=0;
	semop(semaforo, sops, 1);
	luz_semAforo(VERTICAL, ROJO);
	
	luz_semAforo(HORIZONTAL,VERDE);
	sleep(5);

	luz_semAforo(HORIZONTAL, AMARILLO);
	sleep(1);
	sops[0].sem_num=0;
    sops[0].sem_op=1;
    sops[0].sem_flg=0;
	semop(semaforo, sops, 1);

	}

    pzona[350] = 0;
    contador = pzona[351];
    fin_falonso(&contador);
    semctl(semaforo, 0, IPC_RMID);
    shmctl(zonamemoria, IPC_RMID, NULL);
    msgctl(buzon, IPC_RMID, NULL);
    sigaction(SIGINT, &accion_vieja, NULL);
    sigprocmask(SIG_SETMASK, &conjunto_viejo, NULL);


  }
	
	
  while(pzona[350]) {
    if(desp == 136)
      sig = 0;
    else
      sig = desp+1;
      
    cruze = 0;
    entra = 0;
	//Semáforo carril verticalCI
	if(carril==CARRIL_IZQUIERDO){
	while(pzona[275]==ROJO && sig==22){
	sops[0].sem_num=0;
    sops[0].sem_op=-1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	sops[0].sem_num=0;
    sops[0].sem_op=1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	}
	}
	
	//Semáforo carril verticalCD
	if(carril==CARRIL_DERECHO){
	while(pzona[275]==ROJO && sig==20){
	sops[0].sem_num=0;
    sops[0].sem_op=-1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	sops[0].sem_num=0;
    sops[0].sem_op=1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	}
	}

	//Semáforo carril verticalCD
	if(carril==CARRIL_DERECHO){
	while(pzona[275]==AMARILLO && sig==19){
	sops[0].sem_num=0;
    sops[0].sem_op=-1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	sops[0].sem_num=0;
    sops[0].sem_op=1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	}
	}
	
	//Semáforo carril horizontalCD
	if(carril==CARRIL_DERECHO){	
	while((pzona[274]==ROJO && sig==106)){
	sops[0].sem_num=0;
    sops[0].sem_op=-1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	sops[0].sem_num=0;
    sops[0].sem_op=1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	}
	}

	if(carril==CARRIL_DERECHO){	
	while((pzona[274]==AMARILLO && sig==105)){
	sops[0].sem_num=0;
    sops[0].sem_op=-1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	sops[0].sem_num=0;
    sops[0].sem_op=1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	}
	}

	if(carril==CARRIL_IZQUIERDO){	
	while((pzona[274]==ROJO && sig==101)){
	sops[0].sem_num=0;
    sops[0].sem_op=-1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	sops[0].sem_num=0;
    sops[0].sem_op=1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	}
	}

	if(carril==CARRIL_IZQUIERDO){	
	while(pzona[274]==ROJO && sig==96){
	sops[0].sem_num=0;
    sops[0].sem_op=-1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	sops[0].sem_num=0;
    sops[0].sem_op=1;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
	}
	}
	
    //Indicamos cual va a ser la siguiente posicion del proceso
    m.pid_destino = sig;
    //Si la siguiente posición está ocupada
    if(pzona[sig] != ' ') {
      entra = 1;
      //Recibimos el mensaje que bloqueará el coche
      msgrcv(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), m.pid_destino, 0);
	}/*else if(pzona[sig] != ' ' && pzona[desp+137]==' '){
	 
	 m.pid_destino = desp+137;
	 cambio_carril(&carril, &desp, color);
	 }*/
    else {
      //Si la posición está libre, leemos el mensaje pero sin bloquear el coche
      msgrcv(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), m.pid_destino, IPC_NOWAIT);
    }
    //fprintf(stderr, "rcv: %d, ent: %d\n", m.pid_destino, entra);
    

	//Hacemos una comprobación para ver si nos encontramos en el cruce
    if(comprobar_cruze(carril, desp) != 0){
		//Si nos encontramos en una posicion del cruce la posicion
		//de destino del mensaje será la posición del cruce mas 500 posicionesd e memoria
      m.pid_destino = comprobar_cruze(carril, desp) + 500;

		//Si la posicion de memoria está ocupada, entonces, mientras que 
		//la posicion siga ocupada el coche sigue parado
      if(pzona[comprobar_cruze(carril, desp)] != ' ') {
        //entra = 1;
        while(pzona[comprobar_cruze(carril, desp)] != ' ')
		  //Se pausa el coche
          msgrcv(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), m.pid_destino, 0);
		//Si la posicion no se encuentra ocupada recibimos el mensaje pero no lo detenemos
      } else {
		//Con ipcnowait el coche recibe mensaje pero no se para
        msgrcv(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), m.pid_destino, IPC_NOWAIT);
      }
    }
    
	//Si esta en el carril derecho guarda en pid_destino la posicion en la que se encuentra
    if(!carril)
      m.pid_destino = desp;
	//Si está en el carril izquierdo la guarda la guarda en la posicion equivalente sumándole 137 posiciones
    else
      m.pid_destino = desp+137;

	//Si esta en una posición del cruce, devuelve la posición que es un numero positivo, si no, devuelve un 0  
    cruze = esta_cruze(carril, desp);

	//Avanza el coche
    if(avance_coche(&carril, &desp, color) == -1){
      //fprintf(stderr, "(%d) pos: %d ent: %d\n", m.pid_destino, desp, entra);
    }


	//Envia el mensaje en el que indica la posicion en la que se encuentra
    msgsnd(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), 0);
    //fprintf(stderr, "snd: %d\n", m.pid_destino);

	//Si está en una posición del cruce
    if(cruze) {
	  //Enviará el mensaje 500 posiciones de memoria mas allá
      m.pid_destino = cruze + 500;
      msgsnd(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), 0);
    }

    velocidad(vel, carril, desp);

    if(carril == CARRIL_DERECHO && desp == 133)
      pzona[351]++;
    else if(carril == CARRIL_IZQUIERDO && desp == 131)
      pzona[351]++;
  }
  return 0;
}

int comprobar_cambio_carril(int carril, int desp) {
  int cambioCarril[137][2], i , j;

  for(i = 0; i<137; i++) {
    if(i >= 0 && i <= 13)
      cambioCarril[i][CARRIL_IZQUIERDO] = i;
    else if(i >= 14 && i <= 28)
      cambioCarril[i][CARRIL_IZQUIERDO] = i + 1;
    else if(i >= 29 && i <= 60)
      cambioCarril[i][CARRIL_IZQUIERDO] = i;
    else if(i >= 61 && i <= 62)
      cambioCarril[i][CARRIL_IZQUIERDO] = i - 1;
    else if(i >= 63 && i <= 65)
      cambioCarril[i][CARRIL_IZQUIERDO] = i - 2;
    else if(i >= 66 && i <= 67)
      cambioCarril[i][CARRIL_IZQUIERDO] = i - 3;
    else if(i == 68)
      cambioCarril[i][CARRIL_IZQUIERDO] = i - 4;
    else if(i >= 69 && i <= 129)
      cambioCarril[i][CARRIL_IZQUIERDO] = i - 5;
    else if(i == 130)
      cambioCarril[i][CARRIL_IZQUIERDO] = i - 3;
    else if(i >= 131 && i <= 134)
      cambioCarril[i][CARRIL_IZQUIERDO] = i - 2;
    else if(i >= 135 && i <= 136)
      cambioCarril[i][CARRIL_IZQUIERDO] = i - 1;

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
  if(desp == 20 && carril == CARRIL_DERECHO)
    return 108;
  else if(desp == 22 && carril == CARRIL_DERECHO)
    return 101+137;
  else if(desp == 105 && carril == CARRIL_DERECHO)
    return 23+137;
  else if(desp == 107 && carril == CARRIL_DERECHO)
    return 21;
  else if(desp == 22 && carril == CARRIL_IZQUIERDO)
    return 106;
  else if(desp == 24 && carril == CARRIL_IZQUIERDO)
    return 99+137;
  else if(desp == 98 && carril == CARRIL_IZQUIERDO)
    return 25+137;
  else if(desp == 100 && carril == CARRIL_IZQUIERDO)
    return 23;
  else
    return 0;
}

int esta_cruze(int carril, int desp){
  if(desp == 21 && carril == CARRIL_DERECHO)
    return 21;
  else if(desp == 23 && carril == CARRIL_DERECHO)
    return 23+137;
  else if(desp == 106 && carril == CARRIL_DERECHO)
    return 106+137;
  else if(desp == 108 && carril == CARRIL_DERECHO)
    return 108;
  else if(desp == 23 && carril == CARRIL_IZQUIERDO)
    return 23;
  else if(desp == 25 && carril == CARRIL_IZQUIERDO)
    return 25+137;
  else if(desp == 99 && carril == CARRIL_IZQUIERDO)
    return 99+137;
  else if(desp == 101 && carril == CARRIL_IZQUIERDO)
    return 101;
  else
    return 0;
}

int comprobar_sig(int desp, int carril) {
    if(desp == 136 && !carril)
      return 0;
    else if(desp == 136 && carril)
      return 137;
    else if(!carril)
      return desp+1;
    else
      return desp+138;
}

int comprobar_ant(int desp, int carril) {
    if(desp == 0 && !carril)
      return 136;
    else if(desp == 0 && carril)
      return 137+136;
    else if(!carril)
      return desp-1;
    else if(carril)
      return desp+136;
}
