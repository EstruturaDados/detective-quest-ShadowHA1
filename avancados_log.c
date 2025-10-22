/*
 Detective Quest - Módulo de pistas, suspeitos e julgamento final
 Implementação em C - nível mestre
 - Mansão: árvore binária de salas (mapa fixo, montado em main)
 - Pistas coletadas: armazenadas em BST (ordenadas por string)
 - Tabela hash: associa pista -> suspeito (cadeias encadeadas)
 - Exploração interativa: ir para esquerda (e), direita (d) ou sair (s)
 - Ao final: listar pistas coletadas, jogador acusa um suspeito e sistema verifica
   se pelo menos duas pistas apontam para o acusado.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define HASH_SIZE 103  // número primo para a tabela hash
#define MAX_INPUT 128

/* ---------------------------
   Tipos de dados (structs)
   --------------------------- */

/* Nó da árvore de salas (mapa da mansão) */
typedef struct Sala {
    char *nome;            // identificador único da sala
    char *pista;           // pista associada (pode ser NULL)
    struct Sala *esq;      // sala à esquerda
    struct Sala *dir;      // sala à direita
} Sala;

/* Nó da BST que armazena pistas coletadas (sem duplicatas) */
typedef struct NoPista {
    char *pista;
    struct NoPista *esq;
    struct NoPista *dir;
} NoPista;

/* Entrada da tabela hash (cadeia encadeada) */
typedef struct HashEntry {
    char *pista;           // chave
    char *suspeito;        // valor
    struct HashEntry *next;
} HashEntry;

/* Tabela hash (vetor de ponteiros para HashEntry) */
typedef struct {
    HashEntry *buckets[HASH_SIZE];
} HashTable;

/* ---------------------------
   Protótipos das funções
   --------------------------- */

/* criarSala - cria dinamicamente uma sala com nome e pista
   Retorna ponteiro para Sala alocada */
Sala *criarSala(const char *nome, const char *pista);

/* explorarSalas - navega recursivamente pela árvore de salas,
   exibindo pista da sala atual (se existir) e permitindo ao jogador
   escolher para onde ir. Colhe pistas e adiciona no BST e considera
   a tabela hash já carregada. */
void explorarSalas(Sala *raiz, NoPista **colecao, HashTable *ht);

/* inserirPista / adicionarPista - insere uma pista na BST de pistas coletadas.
   Evita duplicatas; pista será armazenada com strdup. */
NoPista *inserirPista(NoPista *raiz, const char *pista);

/* inserirNaHash - insere (ou substitui) uma associação pista -> suspeito na tabela hash */
void inserirNaHash(HashTable *ht, const char *pista, const char *suspeito);

/* encontrarSuspeito - procura na tabela hash o suspeito associado a uma pista.
   Retorna ponteiro para string (não duplicar), ou NULL se não encontrada. */
const char *encontrarSuspeito(HashTable *ht, const char *pista);

/* verificarSuspeitoFinal - percorre a BST de pistas coletadas e conta quantas
   pistas apontam para o suspeito acusado. Retorna número de pistas encontradas. */
int verificarSuspeitoFinal(NoPista *raiz, HashTable *ht, const char *acusado);

/* funções utilitárias: busca em BST, impressão em-ordem, liberar memória, hash */
void listarPistas(NoPista *raiz);
void liberarPistas(NoPista *raiz);
unsigned long hash_str(const char *s);
void inicializarHash(HashTable *ht);
void liberarHash(HashTable *ht);

/* ---------------------------
   Implementação
   --------------------------- */

/* criarSala() – cria dinamicamente um cômodo.
   Comentário: aloca e inicializa uma Sala com nome e pista.
*/
Sala *criarSala(const char *nome, const char *pista) {
    Sala *s = (Sala *) malloc(sizeof(Sala));
    if (!s) { perror("malloc sala"); exit(EXIT_FAILURE); }
    s->nome = strdup(nome ? nome : "");
    s->pista = pista ? strdup(pista) : NULL;
    s->esq = s->dir = NULL;
    return s;
}

/* inserirPista() / adicionarPista() – insere a pista coletada na árvore de pistas.
   Comentário: faz inserção ordenada por strcmp; não insere duplicatas. */
NoPista *inserirPista(NoPista *raiz, const char *pista) {
    if (!pista) return raiz;
    if (!raiz) {
        NoPista *n = (NoPista *) malloc(sizeof(NoPista));
        if (!n) { perror("malloc no pista"); exit(EXIT_FAILURE); }
        n->pista = strdup(pista);
        n->esq = n->dir = NULL;
        return n;
    }
    int cmp = strcmp(pista, raiz->pista);
    if (cmp == 0) {
        // duplicata: não insere
        return raiz;
    } else if (cmp < 0) {
        raiz->esq = inserirPista(raiz->esq, pista);
    } else {
        raiz->dir = inserirPista(raiz->dir, pista);
    }
    return raiz;
}

