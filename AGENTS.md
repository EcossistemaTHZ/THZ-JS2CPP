# AGENTS.md

Guia para agentes de IA (e desenvolvedores) trabalharem neste repositório.

## Visão geral do projeto

**THZ-JS2CPP** é um transpilador de **JavaScript para C++**. Ele lê um arquivo
`.js`, faz *lexing*, *parsing*, converte para uma representação intermediária
(IR) e gera código C++ que pode ser compilado e executado nativamente.

O pipeline é:

```
fonte .js
   │  src/lexer.cpp      (tokens)
   ▼
src/parser.cpp           (AST — include/thz/ast.hpp)
   │
   ▼
src/irbuilder.cpp        (IR — include/thz/ir.hpp)
   │
   ▼
src/codegen.cpp          (gera C++ com um runtime embarcado)
   │
   ▼
arquivo .cpp → g++ → binário executável
```

O C++ gerado embute um pequeno runtime dinâmico (`thz::Value`) capaz de
representar números, strings, booleanos, arrays, objetos e funções, além das
operações típicas de JS (`+`, `&&`, `??`, acesso a membros, `console.log`, etc.).
A maior parte do código gerado está em `src/codegen.cpp` (função `generate()`).

## Estrutura de diretórios

- `main.cpp` — entrada do CLI (`thzc [arquivo.js] [-o saida.cpp]`).
- `include/thz/*.hpp` — cabeçalhos públicos (AST, IR, token, etc.).
- `src/*.cpp` — implementações do pipeline.
- `tests/cases/*.js` — casos de teste (arquivos JS de entrada).
- `Makefile` — build via `g++` (make all / test / clean).
- `CMakeLists.txt` — build alternativo via CMake.

## Como compilar e executar

### Makefile

```bash
make            # compila o transpilador (gera ./thzc)
make test       # roda o transcilder em todos os casos em tests/cases/*
make clean      # remove objetos e o binário
```

### CMake

```bash
cmake -B build && cmake --build build
```

### Uso manual

```bash
./thzc tests/cases/hello.js -o /tmp/out.cpp   # transpila
g++ -std=c++20 -O2 -o /tmp/out /tmp/out.cpp   # compila o C++ gerado
/tmp/out                                       # executa
```

## Convenções de código

- **Padrão**: C++20, compilado com `-std=c++20 -Wall -Wextra -O2`.
- Compiladores suportados/testados: `g++` (Makefile) e provedor usado pelo CMake.
- **Cabeçalhos**: formato `#pragma once`, namespace `thz::`.
- **AST**: `Expr`/`Stmt` são tipos polimórficos com campo `kind` (enum).
- **Ponteiros**: uso de `std::unique_ptr` para nós de AST (`ExprPtr`, `StmtPtr`)
  e `std::shared_ptr` no runtime gerado (`ArrayPtr`, `ObjectPtr`, `FuncPtr`).
- **Mensagens de erro** do parser em PT-BR (ex.: `error("esperado identificador")`).
- **Nomes**: `camelCase` para funções/métodos e `snake_case_` para membros
  privados (ex.: `noIn_`, `toks_`, `pos_`).
- **Não adicionar comentários desnecessários**; manter o estilo do código ao redor.
- Seguir as convenções já existentes ao editar um arquivo (libs, tipos, padrões).

## Testes

- Casos de entrada vivem em `tests/cases/hello.js`.
- O objetivo do `make test` é compilar e **executar** o binário gerado, não
  apenas transpilar (verifique/ajuste se o alvo reproduz isso).
- Para validar mudanças no pipeline, transpile `tests/cases/hello.js`,
  compile o `.cpp` resultante com `g++` e rode o binário.

## Observações / limitações conhecidas

- `>>>` (deslocamento sem sinal) é tratado como `>>`.
- `**` (exponenciação) ainda não é suportado.
- Algumas conversões no C++ gerado podem gerar erros de ambiguidade; cuidado ao
  usar operadores bitwise/negação com o `thz::Value`.