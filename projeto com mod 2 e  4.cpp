// =====================================================================
//  Busca e Ordenação - Projeto 1
//  Módulos implementados: 2 (Intervalo Numérico) e 4 (IDs inconsistentes)
//
//  ---------------- MÓDULO 2: Buscas de Intervalo Numérico -------------
//  Ano e duração são indexados em ÁRVORES AVL (implementadas do zero).
//  Cada nó guarda um VALOR distinto (ex: ano 2018) e a lista de posições
//  dos filmes com esse valor. Como há poucos valores distintos
//  (~140 anos, ~370 durações), as árvores são pequenas e baixas.
//  Busca por intervalo [a, b]: desce só pelos ramos que podem conter
//  valores no intervalo -> O(log m + k), m = valores distintos,
//  k = resultados. A busca sequencial seria O(n), n = 584 mil filmes.
//  Cinemas por ano: uma terceira AVL liga cada ano aos cinemas que
//  exibem algum filme daquele ano.
//
//  ---------------- MÓDULO 4: Regra de Inconsistência de IDs -----------
//
//  Se um cinema referencia um código de filme que NÃO existe, ele é
//  associado ao filme existente com o código MAIOR MAIS PRÓXIMO
//  (o "sucessor").
//
//  Estratégia:
//   1. Carrega os filmes num vector (armazenamento único) e garante que
//      estão ordenados por ID (verifica; se não estiverem, merge sort próprio).
//   2. Pré-processamento: tabela de sucessores. Para todo ID possível entre
//      idMin e idMax, guarda a posição do primeiro filme com ID >= ele.
//      -> consulta em O(1) (um acesso a vetor).
//   3. Busca binária (lower bound) implementada à mão, O(log n), usada
//      para comparação de desempenho.
//   4. Carrega os cinemas resolvendo cada referência pela tabela.
// =====================================================================
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <iostream>
using namespace std;
using Clock = chrono::high_resolution_clock;

// ------------------------------ Estruturas ---------------------------
struct Filme {
    uint32_t id;        // "tt7917518" -> 7917518
    string   tipo;
    string   titulo;
    string   generos;
    int      ano;       // -1 quando "\N"
    int      duracao;   // -1 quando "\N"
};

struct RefFilme {
    uint32_t idOriginal; // ID como estava no arquivo de cinemas
    int      pos;        // posição no vector<Filme> (-1 = sem sucessor)
    bool     corrigido;  // true se o ID original não existia
};

struct Cinema {
    string id, nome;
    int    x = 0, y = 0;
    double preco = 0;
    vector<RefFilme> filmes;
};

// Armazenamento único: índices guardam apenas POSIÇÕES nesses vetores
vector<Filme>  filmes;
vector<Cinema> cinemas;

// Tabela de sucessores: sucessor[id - idMin] = posição do 1º filme com ID >= id
vector<int> sucessor;
uint32_t idMin = 0, idMax = 0;

// ------------------------------ Utilidades ---------------------------
static double ms(Clock::time_point a, Clock::time_point b) {
    return chrono::duration<double, milli>(b - a).count();
}