/* inserirNaHash() – insere associação pista/suspeito na tabela hash.
   Comentário: usa encadeamento; se a chave já existir, substitui o suspeito. */
void inserirNaHash(HashTable *ht, const char *pista, const char *suspeito) {
    if (!pista || !suspeito) return;
    unsigned long h = hash_str(pista) % HASH_SIZE;
    HashEntry *e = ht->buckets[h];
    while (e) {
        if (strcmp(e->pista, pista) == 0) {
            // substitui suspeito
            free(e->suspeito);
            e->suspeito = strdup(suspeito);
            return;
        }
        e = e->next;
    }
    // nova entrada
    HashEntry *ne = (HashEntry *) malloc(sizeof(HashEntry));
    if (!ne) { perror("malloc hash entry"); exit(EXIT_FAILURE); }
    ne->pista = strdup(pista);
    ne->suspeito = strdup(suspeito);
    ne->next = ht->buckets[h];
    ht->buckets[h] = ne;
}

/* encontrarSuspeito() – consulta o suspeito correspondente a uma pista.
   Comentário: busca na tabela hash; retorna ponteiro para string existente. */
const char *encontrarSuspeito(HashTable *ht, const char *pista) {
    if (!pista) return NULL;
    unsigned long h = hash_str(pista) % HASH_SIZE;
    HashEntry *e = ht->buckets[h];
    while (e) {
        if (strcmp(e->pista, pista) == 0) return e->suspeito;
        e = e->next;
    }
    return NULL;
}

/* explorarSalas() – navega pela árvore e ativa o sistema de pistas.
   Comentário: visita a sala atual, mostra a pista (se houver), pergunta ao jogador
   o próximo passo (e=esquerda, d=direita, s=sair). Coleta pistas na BST. */
void explorarSalas(Sala *raiz, NoPista **colecao, HashTable *ht) {
    if (!raiz) return;

    Sala *atual = raiz;
    char escolha[MAX_INPUT];

    while (1) {
        printf("\nVocê está na sala: %s\n", atual->nome);
        if (atual->pista) {
            printf("Você encontrou uma pista: \"%s\"\n", atual->pista);
            // adiciona na coleção (BST) - evita duplicatas
            *colecao = inserirPista(*colecao, atual->pista);
        } else {
            printf("Nenhuma pista nesta sala.\n");
        }

        printf("Para onde ir? (e = esquerda, d = direita, s = sair desta sala)\n");
        printf("Escolha: ");
        if (!fgets(escolha, sizeof(escolha), stdin)) { clearerr(stdin); strcpy(escolha, "s\n"); }
        // pega primeiro caractere não-branco
        char c = '\0';
        for (int i = 0; escolha[i]; i++) {
            if (!isspace((unsigned char)escolha[i])) { c = tolower((unsigned char)escolha[i]); break; }
        }

        if (c == 'e') {
            if (atual->esq) {
                atual = atual->esq;
            } else {
                printf("Não há sala à esquerda. Escolha outra ação.\n");
            }
        } else if (c == 'd') {
            if (atual->dir) {
                atual = atual->dir;
            } else {
                printf("Não há sala à direita. Escolha outra ação.\n");
            }
        } else if (c == 's') {
            printf("Saindo da exploração (voltando ao menu principal)...\n");
            break;
        } else {
            printf("Opção inválida. Use 'e', 'd' ou 's'.\n");
        }
    }
}

/* verificarSuspeitoFinal() – conduz à fase de julgamento final.
   Comentário: percorre a BST de pistas coletadas e conta quantas pistas
   apontam para o suspeito acusado usando a tabela hash. */
int verificarSuspeitoFinal(NoPista *raiz, HashTable *ht, const char *acusado) {
    if (!raiz || !acusado) return 0;
    int count = 0;
    // travessia in-order recursiva
    if (raiz->esq) count += verificarSuspeitoFinal(raiz->esq, ht, acusado);
    const char *s = encontrarSuspeito(ht, raiz->pista);
    if (s && strcmp(s, acusado) == 0) count++;
    if (raiz->dir) count += verificarSuspeitoFinal(raiz->dir, ht, acusado);
    return count;
}

/* listarPistas - imprime as pistas coletadas em ordem (in-order) */
void listarPistas(NoPista *raiz) {
    if (!raiz) return;
    listarPistas(raiz->esq);
    printf(" - %s\n", raiz->pista);
    listarPistas(raiz->dir);
}

/* liberarPistas - libera memória da BST de pistas */
void liberarPistas(NoPista *raiz) {
    if (!raiz) return;
    liberarPistas(raiz->esq);
    liberarPistas(raiz->dir);
    free(raiz->pista);
    free(raiz);
}

