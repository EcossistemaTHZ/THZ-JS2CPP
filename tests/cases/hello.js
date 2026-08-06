// funções
function add(a, b) {
    return a + b;
}

function factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

// variável global
let base = 10;

// arrays
let arr = [1, 2, 3, 4, 5];
let sum = 0;
for (let i = 0; i < arr.length; i++) {
    sum = sum + arr[i];
}

// objetos
let person = { name: "Lucas", age: 30, greeting: "oi" };

// classe com herança
class Animal {
    constructor(nome) {
        this.nome = nome;
    }
    falar() {
        return "Animal " + this.nome;
    }
}

class Cachorro extends Animal {
    falar() {
        return "Au au, eu sou " + this.nome;
    }
}

// arrow function
let aoQuadrado = x => x * x;

// operadores lógicos e ternário
let flag = (sum > 10) ? true : false;

// programa principal
console.log("add(3,4) =", add(3, 4));
console.log("factorial(5) =", factorial(5));
console.log("sum =", sum);
console.log("person =", person);
console.log("aoQuadrado(7) =", aoQuadrado(7));
console.log("flag =", flag);
console.log("base + sum =", base + sum);

let dog = new Cachorro("Rex");
console.log(dog.falar());

let cat = new Animal("Miau");
console.log(cat.falar());