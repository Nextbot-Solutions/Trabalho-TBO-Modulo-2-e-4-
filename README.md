# Busca e Ordenação – Projeto 1

Projeto da disciplina de Busca e Ordenação (IFNMG – Campus Montes Claros).
Professor: Tadeu Zubaran

Aluno: Diogo Henrique e Pedro Lucas

## Módulos implementados

- **Módulo 2 – Buscas de Intervalo Numérico:** consulta de filmes por duração e por ano (valor exato ou intervalo), e de cinemas pelo ano dos filmes exibidos.
- **Módulo 4 – Regra de Inconsistência de IDs:** se um cinema referencia um filme que não existe, ele é associado ao filme existente com o código maior mais próximo (o sucessor).

## Como compilar e executar

```bash
g++ -O2 -std=c++17 projeto.cpp -o projeto
./projeto filmesCrop.txt cinemas.txt
```

No Windows, se os acentos aparecerem errados, rode `chcp 65001` antes.

Os arquivos de dados (`filmesCrop.txt` e `cinemas.txt`) não estão no repositório. Coloque-os na mesma pasta do executável ou passe o caminho como argumento.

## Arquitetura geral

- Os arquivos são lidos inteiros para a memória (`fread`) e interpretados manualmente, o que é mais rápido que ler linha a linha.
- Cada filme e cada cinema é armazenado **uma única vez** em um vetor. Todos os índices guardam apenas **posições** nesses vetores, nunca cópias dos registros.
- Valores ausentes (`\N`) são tratados como -1 e ficam fora dos índices.
- Todo o trabalho pesado acontece na carga, para que as consultas sejam rápidas. Os tempos de carga e de cada consulta são exibidos.

## Módulo 2 – Árvores AVL

Ano e duração são indexados em **árvores AVL implementadas do zero**.

- Cada nó representa um **valor distinto** (ex: o ano 2018) e guarda a lista de posições dos filmes com esse valor. Assim a árvore fica pequena: a de ano tem 134 nós e altura 9, e a de duração tem 372 nós e altura 10.
- O balanceamento é feito pelas rotações simples e duplas (casos LL, RR, LR e RL).
- A busca por intervalo `[a, b]` é um percurso em ordem **podado**: só desce para um lado da árvore se ainda puder haver valores dentro do intervalo. Complexidade O(log m + k), onde m é o número de valores distintos e k o número de resultados. A busca sequencial seria O(n), com n = 584 mil filmes.
- O resultado já sai ordenado pelo valor buscado.
- **Cinemas por ano:** cinemas não têm ano próprio, então uma terceira AVL liga cada ano aos cinemas que exibem algum filme daquele ano. Um cinema pode aparecer em vários anos do intervalo, e as repetições são removidas com um vetor de marcação, em O(k).

## Módulo 4 – Tabela de sucessores

1. O sistema verifica se os filmes estão ordenados por ID. Se não estiverem, aplica um **merge sort** implementado do zero.
2. **Tabela de sucessores:** para cada ID possível entre o menor e o maior, a tabela guarda a posição do primeiro filme com ID maior ou igual. Ela é preenchida de trás para frente em um único passe, e a consulta do sucessor vira um acesso direto, em **O(1)**.
3. **Busca binária** (lower bound) também foi implementada manualmente, em O(log n), para comparação de desempenho.
4. Cada filme citado por um cinema é resolvido pela tabela durante a carga. O sistema registra se a referência precisou de correção.

## Decisões de projeto

- A tabela de sucessores ocupa cerca de 5 MB de memória, mas deixa a consulta cerca de 100 vezes mais rápida que a busca binária.
- IDs maiores que o último filme não têm sucessor e são marcados como tal.
- O cinema `cc00399` aparece duplicado no arquivo. Os dois registros foram mantidos e o sistema exibe um aviso na carga.
- As consultas de cinemas por ano usam os filmes **já corrigidos** pelo Módulo 4, integrando os dois módulos.

## Resultados

| Medida | Valor |
|---|---|
| Filmes carregados | 584.121 |
| Cinemas carregados | 400 |
| Carga total (arquivos + tabela + árvores) | ~300–500 ms |
| Referências de filmes nos cinemas | 2009 |
| Referências corrigidas pelo sucessor | 397 |
| Sucessor pela tabela O(1) | ~3 ns por consulta |
| Sucessor por busca binária | ~325 ns por consulta |
| Intervalo na árvore AVL | ~0,002 ms por consulta |
| Intervalo por busca sequencial | ~2,3 ms por consulta |

Os tempos variam de máquina para máquina. Use as opções de benchmark do menu (4 e 8) para medir no seu computador.

## Menu do programa

| Opção | Função |
|---|---|
| 1 | Filmes por duração (min) |
| 2 | Filmes por ano ou intervalo de anos |
| 3 | Cinemas por ano dos filmes exibidos |
| 4 | Benchmark árvore x busca sequencial |
| 5 | Consultar cinema (filmes resolvidos) |
| 6 | Consultar ID de filme (sucessor) |
| 7 | Relatório de correções |
| 8 | Benchmark O(1) x busca binária |
| 0 | Sair |
