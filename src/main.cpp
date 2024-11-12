#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>

// Global variables for synchronization
constexpr int NUM_JOGADORES = 4;
std::counting_semaphore<NUM_JOGADORES> cadeira_sem(NUM_JOGADORES - 1); // Inicia com n-1 cadeiras, capacidade máxima n
std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true};
std::counting_semaphore<1> next_round_sem(0); //Semáforo para que o jogo indique ao coordenador que pode começar a música novamente
std::mutex chair_fill;

/*
 * Uso básico de um counting_semaphore em C++:
 * 
 * O `std::counting_semaphore` é um mecanismo de sincronização que permite controlar o acesso a um recurso compartilhado 
 * com um número máximo de acessos simultâneos. Neste projeto, ele é usado para gerenciar o número de cadeiras disponíveis.
 * Inicializamos o semáforo com `n - 1` para representar as cadeiras disponíveis no início do jogo. 
 * Cada jogador que tenta se sentar precisa fazer um `acquire()`, e o semáforo permite que até `n - 1` jogadores 
 * ocupem as cadeiras. Quando todos os assentos estão ocupados, jogadores adicionais ficam bloqueados até que 
 * o coordenador libere o semáforo com `release()`, sinalizando a eliminação dos jogadores.
 * O método `release()` também pode ser usado para liberar múltiplas permissões de uma só vez, por exemplo: `cadeira_sem.release(3);`,
 * o que permite destravar várias threads de uma só vez, como é feito na função `liberar_threads_eliminadas()`.
 *
 * Métodos da classe `std::counting_semaphore`:
 * 
 * 1. `acquire()`: Decrementa o contador do semáforo. Bloqueia a thread se o valor for zero.
 *    - Exemplo de uso: `cadeira_sem.acquire();` // Jogador tenta ocupar uma cadeira.
 * 
 * 2. `release(int n = 1)`: Incrementa o contador do semáforo em `n`. Pode liberar múltiplas permissões.
 *    - Exemplo de uso: `cadeira_sem.release(2);` // Libera 2 permissões simultaneamente.
 */

// Classes
class JogoDasCadeiras {
public:
    JogoDasCadeiras(int num_jogadores)
        : num_jogadores(num_jogadores), cadeiras(num_jogadores - 1) {}

    void iniciar_rodada() {
        this->cadeiras = num_jogadores - 1;// TODO: Inicia uma nova rodada, removendo uma cadeira e ressincronizando o semáforo
        cadeira_sem.release(cadeiras);
    }

    void parar_musica() {
        std::cout << "\n------------------------------------------\n A música acabou de parar \n------------------------------------------\n";
        musica_parada = true;
        music_cv.notify_all();// TODO: Simula o momento em que a música para e notifica os jogadores via variável de condição
    }

    void eliminar_jogador(int jogador_id) {
        
        std::cout << "Jogador " << jogador_id << " foi eliminado." << std::endl;
        this->num_jogadores--;// TODO: Elimina um jogador que não conseguiu uma cadeira
        next_round_sem.release(); //Após a eliminação, avisa que a música pode começar novamente
    }

    void exibir_estado() {
        // TODO: Exibe o estado atual das cadeiras e dos jogadores
    }
    int conta_cadeiras_rodada(){
        chair_fill.lock();
        this->cadeiras--;
        chair_fill.unlock();
        return this->cadeiras;
    }
    int get_cadeiras(){
        return this->cadeiras;
    }
    int get_jogadores(){
        return this->num_jogadores;
    }

private:
    int num_jogadores;
    int cadeiras;
};

class Jogador {
public:
    Jogador(int id, JogoDasCadeiras& jogo)
        : id(id), jogo(jogo) {}

    void tentar_ocupar_cadeira() {
        
        cadeira_sem.acquire();
        this->jogo.conta_cadeiras_rodada();
    }

