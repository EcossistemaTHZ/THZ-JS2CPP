# THZ-JS2CPP

**THZ-JS2CPP** é um transpilador AOT (*Ahead-Of-Time*) de **JavaScript para C++20**. Ele lê arquivos de código fonte `.js`, executa análise léxica (*lexing*), sintática (*parsing*), gera uma Representação Intermediária (IR) e emite código C++20 nativo de alta performance contendo um runtime dinâmico embarcado.

---

## 🚀 Visão Geral

O código C++ gerado embute um runtime dinâmico eficiente baseado no tipo `thz::Value`, capaz de manipular tipos primitivos e estruturas de dados dinâmicas do JavaScript sem a necessidade de um interpretador externo ou máquina virtual Node.js/V8.

- **Nativo e Autônomo:** Gera um arquivo `.cpp` único e autossuficiente que pode ser compilado diretamente com `g++` ou `clang++`.
- **Gerenciamento Automático de Memória:** Runtime baseado em contagem de referências (`std::shared_ptr`) para objetos, arrays e funções.
- **Transpilação Estruturada:** Processamento moderno via AST e Representação Intermediária em 3 endereços.

---

## ✨ Recursos e Sintaxes Suportadas

| Categoria | Recursos Suportados |
| :--- | :--- |
| **Tipos Primitivos** | `Number` (double), `String`, `Boolean`, `null`, `undefined` |
| **Coleções & Estruturas** | Arrays (`[1, 2, 3]`), Objetos (`{ a: 1, b: 2 }`), Chaves computadas (`{[key]: val}`), Spread operator (`...`) |
| **Funções** | Declarações de função (`function`), Arrow functions (`=>`), Parâmetros padrão, Rest parameters (`...args`), Funções como valores de primeira classe |
| **Orientação a Objetos (ES6+)** | Declaração de `class`, Herança com `extends`, `constructor`, `super()`, Métodos estáticos e de instância, Getters e Setters, Campos de classe |
| **Controle de Fluxo** | `if` / `else`, `while`, `do...while`, `for`, `for...in`, `for...of`, `switch` / `case` / `default`, `break`, `continue` |
| **Tratamento de Exceções** | `try` / `catch` / `finally`, `throw` |
| **Operadores & Expressões** | Aritméticos, Relacionais, Lógicos (`&&`, `||`), Coalescência Nula (`??`), Operador Ternário (`? :`), Bitwise (`&`, `\|`, `^`, `<<`, `>>`), Template Literals (``` `hello ${name}` ```) |
| **I/O Embarcado** | `console.log(...)` com suporte a múltiplos argumentos |

---

## 🛠️ Arquitetura do Pipeline

O pipeline de transpilação é composto por 5 etapas principais:

```text
       Código Fonte (.js)
               │
               ▼
   ┌──────────────────────┐
   │      src/lexer.cpp   │  --> Stream de Tokens (include/thz/token.hpp)
   └───────────┬──────────┘
               ▼
   ┌──────────────────────┐
   │     src/parser.cpp   │  --> Árvore de Sintaxe Abstrata / AST (include/thz/ast.hpp)
   └───────────┬──────────┘
               ▼
   ┌──────────────────────┐
   │  src/irbuilder.cpp   │  --> Representação Intermediária em 3 Endereços / IR (include/thz/ir.hpp)
   └───────────┬──────────┘
               ▼
   ┌──────────────────────┐
   │    src/codegen.cpp   │  --> Código C++20 com Runtime Embarcado (include/thz/codegen.hpp)
   └───────────┬──────────┘
               ▼
        Arquivo (.cpp)
               │
               ▼  (g++ -std=c++20 -O2)
       Binário Executável
```

### Módulos do Sistema

1. **Lexer** (`include/thz/lexer.hpp`, `src/lexer.cpp`): Varre o código fonte em JavaScript e converte o texto em um fluxo de tokens reconhecidos pelo transpilador.
2. **Parser** (`include/thz/parser.hpp`, `src/parser.cpp`): Processa os tokens e constrói a AST (Árvore de Sintaxe Abstrata) fortemente tipada.
3. **IRBuilder** (`include/thz/irbuilder.hpp`, `src/irbuilder.cpp`): Converte a AST em instruções de Representação Intermediária (IR), simplificando estruturas complexas em instruções de 3 endereços.
4. **Codegen** (`include/thz/codegen.hpp`, `src/codegen.cpp`): Emite o código C++20 final, incluindo as declarações do runtime `thz::Value` e a função `main()`.

