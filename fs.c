/*
 * RSFS - Really Simple File System
 *
 * Copyright © 2010 Gustavo Maciel Dias Vieira
 * Copyright © 2010 Rodrigo Rocco Barbieri
 *
 * This file is part of RSFS.
 *
 * RSFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define CLUSTERSIZE 4096

unsigned short fat[65536];


typedef struct {
       char used;
       char name[25];
       unsigned short first_block;
       int size;
} dir_entry;

typedef struct {
      char mode;
      char *conteudo;
      int id;
} opened_file;

char * buffer;

dir_entry dir[128];
opened_file file_list[128];

int id=0;

int fs_init() {
  int i;
  char *bufferFAT = (char *) fat;
 
  /*Carrega para a memória o sistema a FAT, começando do cluster 0 até o 32,
  multiplicando por 8 para obter cada setor contido */
  /*for(i=0;i<32;i++){
    if(!le_bloco((char *) fat, i)){
       printf("Falha ao carregar o sistema de fat!\n");
       return 0;
     }
  }*/
 
  
  for (i 0;i<(32*8);i++) {
    
    if(!bl_read(i, &bufferFAT[SECTORSIZE*i])){
      printf("Falha ao carregar o sistema de fat!\n");
      return 0;
     }        
  }

  for(i=0; i<(bl_size()/8); i++)
    printf("%d\n", bufferFAT[i]);
  //int i;
  //for(i=0;i<128;i++)printf("%d %d\n",i, fat[i]);
  /*Carrego o arquivo de diretórios, da mesma forma multiplico por 8 para obter o setor*/
  if(!le_bloco((char *) dir, 32)){
    printf("Falha ao carregar o sistema de diretórios!\n");
    return 0;
  }

  for(i=0; i<128; i++){
    file_list[i].id = -1;
  }
/*  for (setor=(32*8); setor < ((32*8)+8); setor++){
      if(!bl_read(setor , &bufferDir[SECTORSIZE * setor])){
        printf("Falha ao carregar o sistema de diretórios!\n");
        return 0;
      }
  }
*/
  checkdisk();
  
  return 1;
}

/*Essa função é responsável pela verificação da integridade do disco,
  bem como sua teblela de alocação e diretório raiz */
int checkdisk(){
  int i;
    for (i = 0; i < 32; i++){ //percorro os 32 (0~31) primeiros clusters do disco, se ele não é reservado para a FAT retorna erro.
      
      if(fat[i] != 3){
        printf("Aviso: O disco não está formatado.\n");
        return 0;
      }
  }
  if (fat[32] != 4){ //verifico se o arquivo de diretório está mapeado.
      printf("Aviso: Imagem com arquivo de diretório comprometido!\n");
      return 0;
  }
  return 1;
}

int fs_format() {
  int i;
  printf("Formatando o disco\n");
  for(i = 0; i < 32; i++)
    fat[i] = 3;

  for (i = 33; i < 65536; i++)
    fat[i] = 1;

  fat[32] = 4;

  for (i = 0; i < 128; i++) {dir[i].used = 0;}
  
  flush();
  return 1;
}

int fs_free() {
  int i, cont=0;
  for(i=33;i<(bl_size()/8);i++){
    if(fat[i]==1)cont++;
  }
  return cont*CLUSTERSIZE; //retorna o numero de entradas livres na fat * o tamanho do cluster
}

int fs_list(char *buffer, int size) {
  int i; 
  for(i=0; i<128; i++){
    if(dir[i].used == 1)
      buffer += sprintf(buffer, "%s\t\t%d\n", dir[i].name, dir[i].size);
  }
  return 1;
}

int fs_create(char* file_name) {
  int i, livre=-1, bloco_livre=-1;
  for(i=0; i<128; i++){
    if(file_name == dir[i].name){
      printf("O arquivo já existe!\n");
      return 0;
    }
   if(dir[i].used == 0 && livre == -1){
      livre = i;
   }
  }

  if(livre == -1){
    printf("Diretório cheio!\n");
    return 0;
  }

  for(i=0; i<(bl_size()/8); i++){
    if(fat[i] == 1 && bloco_livre == -1){
      bloco_livre = i;
    }
  }

  if(bloco_livre == -1){
    printf("Disco cheio!\n");
    return 0;
  }

  dir[livre].used = 1;
  dir[livre].first_block = bloco_livre;
  strcpy(dir[livre].name, file_name);
  dir[livre].size = 0;

  fat[bloco_livre] = 2;

  flush();
  return 0;
}

int fs_remove(char *file_name) {
  printf("Função não implementada: fs_remove\n");
  return 0;
}

int fs_open(char *file_name, int mode) {
  int i, primeiro;
  char arq_existe=0;
  for(i=0; i<128; i++)
    if(dir[i].name == file_name){
      primeiro=dir[i].first_block;
      arq_existe=1;
    }

  if(arq_existe == 1){
    id++;
    for(i=0; i<128; i++){
      if(file_list[i].id == -1){
        if(!le_bloco(file_list[i]->conteudo, primeiro)) return -1;
        file_list[i].id = id;
        if(mode == FS_R) file_list[i].mode = FS_R;
        if(mode == FS_W) file_list[i].mode = FS_W;
        return file_list[i].id;
      }
    }

  }


  return -1;
}

int fs_close(int file)  {
  printf("Função não implementada: fs_close\n");
  return 0;
}

int fs_write(char *buffer, int size, int file) {
  printf("Função não implementada: fs_write\n");
  return -1;
}

int fs_read(char *buffer, int size, int file) {
  printf("Função não implementada: fs_read\n");
  return -1;
}


/*Funções auxíliares*/

/*Função que se comunica com o disco escrevendo blocos*/
int escreve_bloco(char *bufferSetor, int setor){
  int s;
  /*Este for garante que serão escrito apenas sequências de 8 setores, ou seja, um bloco por vez*/
  for(s=0;s<8;s++)
    /*setor*8 garante que eu só ando setores de 8 em 8*/ 
    /*bufferSector limitado sempre a tamanho do setor, ou seja 512*/
    if(!bl_write((setor*8)+s, &bufferSetor[SECTORSIZE*s])) return 0;
  return 1;
}

int le_bloco(char *bufferSetor, int setor){
  int s;
  for(s=0;s<8;s++){
    if(!bl_read((setor*8)+s, &bufferSetor[SECTORSIZE*s])) return 0;
  }
  return 1;
}

int flush(){
  int i;
  buffer = (char *) fat;

  for(i=0; i<32; i++)
    if(!escreve_bloco(&buffer[CLUSTERSIZE*i], i)) return 0;

  buffer = (char *) dir;

  if(!escreve_bloco(buffer, 32)) return 0;
  return 1;
}