    int verificar_eliminacao() {
        if(this->jogo.get_cadeiras() == -1){
            return 1;
        }
        return 0;
    }
        // TODO: Verifica se foi eliminado após ser destravado do semáforo

    int get_id(){
        return this->id;
    }
    void joga() {

        while(this->jogo.get_jogadores() != 1 && jogo_ativo){
        // TODO: Aguarda a música parar usando a variavel de condicao
        if(!jogo_ativo) break;
        std::unique_lock<std::mutex> lock(music_mutex);
        music_cv.wait(lock);
        if(!jogo_ativo) break;
        // TODO: Tenta ocupar uma cadeira
        tentar_ocupar_cadeira();
        // TODO: Verifica se foi eliminado
            if(this->verificar_eliminacao()){
                this->jogo.eliminar_jogador(this->id);
                break;
                std::cout << "Break não funcionou apesar de eliminado, Thread continua executando\n";
            }
            std::cout << "Jogador " << this->id << " conseguiu se sentar nessa rodada\n";
        }
    }

private:
    int id;
    JogoDasCadeiras& jogo;
};

class Coordenador {
public:
    Coordenador(JogoDasCadeiras& jogo)
        : jogo(jogo) {}

    void iniciar_jogo() {
        while(jogo_ativo) {
        std::cout << "\n------------------------------------------\n A música acabou de começar \n------------------------------------------\n";
        musica_parada = false;
        //Começar jogo
        int sleeptime = 2 + (rand() % 6); // Gera um período aleatório dentre 2 e 7 segundos
        std::this_thread::sleep_for(std::chrono::seconds(sleeptime)); // Espera
        //Sinaliza todos
        this->jogo.parar_musica();
        // TODO: Começa o jogo, dorme por um período aleatório, e então para a música, sinalizando os jogadores
        this->liberar_threads_eliminadas();
        next_round_sem.acquire();
        std::cout << "A quantidade de jogadores restantes para a próxima rodada é:" << this->jogo.get_jogadores() << std::endl; 

        if( this->jogo.get_jogadores()==1){
            jogo_ativo = false;
            music_cv.notify_all();
            break;
        }

        this->jogo.iniciar_rodada();
        }
    }

    void liberar_threads_eliminadas() {
        // Libera múltiplas permissões no semáforo para destravar todas as threads que não conseguiram se sentar
        cadeira_sem.release(1); // Libera o número de permissões igual ao número de jogadores que ficaram esperando - Em tese sempre é so um jogador que fica esperando já que num cadeiras = num jogadores - 1
    }

private:
    JogoDasCadeiras& jogo;
};

// Main function
int main() {
    std::srand(static_cast<unsigned int>(std::time(0))); //Seed random

    std::cout << "-----------------------------------------------\n";
    std::cout << "Bem-vindo ao Jogo das Cadeiras Concorrente!\n";
    std::cout << "-----------------------------------------------\n";
    std::cout << "O jogo iniciou com " << NUM_JOGADORES << " jogadores e " << NUM_JOGADORES-1 <<" cadeiras\n";

    JogoDasCadeiras jogo(NUM_JOGADORES);
    Coordenador coordenador(jogo);
    std::vector<std::thread> jogadores;

    // Criação das threads dos jogadores
    std::vector<Jogador> jogadores_objs;
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores_objs.emplace_back(i, jogo);
    }

    for (int i = 0; i < NUM_JOGADORES; ++i) {
        jogadores.emplace_back(&Jogador::joga, &jogadores_objs[i]);
    }

    // Thread do coordenador
    std::thread coordenador_thread(&Coordenador::iniciar_jogo, &coordenador);

    // Esperar pelas threads dos jogadores
    for (auto& t : jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Esperar pela thread do coordenador
    if (coordenador_thread.joinable()) {
        coordenador_thread.join();
    }

    std::cout << "Jogo das Cadeiras finalizado." << std::endl;
    return 0;
}

