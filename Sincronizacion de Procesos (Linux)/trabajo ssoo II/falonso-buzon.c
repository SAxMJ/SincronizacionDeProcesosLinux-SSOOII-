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
int comprobar_cruze(int carril, int desp);
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
  int tmp;

  struct tipo_mensaje {
	long pid_destino;
	long pid_origen;
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
  semctl(semaforo, 1, SETVAL, sem);
  sem.val = 0;
  semctl(semaforo, 2, SETVAL, sem);

  zonamemoria=shmget(IPC_PRIVATE, 800, IPC_CREAT | 0600);
  pzona = (char *) shmat(zonamemoria, 0, 0);
  pzona[350] = 1;
  pzona[351] = 0;
  for(i = 399; i < 800; i++)
    pzona[i] = -1;

  inicio_falonso(1, semaforo, pzona);
  luz_semAforo(HORIZONTAL, VERDE);
  luz_semAforo(VERTICAL, VERDE);

  buzon=msgget(IPC_PRIVATE, IPC_CREAT | 0600);

  pidpadre = getpid();
    
  for(i = 0; i < 15; i++) {
    if(getpid() == pidpadre){
      desp += 2;
      switch(fork()) {
        case 0 :  srand(100*getpid());
                  color = colores[2];
                  //carril = i % 2;
                  if(atoi(argv[2]))
                    vel = 99;
                  else
                    vel = 100;
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
    sops[0].sem_op=-15;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
    sops[0].sem_num=2;
    sops[0].sem_op=15;
    sops[0].sem_flg=0;
    semop(semaforo, sops, 1);
    sigsuspend(&conjunto_sin_SIGINT);
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
    sig = comprobar_sig(desp, carril);
    desp_antes = desp;
    desp_cambio = comprobar_cambio_carril(carril, desp);
    
    cambio = 0;

    if(pzona[sig] != ' ' && 
       pzona[desp_cambio] == ' ' &&
       pzona[comprobar_sig(desp_cambio, carril)] == ' ') {
      if(!carril)
        m.pid_destino = pzona[desp_cambio+537];
      else
        m.pid_destino = pzona[desp_cambio+400];
        
      if(m.pid_destino != -1)
        msgrcv(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), m.pid_destino, 0);
      else
        msgrcv(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), m.pid_destino, IPC_NOWAIT);
        
      if(!carril)
        pzona[comprobar_ant(desp_cambio, carril)+400] = id + 25;
      else
        pzona[comprobar_ant(desp_cambio, carril)+400] = id + 25;
      m.pid_destino = id;
      cambio_carril(&carril, &desp, color);
      msgsnd(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), 0);
      cambio = 1;      
    } else if(pzona[sig] != ' ') {
      if(!carril)
        m.pid_destino = pzona[desp+400];
      else
        m.pid_destino = pzona[desp+537];
      msgrcv(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), m.pid_destino, 0);
    } else {
      if(!carril)
        m.pid_destino = pzona[desp+400];
      else
        m.pid_destino = pzona[desp+537];
      msgrcv(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), m.pid_destino, IPC_NOWAIT);
    }
    
    if(comprobar_cruze(carril, desp) != 0) {
      if(pzona[comprobar_cruze(carril, desp)] != ' '){
        if(!carril)
          m.pid_destino = pzona[comprobar_cruze(carril, desp)+400];
        else
          m.pid_destino = pzona[comprobar_cruze(carril, desp)+400];
        msgrcv(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), m.pid_destino, 0);
        
        msgsnd(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), 0);
      }
    }
    tmp = pzona[desp+400];

    if(!carril)
      pzona[desp+400] = id;
    else
      pzona[desp+537] = id;
      
    if(!cambio)
      m.pid_destino = id;
    else
      m.pid_destino = id + 25;
      
    if(avance_coche(&carril, &desp, color) == -1){
      fprintf(stderr, "(%d) %d pos: %d\n", id, tmp, desp);
    }
    msgsnd(buzon, &m, sizeof(struct tipo_mensaje)-sizeof(long), 0);

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

int comprobar_sig(int desp, int carril) {
    if(desp == 136 && !carril)
      return 0;
    else if(desp == 136 && carril)
      return 137;
    else if(!carril)
      return desp+1;
    else if(carril)
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