// Lê o arquivo inteiro para memória de uma vez (bem mais rápido que getline)
bool lerArquivo(const char* caminho, vector<char>& buf) {
    FILE* f = fopen(caminho, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf.resize(n + 1);
    size_t lidos = fread(buf.data(), 1, n, f);
    fclose(f);
    buf.resize(lidos + 1);
    buf[lidos] = '\0';
    return true;
}

// Extrai um campo terminado por 'sep' ou fim de linha.
// Retorna o início, preenche 'fim' e avança 'p' para depois do separador.
static const char* campo(const char*& p, char sep, const char*& fim, bool pularEspacos = false) {
    if (pularEspacos) while (*p == ' ') p++;
    const char* ini = p;
    while (*p && *p != sep && *p != '\n' && *p != '\r') p++;
    fim = p;
    if (*p == sep) p++;
    return ini;
}

// Converte os dígitos do intervalo [a, b) em inteiro. "\N" ou vazio -> -1.
// Ignora não-dígitos, então "tt7917518" vira 7917518.
static long long paraInt(const char* a, const char* b) {
    long long v = 0;
    bool temDigito = false;
    for (; a < b; a++)
        if (*a >= '0' && *a <= '9') { v = v * 10 + (*a - '0'); temDigito = true; }
    return temDigito ? v : -1;
}

static void pularLinha(const char*& p) {
    while (*p && *p != '\n') p++;
    if (*p == '\n') p++;
}

// ------------------------------ Ordenação ----------------------------
// Merge sort (estável) implementado do zero, usado só se o arquivo
// vier fora de ordem.
void mergeSort(vector<Filme>& v, vector<Filme>& aux, size_t ini, size_t fim) {
    if (fim - ini < 2) return;
    size_t meio = ini + (fim - ini) / 2;
    mergeSort(v, aux, ini, meio);
    mergeSort(v, aux, meio, fim);
    size_t i = ini, j = meio, k = ini;
    while (i < meio && j < fim)
        aux[k++] = move(v[j].id < v[i].id ? v[j++] : v[i++]); // "<" estrito = estável
    while (i < meio) aux[k++] = move(v[i++]);
    while (j < fim)  aux[k++] = move(v[j++]);
    for (k = ini; k < fim; k++) v[k] = move(aux[k]);
}

bool estaOrdenado() {
    for (size_t i = 1; i < filmes.size(); i++)
        if (filmes[i - 1].id > filmes[i].id) return false;
    return true;
}

// ------------------------------ Carga dos filmes ---------------------
bool carregarFilmes(const char* caminho) {
    vector<char> buf;
    if (!lerArquivo(caminho, buf)) return false;

    const char* p = buf.data();
    pularLinha(p); // cabeçalho
    filmes.reserve(600000);

    while (*p) {
        if (*p == '\n' || *p == '\r') { p++; continue; } // linhas vazias
        const char *a, *b;
        Filme f;
        a = campo(p, '\t', b); f.id = (uint32_t)paraInt(a, b);  // tconst
        a = campo(p, '\t', b); f.tipo.assign(a, b);             // titleType
        a = campo(p, '\t', b); f.titulo.assign(a, b);           // primaryTitle
        campo(p, '\t', b);                                      // originalTitle
        campo(p, '\t', b);                                      // isAdult
        a = campo(p, '\t', b); f.ano = (int)paraInt(a, b);      // startYear
        campo(p, '\t', b);                                      // endYear
        a = campo(p, '\t', b); f.duracao = (int)paraInt(a, b);  // runtimeMinutes
        a = campo(p, '\t', b); f.generos.assign(a, b);          // genres
        pularLinha(p);
        if (f.id > 0) filmes.push_back(move(f));
    }
    return !filmes.empty();
}

// ------------------------------ Módulo 4 -----------------------------
// Pré-processamento: preenche a tabela de trás pra frente.
// Custo: O(intervalo de IDs + n) na carga. Memória: ~1,28 mi ints (~5 MB).
void construirTabelaSucessores() {
    idMin = filmes.front().id;
    idMax = filmes.back().id;
    sucessor.assign((size_t)(idMax - idMin) + 1, -1);

    int pos = (int)filmes.size() - 1; // filmes[pos].id == idMax
    for (long long id = idMax; id >= (long long)idMin; id--) {
        // recua enquanto o filme anterior ainda for >= id
        while (pos > 0 && filmes[pos - 1].id >= id) pos--;
        sucessor[id - idMin] = pos;
    }
}

// O(1): um acesso direto à tabela
int sucessorO1(uint32_t id) {
    if (id < idMin) return 0;         // qualquer filme serve: o menor é o sucessor
    if (id > idMax) return -1;        // não existe código maior
    return sucessor[id - idMin];
}

// O(log n): busca binária (lower bound) feita à mão, para comparação
int sucessorBinaria(uint32_t id) {
    int lo = 0, hi = (int)filmes.size();
    while (lo < hi) {
        int meio = lo + (hi - lo) / 2;
        if (filmes[meio].id < id) lo = meio + 1;
        else hi = meio;
    }
    return lo < (int)filmes.size() ? lo : -1;
}

// ------------------------------ Carga dos cinemas --------------------
bool carregarCinemas(const char* caminho) {
    vector<char> buf;
    if (!lerArquivo(caminho, buf)) return false;

    const char* p = buf.data();
    pularLinha(p); // cabeçalho

    while (*p) {
        if (*p == '\n' || *p == '\r') { p++; continue; }
        const char *a, *b;
        Cinema c;
        a = campo(p, ',', b, true); c.id.assign(a, b);
        a = campo(p, ',', b, true); c.nome.assign(a, b);
        a = campo(p, ',', b, true); c.x = (int)paraInt(a, b);
        a = campo(p, ',', b, true); c.y = (int)paraInt(a, b);
        a = campo(p, ',', b, true); c.preco = strtod(string(a, b).c_str(), nullptr);

        // Filmes em exibição: quantidade variável até o fim da linha
        while (*p && *p != '\n' && *p != '\r') {
            a = campo(p, ',', b, true);
            long long id = paraInt(a, b);
            if (id < 0) continue;

            RefFilme r;
            r.idOriginal = (uint32_t)id;
            r.pos        = sucessorO1(r.idOriginal);           // <- regra do Módulo 4
            r.corrigido  = (r.pos == -1 || filmes[r.pos].id != r.idOriginal);
            c.filmes.push_back(r);
        }
        pularLinha(p);
        cinemas.push_back(move(c));
    }
    return !cinemas.empty();
}

// Os IDs de cinema vêm com duplicata (cc00399). Decisão: manter ambos e avisar.
void avisarCinemasDuplicados() {
    for (size_t i = 0; i < cinemas.size(); i++)
        for (size_t j = i + 1; j < cinemas.size(); j++)
            if (cinemas[i].id == cinemas[j].id)
                printf("  [aviso] ID de cinema duplicado: %s (\"%s\" e \"%s\") - ambos mantidos\n",
                       cinemas[i].id.c_str(), cinemas[i].nome.c_str(), cinemas[j].nome.c_str());
}

// ------------------------------ Módulo 2: Árvore AVL -----------------
struct NoAVL {
    int chave;                 // valor indexado (ano ou duração)
    int altura = 1;
    NoAVL* esq = nullptr;
    NoAVL* dir = nullptr;
    vector<int> itens;         // posições (no vector) dos registros com essa chave
    explicit NoAVL(int c) : chave(c) {}
};

class ArvoreAVL {
    NoAVL* raiz = nullptr;
    int qtdNos = 0;

    static int alt(NoAVL* n) { return n ? n->altura : 0; }
    static void atualizar(NoAVL* n) {
        int ae = alt(n->esq), ad = alt(n->dir);
        n->altura = 1 + (ae > ad ? ae : ad);
    }
    static NoAVL* rotacaoDireita(NoAVL* y) {
        NoAVL* x = y->esq;
        y->esq = x->dir;
        x->dir = y;
        atualizar(y);
        atualizar(x);
        return x;
    }
    static NoAVL* rotacaoEsquerda(NoAVL* x) {
        NoAVL* y = x->dir;
        x->dir = y->esq;
        y->esq = x;
        atualizar(x);
        atualizar(y);
        return y;
    }

    NoAVL* inserir(NoAVL* n, int chave, int item) {
        if (!n) {
            qtdNos++;
            NoAVL* novo = new NoAVL(chave);
            novo->itens.push_back(item);
            return novo;
        }
        if (chave < n->chave)      n->esq = inserir(n->esq, chave, item);
        else if (chave > n->chave) n->dir = inserir(n->dir, chave, item);
        else { n->itens.push_back(item); return n; } // chave já existe: só agrupa

        atualizar(n);
        int fb = alt(n->esq) - alt(n->dir);           // fator de balanceamento
        if (fb > 1) {                                  // pesado à esquerda
            if (chave > n->esq->chave) n->esq = rotacaoEsquerda(n->esq); // caso LR
            return rotacaoDireita(n);                                    // caso LL
        }
        if (fb < -1) {                                 // pesado à direita
            if (chave < n->dir->chave) n->dir = rotacaoDireita(n->dir);  // caso RL
            return rotacaoEsquerda(n);                                   // caso RR
        }
        return n;
    }

    // Percurso em ordem PODADO: só desce para um lado se ainda puder haver
    // chaves dentro de [lo, hi]. Resultado sai ordenado pela chave.
    void intervalo(NoAVL* n, int lo, int hi, vector<int>& out) const {
        if (!n) return;
        if (lo < n->chave) intervalo(n->esq, lo, hi, out);
        if (lo <= n->chave && n->chave <= hi)
            for (int item : n->itens) out.push_back(item);
        if (n->chave < hi) intervalo(n->dir, lo, hi, out);
    }

    // Só conta, sem copiar: soma o tamanho das listas dos nós no intervalo
    size_t contar(NoAVL* n, int lo, int hi) const {
        if (!n) return 0;
        size_t total = 0;
        if (lo < n->chave) total += contar(n->esq, lo, hi);
        if (lo <= n->chave && n->chave <= hi) total += n->itens.size();
        if (n->chave < hi) total += contar(n->dir, lo, hi);
        return total;
    }

    void liberar(NoAVL* n) {
        if (!n) return;
        liberar(n->esq);
        liberar(n->dir);
        delete n;
    }

public:
    ArvoreAVL() = default;
    ArvoreAVL(const ArvoreAVL&) = delete;            // evita cópia acidental
    ArvoreAVL& operator=(const ArvoreAVL&) = delete;
    ~ArvoreAVL() { liberar(raiz); }

    void inserir(int chave, int item) { raiz = inserir(raiz, chave, item); }
    void buscarIntervalo(int lo, int hi, vector<int>& out) const { intervalo(raiz, lo, hi, out); }
    size_t contarIntervalo(int lo, int hi) const { return contar(raiz, lo, hi); }
    int altura() const { return alt(raiz); }
    int nos() const { return qtdNos; }
};

ArvoreAVL arvoreAno;        // ano -> filmes
ArvoreAVL arvoreDuracao;    // duração (min) -> filmes
ArvoreAVL arvoreCinemaAno;  // ano -> cinemas que exibem filme daquele ano

void construirIndicesModulo2() {
    // Filmes: posição final já definida (após verificação/ordenação)
    for (int i = 0; i < (int)filmes.size(); i++) {
        if (filmes[i].ano > 0)     arvoreAno.inserir(filmes[i].ano, i);
        if (filmes[i].duracao > 0) arvoreDuracao.inserir(filmes[i].duracao, i);
    }
    // Cinemas: cada cinema entra uma vez por ano distinto dos seus filmes
    for (int c = 0; c < (int)cinemas.size(); c++) {
        vector<int> anosJaInseridos; // no máximo ~7 filmes por cinema
        for (const RefFilme& r : cinemas[c].filmes) {
            if (r.pos == -1) continue;
            int ano = filmes[r.pos].ano;
            if (ano <= 0) continue;
            bool repetido = false;
            for (int a : anosJaInseridos) if (a == ano) { repetido = true; break; }
            if (repetido) continue;
            anosJaInseridos.push_back(ano);
            arvoreCinemaAno.inserir(ano, c);
        }
    }
}

// ------------------------------ Exibição -----------------------------
void imprimirFilme(int pos) {
    const Filme& f = filmes[pos];
    printf("tt%07u | %-12s | %s", f.id, f.tipo.c_str(), f.titulo.c_str());
    if (f.ano > 0) printf(" (%d)", f.ano);
    printf("\n");
}

void imprimirRef(const RefFilme& r) {
    printf("    tt%07u -> ", r.idOriginal);
    if (r.pos == -1) { printf("SEM SUCESSOR (maior que o ultimo ID)\n"); return; }
    if (r.corrigido) printf("[corrigido] ");
    imprimirFilme(r.pos);
}

// ------------------------------ Consultas Módulo 2 -------------------
static int lerInt(const char* msg) {
    int v;
    printf("%s", msg);
    while (!(cin >> v)) { cin.clear(); cin.ignore(10000, '\n'); printf("Valor invalido. %s", msg); }
    return v;
}

static void lerIntervalo(const char* nome, int& lo, int& hi) {
    printf("(para valor exato, digite o mesmo numero nos dois)\n");
    string m1 = string(nome) + " inicial: ", m2 = string(nome) + " final: ";
    lo = lerInt(m1.c_str());
    hi = lerInt(m2.c_str());
    if (lo > hi) { int t = lo; lo = hi; hi = t; }
}

static const int LIMITE_EXIBICAO = 20;

void mostrarFilmes(const vector<int>& res, double tempo) {
    printf("\n%zu filme(s) encontrado(s) em %.4f ms\n", res.size(), tempo);
    for (size_t i = 0; i < res.size() && i < (size_t)LIMITE_EXIBICAO; i++) {
        const Filme& f = filmes[res[i]];
        printf("  tt%07u | %4s | %4s min | %-12s | %s\n", f.id,
               f.ano > 0 ? to_string(f.ano).c_str() : "-",
               f.duracao > 0 ? to_string(f.duracao).c_str() : "-",
               f.tipo.c_str(), f.titulo.c_str());
    }
    if (res.size() > (size_t)LIMITE_EXIBICAO)
        printf("  ... e mais %zu\n", res.size() - LIMITE_EXIBICAO);
}

void consultarPorDuracao() {
    int lo, hi;
    lerIntervalo("Duracao (min)", lo, hi);
    vector<int> res;
    auto t0 = Clock::now();
    arvoreDuracao.buscarIntervalo(lo, hi, res);
    auto t1 = Clock::now();
    mostrarFilmes(res, ms(t0, t1));
}

void consultarFilmesPorAno() {
    int lo, hi;
    lerIntervalo("Ano", lo, hi);
    vector<int> res;
    auto t0 = Clock::now();
    arvoreAno.buscarIntervalo(lo, hi, res);
    auto t1 = Clock::now();
    mostrarFilmes(res, ms(t0, t1));
}

void consultarCinemasPorAno() {
    int lo, hi;
    lerIntervalo("Ano", lo, hi);
    vector<int> bruto, res;
    auto t0 = Clock::now();
    arvoreCinemaAno.buscarIntervalo(lo, hi, bruto);
    // Um cinema pode aparecer em vários anos do intervalo: remove repetidos
    // com um vetor de marcação (O(k), sem ordenar)
    vector<char> visto(cinemas.size(), 0);
    for (int c : bruto)
        if (!visto[c]) { visto[c] = 1; res.push_back(c); }
    auto t1 = Clock::now();

    printf("\n%zu cinema(s) exibem filmes de %d a %d (%.4f ms)\n", res.size(), lo, hi, ms(t0, t1));
    for (size_t i = 0; i < res.size() && i < (size_t)LIMITE_EXIBICAO; i++) {
        const Cinema& c = cinemas[res[i]];
        printf("  %s - %s | R$ %.2f\n", c.id.c_str(), c.nome.c_str(), c.preco);
        for (const RefFilme& r : c.filmes)
            if (r.pos != -1 && filmes[r.pos].ano >= lo && filmes[r.pos].ano <= hi)
                printf("      tt%07u (%d) %s\n", filmes[r.pos].id, filmes[r.pos].ano, filmes[r.pos].titulo.c_str());
    }
    if (res.size() > (size_t)LIMITE_EXIBICAO)
        printf("  ... e mais %zu\n", res.size() - LIMITE_EXIBICAO);
}

// Compara a árvore com a busca sequencial (varrer os 584 mil filmes)
void benchmarkModulo2() {
    const int N = 200;
    mt19937 gen(7);
    uniform_int_distribution<int> anoIni(1950, 2020), larguraAno(0, 5);
    uniform_int_distribution<int> durIni(1, 180), larguraDur(0, 30);

    volatile size_t soma = 0;
    double tArvore = 0, tSeq = 0;
    size_t divergencias = 0;

    for (int q = 0; q < N; q++) {
        bool porAno = (q % 2 == 0);
        int lo = porAno ? anoIni(gen) : durIni(gen);
        int hi = lo + (porAno ? larguraAno(gen) : larguraDur(gen));

        auto t0 = Clock::now();
        size_t cArv = porAno ? arvoreAno.contarIntervalo(lo, hi)
                             : arvoreDuracao.contarIntervalo(lo, hi);
        auto t1 = Clock::now();
        size_t cSeq = 0;
        for (const Filme& f : filmes) {
            int v = porAno ? f.ano : f.duracao;
            if (v >= lo && v <= hi) cSeq++;
        }
        auto t2 = Clock::now();

        tArvore += ms(t0, t1);
        tSeq    += ms(t1, t2);
        if (cArv != cSeq) divergencias++;
        soma = soma + cArv + cSeq;
    }

    printf("\n%d consultas de intervalo (contagem):\n", N);
    printf("  Arvore AVL:       %9.3f ms no total (%.4f ms/consulta)\n", tArvore, tArvore / N);
    printf("  Busca sequencial: %9.3f ms no total (%.4f ms/consulta)\n", tSeq, tSeq / N);
    printf("  Divergencias: %zu\n", divergencias);
    printf("  Arvore de ano:     %d nos, altura %d\n", arvoreAno.nos(), arvoreAno.altura());
    printf("  Arvore de duracao: %d nos, altura %d\n", arvoreDuracao.nos(), arvoreDuracao.altura());
}

// ------------------------------ Menu ---------------------------------
void consultarCinema() {
    string id;
    printf("ID do cinema (ex: cc00001): ");
    cin >> id;
    auto t0 = Clock::now();
    bool achou = false;
    for (const Cinema& c : cinemas) {
        if (c.id != id) continue;
        achou = true;
        printf("\n%s - %s | pos (%d, %d) | R$ %.2f\n",
               c.id.c_str(), c.nome.c_str(), c.x, c.y, c.preco);
        for (const RefFilme& r : c.filmes) imprimirRef(r);
    }
    auto t1 = Clock::now();
    if (!achou) printf("Cinema nao encontrado.\n");
    printf("Tempo da consulta: %.4f ms\n", ms(t0, t1));
}

void consultarIdFilme() {
    string entrada;
    printf("ID do filme (ex: tt8000001): ");
    cin >> entrada;
    long long id = paraInt(entrada.data(), entrada.data() + entrada.size());
    if (id < 0) { printf("ID invalido.\n"); return; }

    auto t0 = Clock::now();
    int pos = sucessorO1((uint32_t)id);
    auto t1 = Clock::now();

    if (pos == -1) printf("Nao existe filme com codigo >= tt%07lld.\n", id);
    else {
        printf(filmes[pos].id == id ? "Filme existe: " : "Nao existe. Sucessor: ");
        imprimirFilme(pos);
    }
    printf("Tempo da consulta (O(1)): %.6f ms\n", ms(t0, t1));
}

void relatorioCorrecoes() {
    size_t total = 0, corrigidas = 0, semSucessor = 0;
    for (const Cinema& c : cinemas)
        for (const RefFilme& r : c.filmes) {
            total++;
            if (r.pos == -1) semSucessor++;
            else if (r.corrigido) corrigidas++;
        }
    printf("\nReferencias a filmes nos cinemas: %zu\n", total);
    printf("  corretas:              %zu\n", total - corrigidas - semSucessor);
    printf("  corrigidas (sucessor): %zu\n", corrigidas);
    printf("  sem sucessor:          %zu\n", semSucessor);

    printf("\nPrimeiras 10 correcoes:\n");
    int mostradas = 0;
    for (const Cinema& c : cinemas)
        for (const RefFilme& r : c.filmes)
            if (r.corrigido && mostradas < 10) {
                printf("  %s:", c.id.c_str());
                imprimirRef(r);
                mostradas++;
            }
}

void benchmark() {
    const int N = 1000000;
    mt19937 gen(42);
    uniform_int_distribution<uint32_t> dist(idMin, idMax);
    vector<uint32_t> consultas(N);
    for (auto& q : consultas) q = dist(gen);

    volatile long long soma = 0; // impede o compilador de eliminar o laço

    auto t0 = Clock::now();
    for (uint32_t q : consultas) soma = soma + sucessorO1(q);
    auto t1 = Clock::now();
    for (uint32_t q : consultas) soma = soma + sucessorBinaria(q);
    auto t2 = Clock::now();

    // Confere se as duas abordagens concordam
    size_t divergencias = 0;
    for (int i = 0; i < 10000; i++)
        if (sucessorO1(consultas[i]) != sucessorBinaria(consultas[i])) divergencias++;

    printf("\n%d consultas aleatorias:\n", N);
    printf("  Tabela de sucessores O(1): %8.2f ms (%.1f ns/consulta)\n", ms(t0, t1), ms(t0, t1) * 1e6 / N);
    printf("  Busca binaria O(log n):    %8.2f ms (%.1f ns/consulta)\n", ms(t1, t2), ms(t1, t2) * 1e6 / N);
    printf("  Divergencias (amostra 10k): %zu\n", divergencias);
    printf("  Memoria da tabela: %.2f MB\n", sucessor.size() * sizeof(int) / 1048576.0);
}

int main(int argc, char** argv) {
    const char* arqFilmes  = argc > 1 ? argv[1] : "filmesCrop.txt";
    const char* arqCinemas = argc > 2 ? argv[2] : "cinemas.txt";

    printf("=== Carregamento ===\n");
    auto t0 = Clock::now();
    if (!carregarFilmes(arqFilmes)) { printf("Erro ao ler %s\n", arqFilmes); return 1; }
    auto t1 = Clock::now();
    printf("Filmes carregados: %zu (%.1f ms)\n", filmes.size(), ms(t0, t1));

    if (!estaOrdenado()) {
        printf("Filmes fora de ordem, ordenando com merge sort...\n");
        vector<Filme> aux(filmes.size());
        mergeSort(filmes, aux, 0, filmes.size());
    }
    auto t2 = Clock::now();

    construirTabelaSucessores();
    auto t3 = Clock::now();
    printf("Verificacao/ordenacao: %.1f ms\n", ms(t1, t2));
    printf("Tabela de sucessores (tt%07u a tt%07u): %.1f ms\n", idMin, idMax, ms(t2, t3));

    if (!carregarCinemas(arqCinemas)) { printf("Erro ao ler %s\n", arqCinemas); return 1; }
    auto t4 = Clock::now();
    printf("Cinemas carregados: %zu (%.1f ms)\n", cinemas.size(), ms(t3, t4));
    avisarCinemasDuplicados();

    construirIndicesModulo2();
    auto t5 = Clock::now();
    printf("Arvores AVL (ano, duracao, cinema-ano): %.1f ms\n", ms(t4, t5));
    printf("Tempo total de carga: %.1f ms\n", ms(t0, t5));

    int op = -1;
    while (op != 0) {
        printf("\n=== Modulo 2: Intervalo Numerico ===\n"
               "1 - Filmes por duracao (min)\n"
               "2 - Filmes por ano (ou intervalo de anos)\n"
               "3 - Cinemas por ano dos filmes exibidos\n"
               "4 - Benchmark arvore x busca sequencial\n"
               "=== Modulo 4: IDs Inconsistentes ===\n"
               "5 - Consultar cinema (filmes resolvidos)\n"
               "6 - Consultar ID de filme (sucessor)\n"
               "7 - Relatorio de correcoes\n"
               "8 - Benchmark O(1) x busca binaria\n"
               "0 - Sair\n> ");
        if (!(cin >> op)) break;
        switch (op) {
            case 1: consultarPorDuracao();    break;
            case 2: consultarFilmesPorAno();  break;
            case 3: consultarCinemasPorAno(); break;
            case 4: benchmarkModulo2();       break;
            case 5: consultarCinema();        break;
            case 6: consultarIdFilme();       break;
            case 7: relatorioCorrecoes();     break;
            case 8: benchmark();              break;
        }
    }
    return 0;
}
