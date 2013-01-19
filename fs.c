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

/*Estrutura utilizada na lista de arquivos abertos, contém modo, conteudo e id*/
typedef struct {
      char mode;
      char *conteudo;
      int id;
} opened_file;

char * buffer;

dir_entry dir[128];
/*Lista de arquivos abertos*/
opened_file file_list[128];

/*Variavel incremental de id de arquivo aberto*/
int id=0;

int fs_init() {
  int i;
  char *bufferFAT = (char *) fat;
 

  /*for(i=0;i<32;i++){
    if(!le_bloco((char *) fat, i)){
       printf("Falha ao carregar o sistema de fat!\n");
       return 0;
     }
  }*/
 
  /*Carrega para a memória o sistema a FAT, começando do cluster 0 até o 32,
  multiplicando por 8 para obter cada setor contido */
  for (i=0;i<(32*8);i++) { //versão arthur, queria mudar pra minha =/
    
    if(!bl_read(i, &bufferFAT[SECTORSIZE*i])){
      printf("Falha ao carregar o sistema de fat!\n");
      return 0;
     }        
  }

  /*Carrego o arquivo de diretórios utilizando */
  if(!le_bloco((char *) dir, 32)){
    printf("Falha ao carregar o sistema de diretórios!\n");
    return 0;
  }
  /*Todos os ids vão pra -1, se ele é negativo, não é um arquivo aberto válido*/
  for(i=0; i<128; i++){
    file_list[i].id = -1;
  }

  checkdisk();
  
  return 1;
}

/*Essa função é responsável pela verificação da integridade do disco,
  bem como sua teblela de alocação e diretório raiz */
int checkdisk(){
  int i;
  /*percorro os 32 (0~31) primeiros clusters do disco,
   se ele não é consistente com a FAT retorna erro.*/
    for (i = 0; i < 32; i++){ 
      if(fat[i] != 3){
        printf("Aviso: O disco não está formatado.\n");
        return 0;
      }
  }
  /*verificação pra ver se diretório está mapeado em disco*/
  if (fat[32] != 4){ 
      printf("Aviso: Imagem com arquivo de diretório comprometido!\n");
      return 0;
  }
  return 1;
}

int fs_format(){
  int i;
  printf("Formatando o disco\n");
  /*Inicializo as posições de 0 a 31 com 3, reservando espaço para FAT*/
  for(i = 0; i < 32; i++)
    fat[i] = 3;

  /*Inicializa a posição 32 com 4 para o diretório*/
  fat[32] = 4;

  /*Inicializa o restante com 1, indicando espaço livre*/
  for (i = 33; i < 65536; i++)
    fat[i] = 1;

  /*Deixa todos os elementos do diretório como não usados*/
  for (i = 0; i < 128; i++) dir[i].used = 0;
  
  /*Chama função flush que escreve fat e dir atuais no disco*/
  flush();
  return 1;
}

int fs_free() {
  int i, cont=0;
  /*Conta os blocos livres do disco*/
  for(i=33;i<(bl_size()/8);i++){
    if(fat[i]==1)cont++;
  }
  /*Retorna o número de entradas livres multiplicado pelo tamanho de um bloco*/
  return cont*CLUSTERSIZE; 
}

int fs_list(char *buffer, int size) {
  int i; 
  /*Escreve no buffer a lista de arquivos que estão marcados com used==1 no diretório*/
  for(i=0; i<128; i++){
    if(dir[i].used == 1)
      buffer += sprintf(buffer, "%s\t\t%d\n", dir[i].name, dir[i].size);
  }
  return 1;
}

