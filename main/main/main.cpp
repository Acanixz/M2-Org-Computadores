/*
* Disciplina: Organiza��o de Computadores
* Atividade : Avalia��o 02
*
* Grupo:
* - Cassiano de Sena Crispim
* - H�rick Vitor Vieira Bittencourt
* - Eduardo Miguel Fuchs Perez
*
* OBSERVA��O:
* - Devido o uso de for loops com l�gica [key, value], o padr�o ISO para
* este projeto deve ser o C++ 17, caso contr�rio a build ir� falhar
* (v� na aba projeto->propriedades de main no visual studio 2019)
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include "calculos.h"
using namespace std;


// Armazena informa��es geradas pelo binary dump
struct LinhaASM {
    // Instru��o completa (32 bits)
    string instrucao;

    // Substrings da instru��o completa
    string opcode;
    string rd;
    string funct3;
    string rs1;
    string rs2;
    string funct7;

    // Variaveis informativas
    string tipoInstrucao;
};

// Mostra na tela todas as instrucoes do programa
// (al�m de poder destacar NOPs se preferido)
void VisualizarInstrucoes(vector<LinhaASM> programa, bool destacarNOPs = false) {
    cout << "----------------------" << endl;
    cout << "VISUALIZANDO PROGRAMA:" << endl;

    for (int i = 0; i < programa.size(); i++) {
        cout << (i + 1) << ": " << programa[i].instrucao;
        if (destacarNOPs) {
            if (programa[i].tipoInstrucao != "S") {
                if (programa[i].rd == "00000") {
                    // OBS: jump-and-links e ecall podem acabar sendo considerados NOP
                    cout << " | NOP |";
                }
            }
        }
        cout << endl;
    }

    cout << "----------------------" << endl;
}

// Uma organiza��o, utilizado para gerar estatisticas com um vetor de LinhaASM
struct Organizacao {
    float TClock; // Tempo de clock
    float freqClock; // Frequencia de clock (1/TClock)
    map<string, float> quantCiclos; // Quantos ciclos leva cada instru��o?


    Organizacao() {
        quantCiclos["U"] = 1.f;
        quantCiclos["J"] = 1.f;
        quantCiclos["B"] = 1.f;
        quantCiclos["I_ar"] = 1.f; // Quant. Ciclos p/ instru��o Imm. Aritmetico e ecall
        quantCiclos["I_lo"] = 1.f; // Quant. Ciclos p/ instru��o Imm. Load
        quantCiclos["R"] = 1.f;
        quantCiclos["S"] = 1.f;
    }
};

// Resultados de desempenho da organiza��o com um binary dump
struct Resultados {
    float CiclosTotais; // Ciclos totais gastos
    float CPI; // Ciclos por instru��o
    float TExec; // Tempo de execu��o da CPU (Quant. instrucoes * CPI * Clock)
};

// Abre o arquivo, redundante, mas quem sabe
// ganha um proposito melhor depois
bool abrirArquivo(ifstream& saida, string caminho) {
    saida.open(caminho);
    if (saida.is_open()) {
        return true;
    }
    cout << "Nao foi possivel abrir o arquivo: " << caminho << endl;
    return false;
}

string lerOpcode(string opcode) {
    if (opcode == "0110111" || opcode == "0010111")
        return "U";

    if (opcode == "1101111")
        return "J";

    if (opcode == "1100011")
        return "B";

    if (opcode == "1100111" || opcode == "0010011" || opcode == "0001111" || opcode == "1110011")
        return "I_ar";

    if (opcode == "0000011")
        return "I_lo";

    if (opcode == "0110011")
        return "R";

    if (opcode == "0100011")
        return "S";

    cout << "(aviso) opcode desconhecido: " << opcode << endl;
    return "?";
}

bool verificarHazardInstrucao(LinhaASM instrucaoOrigem, LinhaASM instrucaoJ) {

    // A instru��o tipo S n�o faz nenhuma escrita, logo, as proximas linhas n�o ir�o ter problemas de dependencia
    if (instrucaoOrigem.tipoInstrucao == "S") return false;

    // NO-Operators, ignora
    if (instrucaoOrigem.instrucao == "00000000000000000000000000110011") return false;
    if (instrucaoJ.instrucao == "00000000000000000000000000110011") return false;

    if (instrucaoJ.tipoInstrucao == "R" || instrucaoJ.tipoInstrucao == "I_ar" || instrucaoJ.tipoInstrucao == "I_lo" || instrucaoJ.tipoInstrucao == "S" || instrucaoJ.tipoInstrucao == "B") {
        if (instrucaoOrigem.rd == instrucaoJ.rs1) {
            return true;
        }
    }

    if (instrucaoJ.tipoInstrucao == "R" || instrucaoJ.tipoInstrucao == "S" || instrucaoJ.tipoInstrucao == "B") {
        if (instrucaoOrigem.rd == instrucaoJ.rs2) {
            return true;
        }
    }
    return false;
}

vector<LinhaASM> inserirNOPs(vector<LinhaASM> instrucoes, vector<int> hazards) {
    // Certo, nos temos um array contendo as instru��es I (origens cujo possuem no minimo 1 hazard nas proximas 2 linhas)
    // Se formos de baixo pra cima, o array ser� mais f�cil de mexer
    // Ao verificar as instru��es, precisamos ir de cima pra baixo a partir de I, come�ando em I+1 at� ser maior que I+2
    // Se o hazard for na primeira linha, ser� necess�rio adicionar dois NOPs
    // Se o hazard for na segunda linha, ser� necess�rio adicionar um NOP

    LinhaASM noOperator; // add zero, zero, zero
    noOperator.instrucao = "00000000000000000000000000110011";

    noOperator.opcode = noOperator.instrucao.substr(25, 7);
    noOperator.rd = noOperator.instrucao.substr(20, 5);
    noOperator.funct3 = noOperator.instrucao.substr(17, 3);
    noOperator.rs1 = noOperator.instrucao.substr(12, 5);
    noOperator.rs2 = noOperator.instrucao.substr(7, 5);
    noOperator.funct7 = noOperator.instrucao.substr(0, 7);
    noOperator.tipoInstrucao = lerOpcode(noOperator.opcode);

    // TODO: fazer o for loop i ir do come�o ao fim
    for (int i = hazards.size() - 1; i >= 0; i--) {
        // Quantidade de NOPs a serem adicionados, max 2, min 0
        int quantNOPs = 2;

        // Como mais elementos s�o adicionados ao array, precisamos de um offset se manter nas posi��es corretas
        //int offset = 0;

        // TODO: possivel incrementar hazards relativo a quantNOPs para corre��o de erros

        for (int j = hazards[i] + 1; j <= hazards[i] + 2; j++) {
            if (j > hazards.size() - 1) continue;
            if (verificarHazardInstrucao(instrucoes[hazards[i]], instrucoes[j])) {
                for (int k = 0; k < quantNOPs; k++) {
                    cout << "Inserido NOP na linha " << (i+1) << " para corrigir a dep. da linha " << j << endl;
                    cout << "INFORMACOES: \nI: " << instrucoes[hazards[i]].instrucao << "\nJ: " << instrucoes[j].instrucao << endl << endl;
                    instrucoes.insert(instrucoes.begin() + i + 1, noOperator);
                }
            }
            quantNOPs--;
        }
    }
    cout << "NO OPERATORS INSERIDOS COM SUCESSO" << endl;
    return instrucoes;
}

// Verifica o vetor de instru��es assembly por
// hazards de pipeline
// regra base: rd atual n�o pode ser utilizado
// nos proximos 2 ciclos
vector<int> verificarHazards(vector<LinhaASM> instrucoes) {
    cout << "Executando verificacao de hazards" << endl;
    vector<int> falhas;
    for (int i = 0; i < instrucoes.size(); i++) {
        // Ignora instru��o se o registrador de destino for zero, NOP
        if (instrucoes[i].tipoInstrucao != "S") {
            if (instrucoes[i].rd == "00000") continue;
        }

        // For loop 2 passos a frente de i
        // verifica��o de dependencias
        for (int j = i+1; j <= i + 2; j++) {
            // Ignora itera��o j caso passe da quantidade de instru��es
            if (j >= instrucoes.size()) continue;
            //cout << "Verificando " << j + 1 << " de origem " << i + 1 << endl;
            //cout << "Tipos: \nJ: " + instrucoes[j].tipoInstrucao << "\nI: " << instrucoes[i].tipoInstrucao << "\nRD do I: " << instrucoes[i].rd << "\nRS1 e RS2 do J: " << instrucoes[j].rs1 << " | " << instrucoes[j].rs2 << endl;
            if (verificarHazardInstrucao(instrucoes[i], instrucoes[j])) {
                cout << "Hazard encontrada na linha " << j + 1 << " vindo da linha nao-finalizada " << i + 1 << endl;
                falhas.push_back(i);
            }
        }
    }
    cout << "Verificacao de hazards concluido com exito" << endl;
    return falhas;
}

// Recebe um ifstream para ler e gera um vetor
// cada indice do vetor representa uma instru��o de 32 bits
vector<LinhaASM> lerArquivo(ifstream& arquivo) {
    vector<LinhaASM> instrucoes;

    if (!arquivo.is_open()) {
        throw std::runtime_error("Abra o arquivo antes de tentar ler!");
    }

    int i = 0;
    while (arquivo.good()) {
        i++;
        LinhaASM linhaAtual;
        arquivo >> linhaAtual.instrucao;

        if (linhaAtual.instrucao.size() != 32) {
            if (arquivo.good()) {
                // alguma linha n�o est� certa
                throw std::runtime_error("Nao foi possivel ler a linha " + to_string(i) + ", verifique o arquivo.");
            }
            else {
                // newline final, s� ignorar
                continue;
            }
        }

        // obs: a ordem dos numeros s�o o inverso do site
        // https://jemu.oscc.cc/ADD
        // (site vai da direita pra esquerda, aqui � esq. pra dir.)
        linhaAtual.opcode = linhaAtual.instrucao.substr(25, 7);
        linhaAtual.rd = linhaAtual.instrucao.substr(20, 5);
        linhaAtual.funct3 = linhaAtual.instrucao.substr(17, 3);
        linhaAtual.rs1 = linhaAtual.instrucao.substr(12, 5);
        linhaAtual.rs2 = linhaAtual.instrucao.substr(7, 5);
        linhaAtual.funct7 = linhaAtual.instrucao.substr(0, 7);

        linhaAtual.tipoInstrucao = lerOpcode(linhaAtual.opcode);
        // se tudo deu certo, pode colocar no array
        instrucoes.push_back(linhaAtual);
    }


    return instrucoes;
}

// Retorna uma organizacao conforme
// entrada do usu�rio
Organizacao criarOrganizacao(string nome) {
    cout << "----------------------\n ORGANIZACAO " << nome << endl;
    Organizacao resultado;
    cout << "Forneca o tempo de clock da organizacao " << nome << ": ";
    cin >> resultado.TClock;
    resultado.freqClock = (1 / resultado.TClock);

    for (auto const& [key, value] : resultado.quantCiclos) {
        cout << "Forneca a quantidade de ciclos para instrucoes tipo " << key << ": ";
        cin >> resultado.quantCiclos[key];
    }
    cout << "----------------------" << endl << endl;
    return resultado;
}

Resultados calcularResultados(vector<LinhaASM> programa, Organizacao organizacao) {
    Resultados resultado{};

    std::map<std::string, float> somaCiclos = {
        {"U", 0.f},
        {"J", 0.f},
        {"B", 0.f},
        {"I_ar", 0.f},
        {"I_lo", 0.f},
        {"R", 0.f},
        {"S", 0.f},
        {"?", 0.f}
    };

    for (int i = 0; i < programa.size(); i++) {
        somaCiclos[programa[i].tipoInstrucao] += 1;
        resultado.CiclosTotais += organizacao.quantCiclos[programa[i].tipoInstrucao];
    }

    /*
    * Debug das buscas por cada tipo
    for (auto const& [key, value] : somaCiclos) {
        cout << key << ": " << value << endl;
    }
    */

    resultado.CPI = (resultado.CiclosTotais / programa.size());
    resultado.TExec = TExecCPUPorFreqClock(programa.size(), resultado.CPI, organizacao.freqClock);
    return resultado;
}