/* hash_str - função djb2 para strings */
unsigned long hash_str(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++))
        h = ((h << 5) + h) + c; /* h * 33 + c */
    return h;
}

/* inicializarHash - zera buckets */
void inicializarHash(HashTable *ht) {
    for (int i = 0; i < HASH_SIZE; i++) ht->buckets[i] = NULL;
}

/* liberarHash - desaloca todas entradas da tabela */
void liberarHash(HashTable *ht) {
    for (int i = 0; i < HASH_SIZE; i++) {
        HashEntry *e = ht->buckets[i];
        while (e) {
            HashEntry *nx = e->next;
            free(e->pista);
            free(e->suspeito);
            free(e);
            e = nx;
        }
        ht->buckets[i] = NULL;
    }
}

/* ---------------------------
   main - monta mansão, hash, fluxo de jogo
   --------------------------- */
int main(void) {
    /* Montagem do mapa (mansão) - fixo, manual.
       Estrutura exemplo (names):
                 Hall
                /    \
             Biblioteca  SalaJantar
             /     \       /    \
          Cozinha  Estudio  Jardim  Quarto
    */

    Sala *hall = criarSala("Hall", "Pegadas no carpete");
    Sala *biblioteca = criarSala("Biblioteca", "Página arrancada do diário");
    Sala *salaJantar = criarSala("Sala de Jantar", NULL);
    Sala *cozinha = criarSala("Cozinha", "Colher suja com restinho");
    Sala *estudio = criarSala("Estúdio", "Fio de cabelo loiro");
    Sala *jardim = criarSala("Jardim", "Pegada de botas grandes");
    Sala *quarto = criarSala("Quarto", "Bilhete com assinatura");

    // ligações
    hall->esq = biblioteca;
    hall->dir = salaJantar;
    biblioteca->esq = cozinha;
    biblioteca->dir = estudio;
    salaJantar->esq = jardim;
    salaJantar->dir = quarto;

    // Tabela hash: associar pistas -> suspeitos
    HashTable ht;
    inicializarHash(&ht);
    inserirNaHash(&ht, "Pegadas no carpete", "Sr. Black");
    inserirNaHash(&ht, "Página arrancada do diário", "Sra. Green");
    inserirNaHash(&ht, "Colher suja com restinho", "Cozinheiro");
    inserirNaHash(&ht, "Fio de cabelo loiro", "Sra. Green");
    inserirNaHash(&ht, "Pegada de botas grandes", "Sr. Black");
    inserirNaHash(&ht, "Bilhete com assinatura", "Baronesa");

    // coleção de pistas (BST vazia)
    NoPista *colecao = NULL;

    printf("=== Detective Quest: Exploração da Mansão ===\n");
    printf("Instruções: nas salas, você pode digitar 'e' para esquerda, 'd' para direita, 's' para sair.\n");
    printf("Explore quantas salas quiser. Quando sair, será a hora do julgamento.\n");

    // Exploração interativa a partir do Hall
    explorarSalas(hall, &colecao, &ht);

    // Fase final: listar pistas coletadas
    printf("\n=== Pistas coletadas ===\n");
    if (!colecao) {
        printf("Você não coletou nenhuma pista.\n");
    } else {
        listarPistas(colecao);
    }

    // Acusação: jogador indica suspeito
    char acusado[MAX_INPUT];
    printf("\nQuem você acusa? Digite o nome do suspeito (ex: 'Sr. Black'): ");
    if (!fgets(acusado, sizeof(acusado), stdin)) { strcpy(acusado, ""); }
    // remover newline
    size_t L = strlen(acusado);
    if (L && acusado[L-1] == '\n') acusado[L-1] = '\0';

    // verifica se pelo menos 2 pistas apontam para o acusado
    int correspondencias = verificarSuspeitoFinal(colecao, &ht, acusado);
    printf("\nAvaliação final: o sistema encontrou %d pista(s) que apontam para '%s'.\n",
           correspondencias, acusado);

    if (correspondencias >= 2) {
        printf("Desfecho: Há evidências suficientes. Sua acusação está SUSTENTADA. Parabéns, detetive!\n");
    } else {
        printf("Desfecho: Pistas insuficientes. Sua acusação NÃO está sustentada.\n");
    }

    // libera memória
    liberarPistas(colecao);
    liberarHash(&ht);

    // libera salas e strings
    Sala *all[] = {hall, biblioteca, salaJantar, cozinha, estudio, jardim, quarto};
    for (size_t i = 0; i < sizeof(all)/sizeof(all[0]); i++) {
        free(all[i]->nome);
        if (all[i]->pista) free(all[i]->pista);
        free(all[i]);
    }

    printf("\nFim do jogo. Obrigado por jogar!\n");
    return 0;
}