int fs_create(char* file_name) {
  /*Uso de livre para indicar primeira entrada livre no diretório
  Uso de bloco_livre para indicar primeiro bloco_livre no disco*/
  int i, livre=-1, bloco_livre=-1;

  /*Verificação de file_name que não pode ser maior que 24 caracteres*/
  if(sizeof(file_name)>24){
    printf("Nome maior que 24 caracteres.");
    return 0;
  }

  /*Verifica se não há arquivo de mesmo nome no diretório e sai,
  caso contrário também verifica a primeira entrada livre do diretório */
  for(i=0; i<128; i++){ 
    if(!strcmp(file_name, dir[i].name){
      printf("O arquivo já existe!\n");
      return 0;
    }
   if(dir[i].used == 0 && livre == -1){
      livre = i;
   }
  }

  /*Se livre é -1, então o diretório está cheio*/
  if(livre == -1){
    printf("Diretório cheio!\n");
    return 0;
  }

  /*Procura na lista da FAT o primeiro bloco_livre (que está com valor 1)*/
  for(i=0; i<(bl_size()/8); i++){
    if(fat[i] == 1 && bloco_livre == -1){
      bloco_livre = i;
    }
  }

  /*Se bloco_livre é -1, então o disco está cheio*/
  if(bloco_livre == -1){
    printf("Disco cheio!\n");
    return 0;
  }

  /*Criação da entrada no diretório de fato*/
  dir[livre].used = 1;
  /*Primeiro bloco é primeiro bloco_livre*/
  dir[livre].first_block = bloco_livre;
  strcpy(dir[livre].name, file_name);
  /*Tamanho é 0 como na especificação*/
  dir[livre].size = 0;

  /*Bloco não está mais livre, então é 2 e o bloco final, 
  se o arquivo tem tamanho 0, então o arquivo não passará de um bloco*/
  fat[bloco_livre] = 2;

  /*Escreve as modificações no disco*/
  flush();
  return 1;
}


int fs_remove(char *file_name) {
  int i, tamanho;
  /*Mesmo caso de create, preciso de algo que indique o primeiro bloco ocupado do arquivo*/
  int bloco_primeiro=-1;

  /*Se nome de arquivo existe no diretório, então
  bloco_primeiro recebe a posição do começo do arquivo na FAT*/
  for(i=0; i<128; i++){
    if(!strcmp(dir[i].name,file_name)){
      bloco_primeiro = dir[i].first_block;
      /*O espaço no diretório é "liberado"*/
      dir[i].used = 0;
      tamanho = dir[i].size;
      /*Apaga nome do arquivo*/
      strcpy(dir[i].name, "");
    }
  }

  /*Se bloco_primeiro é -1, então o arquivo não existe*/
  if(bloco_primeiro == -1){
      printf("Esse arquivo não existe.");   
      return 0;
 }

  /*Solução para remover arquivos com um bloco ou mais,
  sempre seto bloco atual com 1 e pego o próximo até eu chegar no final do arquivo*/
  int proxbloco = bloco_primeiro;
  while(proxbloco != 2){
    int aux = fat[proxbloco];
    fat[proxbloco] = 1;
    proxbloco = aux;
  }

  /*Atualizo no disco*/
  flush();
  return 1;
}

int fs_open(char *file_name, int mode) {
  /*variavel primeiro contem o primeiro bloco do arquivo contido no diretorio*/
  int i, primeiro;
  /*flag que é 1 se arq existe ou 0 caso contrário*/
  char arq_existe=0;
  /*laço que procura no diretório o arquivo pelo seu nome*/
  for(i=0; i<128; i++)
    if(!strcmp(dir[i].name, file_name)){
      primeiro=dir[i].first_block;
      arq_existe=1;
    }
  /*id, global começa com 0, vai incrementando conforme abrimos arquivos*/
  id++;
  /*se existe o arquivo no diretorio*/
  if(arq_existe == 1){
    /*procura primeira posição livre na lista de arquivos abertos*/
    for(i=0; i<128; i++){
      if(file_list[i].id == -1){
        /*le bloco do arquivo e guarda em seu conteudo na lista*/
        if(!le_bloco(file_list[i].conteudo, primeiro)) return -1;
        /*atribui um id pro arquivo*/
        file_list[i].id = id;
        /*dependendo do modo, guarda isso no arquivo*/
        if(mode == FS_R) file_list[i].mode = FS_R;
        if(mode == FS_W) file_list[i].mode = FS_W;
        /*retorna id do arquivo aberto*/
        return file_list[i].id;
      }
    }
    /*caso contrário, se modo é de leitura, arquivo não existe*/
  }else{
    if(mode == FS_R){
      printf("O arquivo não existe");
      return -1;
    }

    /*se é modo de escrita, então temos que escrever um novo arquivo com tamanho 0*/
    for(i=0; i<128; i++){
      if(file_list[i].id == -1){
        file_list[i].id = id;
        if(!fs_create(file_name)){
          return 0;
        }
        file_list[i].mode = FS_W;
        return file_list[i].id;
      }
    }
  }


  return -1;
}

int fs_close(int file)  {
  int i;
  /*Procuro na lista identificador do arquivo e removo ele da lista*/
  for(i=0; i<128; i++){
    if(file_list[i].id == file){
      file_list[i].conteudo = null;
      file_list[i].mode = null;
      file_list[i].id = -1;
      return 1;
    }
  }
  printf("Não há arquivo com este identificador...");
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