int main() {
    bool finalizado = false;
    bool semOrganizacao = true;
    Organizacao orgA;
    Organizacao orgB;
    while (!finalizado) {
        system("cls");

        // Cria as organiza��es se elas n�o existem ou usuario quer refazer
        if (semOrganizacao) {
            cout << "Criando organizacoes: " << endl;
            semOrganizacao = false;
            orgA = criarOrganizacao("A");
        }

        string nomeFornecido = "";
        cout << "Forneca o nome/caminho do arquivo contendo as instrucoes: " << endl;
        cin >> nomeFornecido;
        cout << endl << endl;

        ifstream programa;
        abrirArquivo(programa, nomeFornecido);
        vector<LinhaASM> instrucoes = lerArquivo(programa);
        VisualizarInstrucoes(instrucoes, true);
        vector<int> falhas = verificarHazards(instrucoes);
        instrucoes = inserirNOPs(instrucoes, falhas);
        VisualizarInstrucoes(instrucoes, true);
        verificarHazards(instrucoes);

        /*
        * Debug das informa��es gerais
        for (int i = 0; i < instrucoes.size(); i++) {
            cout << "INSTRUCAO " << i + 1 << " completa: " << instrucoes[i].instrucao << endl;
            cout << "TIPO DE INSTRUCAO: " << instrucoes[i].tipoInstrucao << endl;
            cout << "INSTRUCAO " << i + 1 << " separada: "
                << instrucoes[i].funct7 << " "
                << instrucoes[i].rs2 << " "
                << instrucoes[i].rs1 << " "
                << instrucoes[i].funct3 << " "
                << instrucoes[i].rd << " "
                << instrucoes[i].opcode
                << endl << endl;
        }
        */

        /*
        Resultados resultadoA = calcularResultados(instrucoes, orgA);
        cout << "RESULTADOS DA ORGANIZACAO A: " << endl;
        cout << "TOTAL DE CICLOS: " << resultadoA.CiclosTotais << endl;
        cout << "CPI (Ciclos por Instrucao): " << resultadoA.CPI << endl;
        cout << "Tempo de execucao: " << resultadoA.TExec << endl;

        cout << "----------------------" << endl;
        */
        

        int escolha = 0;
        cout << endl << "Escolha uma opcao:" << endl;
        cout << "1 - Abrir novo arquivo" << endl;
        cout << "2 - Recriar organizacoes e abrir novo arquivo" << endl;
        cout << "3 - Sair" << endl;
        while (escolha < 1 || escolha > 3) {
            cout << "Opcao escolhida: ";
            cin >> escolha;
            if (escolha < 1 || escolha > 3)
                cout << "Opcao invalida, tente novamente!" << endl;
        }
        cout << endl << endl;

        switch (escolha)
        {
        case 2:
            semOrganizacao = true;
            break;

        case 3:
            finalizado = true;
            break;
        }
    }
    return 0;
}