---

## 📋 Requisitos do Sistema

- **Compilador C++:** Suporte a C++20 (testado com `g++ 10+` e `clang++ 11+`).
- **Sistema de Build:** `make` ou `cmake` (v3.16+).
- **Sistema Operacional:** Linux / macOS / Windows (via MinGW ou WSL).

---

## 📦 Como Compilar o Transpilador

### Opção 1: Via Makefile (Recomendado)

```bash
# Compila o binário do transpilador (gera ./thzc)
make

# Transpila, compila e executa todos os casos de teste
make test

# Limpa os objetos e o executável
make clean
```

### Opção 2: Via CMake

```bash
# Configura o diretório de build
cmake -B build

# Compila o transpilador
cmake --build build
```

---

## ⚡ Guia de Uso

### 1. Transpilar arquivo JavaScript para C++

Após compilar o `thzc`, execute o transpilador passando o arquivo `.js` de entrada:

```bash
./thzc arquivo.js -o saida.cpp
```

*Se o parâmetro `-o` for omitido, o padrão será `out.cpp`.*

### 2. Compilar o C++ gerado

Utilize um compilador C++ com suporte a C++20:

```bash
g++ -std=c++20 -O2 -o meu_programa saida.cpp
```

### 3. Executar o binário nativo

```bash
./meu_programa
```

---

## 📝 Exemplo Prático (Quick Start)

Considere o arquivo `exemplo.js`:

```javascript
function fatorial(n) {
    if (n <= 1) return 1;
    return n * fatorial(n - 1);
}

class Animal {
    constructor(nome) {
        this.nome = nome;
    }
    falar() {
        return "Sou o animal " + this.nome;
    }
}

class Cachorro extends Animal {
    falar() {
        return "Au au! Eu sou " + this.nome;
    }
}

let dog = new Cachorro("Rex");
console.log(dog.falar());
console.log("Fatorial de 5:", fatorial(5));
```

### Transpilando e Executando:

```bash
./thzc exemplo.js -o exemplo.cpp
g++ -std=c++20 -O2 -o exemplo exemplo.cpp
./exemplo
```

**Saída no Terminal:**
```text
Au au! Eu sou Rex
Fatorial de 5: 120
```

---

## 📂 Estrutura de Diretórios

```text
THZ-JS2CPP/
├── include/
│   └── thz/
│       ├── ast.hpp         # Definições dos nós de AST (Expr, Stmt, ClassDef, etc.)
│       ├── codegen.hpp     # Interface do gerador de código C++
│       ├── ir.hpp          # Tipos e instruções da Representação Intermediária (IR)
│       ├── irbuilder.hpp   # Conversor de AST para IR
│       ├── lexer.hpp       # Interface do Analisador Léxico
│       ├── parser.hpp      # Interface do Analisador Sintático
│       └── token.hpp       # Tipos de Tokens e palavras-chave
├── src/
│   ├── codegen.cpp         # Gerador de código C++20 e runtime dinâmico thz::Value
│   ├── irbuilder.cpp       # Implementação do construtor de IR
│   ├── lexer.cpp           # Implementação do Lexer
│   └── parser.cpp          # Implementação do Parser
├── tests/
│   └── cases/
│       └── hello.js        # Casos de teste integrados em JavaScript
├── main.cpp                # Ponto de entrada do CLI (thzc)
├── CMakeLists.txt          # Configuração de build via CMake
├── Makefile                # Configuração de build via Make
└── README.md               # Documentação do projeto
```

---

## 🧪 Testes Automatizados

Os testes integrados ficam localizados em `tests/cases/`.

Para rodar a suíte completa de testes que transpila o JS, compila o C++ gerado e executa o binário verificando o comportamento final:

```bash
make test
```

---

## ⚠️ Limitações Conhecidas

- **Operador Deslocamento Sem Sinal (`>>>`):** Atualmente é tratado equivalentemente ao deslocamento com sinal (`>>`).
- **Operador de Exponenciação (`**`):** Ainda em fase de implementação na AST e IR.
- **Conversões Implícitas de Tipos:** Operações bitwise e lógicas muito complexas envolvendo misturas heterogêneas no runtime `thz::Value` podem requerer ajustes explícitos de coerção de tipo.

---

## 📄 Licença e Convenções

- **Padrão de Código:** C++20 (`-std=c++20 -Wall -Wextra -O2`).
- **Convenções:** Namespaces `thz::`, headers `#pragma once`, `camelCase` para métodos/funções e `snake_case_` para membros privados.