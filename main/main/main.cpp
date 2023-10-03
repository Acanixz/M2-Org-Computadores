﻿/*
* Disciplina: Organização de Computadores
* Atividade : Avaliação 02
*
* Grupo:
* - Cassiano de Sena Crispim
* - Hérick Vitor Vieira Bittencourt
* - Eduardo Miguel Fuchs Perez
*
* OBSERVAÇÃO:
* - Devido o uso de for loops com lógica [key, value], o padrão ISO para
* este projeto deve ser o C++ 17, caso contrário a build irá falhar
* (vá na aba projeto->propriedades de main no visual studio 2019)
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include "calculos.h"
using namespace std;


// Armazena informações geradas pelo binary dump
struct LinhaASM {
    // Instrução completa (32 bits)
    string instrucao;

    // Substrings da instrução completa
    string opcode;
    string rd;
    string funct3;
    string rs1;
    string rs2;
    string funct7;

    // Variaveis informativas
    string tipoInstrucao;
    bool isManualNOP = false;
};

// Mostra na tela todas as instrucoes do programa
// (além de poder destacar NOPs se preferido)
void VisualizarInstrucoes(vector<LinhaASM> programa, bool destacarNOPs = true) {
    cout << "----------------------" << endl;
    cout << "VISUALIZANDO PROGRAMA:" << endl;

    for (int i = 0; i < programa.size(); i++) {
        cout << (i + 1) << ": " << programa[i].instrucao;
        if (destacarNOPs) {
            if (programa[i].isManualNOP) {
                cout << " | NOP |";
            }
        }
        cout << endl;
    }

    cout << "----------------------" << endl;
}

void salvarPrograma(vector<LinhaASM> programa, string nomePrograma) {
    ofstream arquivoFinal(nomePrograma);

    // Verificação de abertura do arquivo
    if (!arquivoFinal.is_open()) {
        throw std::runtime_error("Nao foi possivel abrir o arquivo " + nomePrograma + " para escrita");
        return;
    }

    // Transferencia das instruções do vetor para o arquivo
    for (LinhaASM linha : programa) {
        arquivoFinal << linha.instrucao << endl;
    }

    cout << "Instrucoes salvas com exito em " << nomePrograma << endl;
    arquivoFinal.close();
}

// Uma organização, utilizado para gerar estatisticas com um vetor de LinhaASM
struct Organizacao {
    float TClock; // Tempo de clock
    float freqClock; // Frequencia de clock (1/TClock)
    map<string, float> quantCiclos; // Quantos ciclos leva cada instrução?


    Organizacao() {
        quantCiclos["U"] = 1.f;
        quantCiclos["J"] = 1.f;
        quantCiclos["B"] = 1.f;
        quantCiclos["I_ar"] = 1.f; // Quant. Ciclos p/ instrução Imm. Aritmetico e ecall
        quantCiclos["I_lo"] = 1.f; // Quant. Ciclos p/ instrução Imm. Load
        quantCiclos["R"] = 1.f;
        quantCiclos["S"] = 1.f;
    }
};

// Resultados de desempenho da organização com um binary dump
struct Resultados {
    float CiclosTotais; // Ciclos totais gastos
    float CPI; // Ciclos por instrução
    float TExec; // Tempo de execução da CPU (Quant. instrucoes * CPI * Clock)
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

bool verificarHazardInstrucao(LinhaASM instrucaoOrigem, LinhaASM instrucaoJ, bool forwardingImplementado) {

    // A instrução tipo S não faz nenhuma escrita, logo, as proximas linhas não irão ter problemas de dependencia
    if (instrucaoOrigem.tipoInstrucao == "S") return false;

    // NO-Operators, ignora
    if (instrucaoOrigem.isManualNOP) return false;
    if (instrucaoJ.isManualNOP) return false;

    // Ignora ecalls
    if (instrucaoJ.instrucao == "00000000000000000000000001110011") return false;

    // Se forwardingImplementado, apenas procura por hazards cujo a instrução origem é lw
    if (forwardingImplementado && instrucaoOrigem.tipoInstrucao != "I_lo") return false;

    if (instrucaoJ.tipoInstrucao == "R" || instrucaoJ.tipoInstrucao == "I_ar" || instrucaoJ.tipoInstrucao == "I_lo" || instrucaoJ.tipoInstrucao == "S" || instrucaoJ.tipoInstrucao == "B") {
        if (instrucaoOrigem.rd == instrucaoJ.rs1) {
            cout << "RD de origem conflita com rs1!" << endl;
            return true;
        }
    }

    if (instrucaoJ.tipoInstrucao == "R" || instrucaoJ.tipoInstrucao == "S" || instrucaoJ.tipoInstrucao == "B") {
        if (instrucaoOrigem.rd == instrucaoJ.rs2) {
            cout << "RD de origem conflita com rs2!" << endl;
            return true;
        }
    }
    return false;
}

// Insere um NOP a frente de cada jump, só pra ter certeza
vector<LinhaASM> inserirNOPsEmJump(vector<LinhaASM> instrucoes) {
    LinhaASM noOperator; // add zero, zero, zero
    noOperator.instrucao = "00000000000000000000000000110011";
    noOperator.isManualNOP = true;

    for (int i = instrucoes.size()-1; i >= 0; i--) {
        if (instrucoes[i].tipoInstrucao == "J") {
            instrucoes.insert(instrucoes.begin() + i, noOperator);
        }
    }
    return instrucoes;
}

// Solução 1
vector<LinhaASM> inserirNOPs(vector<LinhaASM> instrucoes, vector<int> hazards, bool forwardingImplementado) {
    // Certo, nos temos um array contendo as instruções I (origens cujo possuem no minimo 1 hazard nas proximas 2 linhas)
    // Se formos de baixo pra cima, o array será mais fácil de mexer
    // Ao verificar as instruções, precisamos ir de cima pra baixo a partir de I, começando em I+1 até ser maior que I+2
    // Se o hazard for na primeira linha, será necessário adicionar dois NOPs
    // Se o hazard for na segunda linha, será necessário adicionar um NOP

    LinhaASM noOperator; // add zero, zero, zero
    noOperator.instrucao = "00000000000000000000000000110011";

    noOperator.opcode = noOperator.instrucao.substr(25, 7);
    noOperator.rd = noOperator.instrucao.substr(20, 5);
    noOperator.funct3 = noOperator.instrucao.substr(17, 3);
    noOperator.rs1 = noOperator.instrucao.substr(12, 5);
    noOperator.rs2 = noOperator.instrucao.substr(7, 5);
    noOperator.funct7 = noOperator.instrucao.substr(0, 7);
    noOperator.tipoInstrucao = lerOpcode(noOperator.opcode);
    noOperator.isManualNOP = true;

    for (int i = hazards.size() - 1; i >= 0; i--) {
        // Quantidade de NOPs a serem adicionados, 2 se não tiver forwarding, 1 se tiver
        int quantNOPs = forwardingImplementado ? 1 : 2;

        for (int j = hazards[i] + 1; j <= hazards[i] + 2; j++) {
            // Ignora iteração j caso passe da quantidade de instruções
            if (j > instrucoes.size() - 1) continue;

            if (verificarHazardInstrucao(instrucoes[hazards[i]], instrucoes[j], forwardingImplementado)) {
                for (int k = 0; k < quantNOPs; k++) {
                    instrucoes.insert(instrucoes.begin() + hazards[i] + 1, noOperator);
                }
            }
            quantNOPs--;
        }
    }
    cout << "NO OPERATORS INSERIDOS COM SUCESSO" << endl;
    return inserirNOPsEmJump(instrucoes);
}



// Verifica o vetor de instruções assembly por
// hazards de pipeline
// regra base: rd atual não pode ser utilizado
// nos proximos 2 ciclos
// exceto caso haja implementação de fowarding, que ai
// é apenas um ciclo
vector<int> verificarHazards(vector<LinhaASM> instrucoes, bool forwardingImplementado) {
    cout << "Executando verificacao de hazards" << endl;
    vector<int> falhas;
    for (int i = 0; i < instrucoes.size(); i++) {
        // Ignora instrução se o registrador de destino for zero, NOP
        if (instrucoes[i].tipoInstrucao != "S") {
            if (instrucoes[i].rd == "00000") continue;
        }

        // For loop 2 passos a frente de i
        // verificação de dependencias
        for (int j = i+1; j <= i+2; j++) {
            // Ignora iteração j caso passe da quantidade de instruções
            if (j >= instrucoes.size()) continue;

            // Ignora segunda iteração caso forwarding esteja implementado
            if (j == i + 2 && forwardingImplementado) continue;

            if (verificarHazardInstrucao(instrucoes[i], instrucoes[j], forwardingImplementado)) {
                cout << "| Hazard encontrada na linha " << j + 1 << " vindo da linha nao-finalizada " << i + 1 << endl;
                falhas.push_back(i);
            }
        }
    }

    if (falhas.size() == 0) cout << "Nenhum hazard com encontrado" << endl;
    cout << "Verificacao de hazards concluido com exito" << endl;
    return falhas;
}

// Recebe um ifstream para ler e gera um vetor
// cada indice do vetor representa uma instrução de 32 bits
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
                // alguma linha não está certa
                throw std::runtime_error("Nao foi possivel ler a linha " + to_string(i) + ", verifique o arquivo.");
            }
            else {
                // newline final, só ignorar
                continue;
            }
        }

        // obs: a ordem dos numeros são o inverso do site
        // https://jemu.oscc.cc/ADD
        // (site vai da direita pra esquerda, aqui é esq. pra dir.)
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
// entrada do usuário
Organizacao criarOrganizacao(string nome) {
    cout << "----------------------\n ORGANIZACAO " << nome << endl;
    Organizacao resultado;
    cout << "Forneca o tempo de clock da organizacao " << nome << ": ";
    cin >> resultado.TClock;
    resultado.freqClock = (1 / resultado.TClock);
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


void solucao(int tecnica, string nomeFornecido) {
    if (tecnica == 1) {
        ifstream programa;
        abrirArquivo(programa, nomeFornecido);
        vector<LinhaASM> instrucoes = lerArquivo(programa);
        VisualizarInstrucoes(instrucoes);
        vector<int> falhas = verificarHazards(instrucoes, false);
        instrucoes = inserirNOPs(instrucoes, falhas, false);
        VisualizarInstrucoes(instrucoes);
        verificarHazards(instrucoes, false);
        salvarPrograma(instrucoes, "Solucao1_NOP-NoForward.txt");
    }
    else if (tecnica == 2) {
        ifstream programa;
        abrirArquivo(programa, nomeFornecido);
        vector<LinhaASM> instrucoes = lerArquivo(programa);
        VisualizarInstrucoes(instrucoes);
        vector<int> falhas = verificarHazards(instrucoes, true);
        instrucoes = inserirNOPs(instrucoes, falhas, true);
        VisualizarInstrucoes(instrucoes);
        verificarHazards(instrucoes, true);
        salvarPrograma(instrucoes, "Solucao2_NOP-Forward.txt");
    }
    /*
    else if (tecnica = 3) {
        cout << "Tecnica ainda nao implementada!" << endl;
        cin >> tecnica;
    }
    else if (tecnica = 4) {
        cout << "Tecnica ainda nao implementada!" << endl;
        cin >> tecnica;
    }
     */
}

int main() {
    bool finalizado = false;
    bool semOrganizacao = true;
    Organizacao orgA;
    Organizacao orgB;
    while (!finalizado) {
        system("cls");

        // Cria as organizações se elas não existem ou usuario quer refazer
        if (semOrganizacao) {
            cout << "Criando organizacoes: " << endl;
            semOrganizacao = false;
            orgA = criarOrganizacao("A");
        }

        string nomeFornecido = "";
        cout << "Forneca o nome/caminho do arquivo contendo as instrucoes: " << endl;
        cin >> nomeFornecido;
        cout << endl << endl;

        int tecnica = 0;
        cout << "Escolha a tecnica:" << endl;
        cout << "1- Sem solucao em hardware, insercao de NOPs" << endl;
        cout << "2- Forwarding, insercao de NOPs" << endl;
        cout << "3- Sem solucao em hardware, reordenamento, insercao de NOPs" << endl;
        cout << "4- Forwarding. reordenamento, insercao de NOPs" << endl;
        while (tecnica < 1 || tecnica > 4) {
            cout << "Opcao escolhida: ";
            cin >> tecnica;


            if (tecnica < 1 || tecnica > 4){
                cout << "Opcao invalida, tente novamente!" << endl;
            }
            else if (tecnica == 3) {
                cout << "Tecnica ainda nao implementada!" << endl;
            }
            else if (tecnica == 4) {
                cout << "Tecnica ainda nao implementada!" << endl;
            }
        }
           
        solucao(tecnica, nomeFornecido);


        /*
        * Debug das informações gerais
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
