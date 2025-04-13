#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "pico/stdlib.h" // Biblioteca padrão do Pico SDK
#include "hardware/pio.h"  // Biblioteca para usar o PIO (Programmable I/O)
#include "hardware/clocks.h" // Biblioteca para controlar os clocks
#include "hardware/gpio.h" // Biblioteca para controlar os GPIOs (General Purpose I/O)
#include "hardware/adc.h"  // Biblioteca para usar o ADC (Analog-to-Digital Converter)
#include "ws2818b.pio.h" // Biblioteca para controlar os LEDs WS2812B (NeoPixel) usando PIO

// Definições de pinos
#define LED_COUNT 25       // Número de LEDs na matriz
#define LED_PIN 7          // Pino GPIO conectado aos LEDs
#define JOYSTICK_VRX 27    // Pino GPIO conectado à saída VRx do joystick (eixo X)
#define JOYSTICK_VRY 26    // Pino GPIO conectado à saída VRy do joystick (eixo Y)
#define JOYSTICK_SW 22     // Pino GPIO conectado ao botão do joystick
#define BUTTON_COR_1 5     // Pino GPIO conectado ao botão da cor 1
#define BUTTON_COR_2 6     // Pino GPIO conectado ao botão da cor 2
#define BUZZER_ACERTO 10   // Pino GPIO conectado ao buzzer de acerto
#define BUZZER_ERRO 21     // Pino GPIO conectado ao buzzer de erro
#define LED_ERRO 13        // Pino GPIO conectado ao LED de erro
#define LED_ACERTO 11      // Pino GPIO conectado ao LED de acerto

// Definições de cores
#define COR_1_R 32        // Componente vermelha da cor 1
#define COR_1_G 0          // Componente verde da cor 1
#define COR_1_B 0          // Componente azul da cor 1
#define COR_2_R 0          // Componente vermelha da cor 2
#define COR_2_G 32        // Componente verde da cor 2
#define COR_2_B 0          // Componente azul da cor 2
#define COR_OFF_R 0          // Componente vermelha da cor apagada
#define COR_OFF_G 0          // Componente verde da cor apagada
#define COR_OFF_B 0          // Componente azul da cor apagada
#define COR_PROGRESSO_R 0    // Componente vermelha da cor da barra de progresso
#define COR_PROGRESSO_G 0    // Componente verde da cor da barra de progresso
#define COR_PROGRESSO_B 32  // Componente azul da cor da barra de progresso
#define COR_NUMERO_R 0       // Componente vermelha da cor do número
#define COR_NUMERO_G 0       // Componente verde da cor do número
#define COR_NUMERO_B 32       // Componente azul da cor do número

// Definição do tipo enum para as direções
typedef enum {
    CIMA,    // Direção para cima
    BAIXO,   // Direção para baixo
    ESQUERDA, // Direção para a esquerda
    DIREITA,  // Direção para a direita
    CENTRO   // Direção central (neutra)
} Direcao;

// Definição da estrutura para representar um pixel (LED)
struct pixel_t {
    uint8_t G, R, B; // Componentes verde, vermelha e azul do pixel
};
typedef struct pixel_t pixel_t; // Cria um alias para a struct pixel_t
typedef pixel_t npLED_t;       // Cria um alias para representar um LED NeoPixel

// Variáveis globais
npLED_t leds[LED_COUNT]; // Array para armazenar o estado de cada LED
PIO np_pio;               // Instância do PIO que será utilizada
uint sm;                 // State Machine do PIO

// Protótipos das funções
void npInit(uint pin);                                                              // Inicializa os LEDs NeoPixel
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b); // Define a cor de um LED
void npClear();                                                                   // Apaga todos os LEDs
void npWrite();                                                                   // Envia os dados para os LEDs
int getIndex(int x, int y);                                                        // Calcula o índice do LED na matriz
Direcao lerJoystick();                                                              // Lê a direção do joystick
bool lerBotaoCor1();                                                               // Lê o estado do botão da cor 1
bool lerBotaoCor2();                                                               // Lê o estado do botão da cor 2
bool lerBotaoJoystick();                                                           // Lê o estado do botão do joystick
void tocarBuzzerEco(int buzzer_a_pin, int buzzer_b_pin);                           // Toca o buzzer com um efeito de eco
void tocarBuzzerCoro(int buzzer_a_pin, int buzzer_b_pin);                           // Toca o buzzer em coro
void feedback(int buzzer_a_pin, int buzzer_b_pin, int led_pin, int duracao_ms, bool is_acerto); // Fornece feedback visual e sonoro
void mostrarSequencia(const Direcao *sequencia, const bool *cores, int tamanho); // Exibe a sequência de direções e cores
bool verificarSequencia(const Direcao *sequencia_correta, const bool *cores_corretas, int tamanho); // Verifica se a sequência inserida está correta
void desenharSeta(Direcao direcao, bool cor1, bool cor2);                         // Desenha uma seta na matriz de LEDs
void mapearDirecaoNaMatriz(Direcao direcao, int r, int g, int b);                   // Mapeia uma direção para a matriz de LEDs
void mostrarBarraProgresso(int progresso);                                       // Exibe uma barra de progresso na matriz de LEDs
void desenharNumero(int numero, int r, int g, int b);                             // Desenha um número na matriz de LEDs
void desenharCheckmark(int r, int g, int b);                                      // DEsenha um verificado

// Funções de inicialização do PIO e manipulação da matriz
void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program); // Adiciona o programa PIO para controlar os LEDs WS2812B
    np_pio = pio0;                                        // Seleciona o PIO0
    sm = pio_claim_unused_sm(np_pio, false);             // Aloca uma State Machine não utilizada
    if (sm < 0) {                                           // Se não encontrou no PIO0, tenta no PIO1
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true);
    }
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f); // Inicializa a State Machine com o programa e a frequência
    for (uint i = 0; i < LED_COUNT; ++i) {                    // Apaga todos os LEDs
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

// Define a cor de um LED específico
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r; // Define o componente vermelho
    leds[index].G = g; // Define o componente verde
    leds[index].B = b; // Define o componente azul
}

// Apaga todos os LEDs
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i) // Itera sobre todos os LEDs
        npSetLED(i, 0, 0, 0);               // Define a cor como preto (apagado)
}

// Envia os dados para os LEDs
void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {                  // Itera sobre todos os LEDs
        pio_sm_put_blocking(np_pio, sm, leds[i].G);         // Envia o componente verde
        pio_sm_put_blocking(np_pio, sm, leds[i].R);         // Envia o componente vermelho
        pio_sm_put_blocking(np_pio, sm, leds[i].B);         // Envia o componente azul
    }
    sleep_us(100); // Pequena pausa para garantir a transmissão dos dados
}

// Calcula o índice do LED na matriz, considerando o layout em zigue-zague
int getIndex(int x, int y) {
    if (y % 2 == 0) {                  // Se a linha for par
        return 24 - (y * 5 + x);       // Calcula o índice da esquerda para a direita
    } else {                             // Se a linha for ímpar
        return 24 - (y * 5 + (4 - x)); // Calcula o índice da direita para a esquerda
    }
}

// Funções de leitura do joystick e botões
Direcao lerJoystick() {
    const int16_t centro = 2048;   // Valor central do ADC
    const int16_t deadzone = 200; // Deadzone para evitar leituras imprecisas
    int16_t vrx_value = adc_read();  // Lê o valor do ADC do eixo X
    adc_select_input(0);             // Seleciona o pino do eixo X
    int16_t vry_value = adc_read();  // Lê o valor do ADC do eixo Y
    adc_select_input(1);             // Seleciona o pino do eixo Y

    if (vrx_value < (centro - deadzone)) { // Se o valor do eixo X for menor que o centro menos a deadzone
        return ESQUERDA;                     // Retorna ESQUERDA
    } else if (vrx_value > (centro + deadzone)) { // Se o valor do eixo X for maior que o centro mais a deadzone
        return DIREITA;                              // Retorna DIREITA
    } else if (vry_value > (centro + deadzone)) { // Se o valor do eixo Y for maior que o centro mais a deadzone
        return CIMA;                                 // Retorna CIMA
    } else if (vry_value < (centro - deadzone)) { // Se o valor do eixo Y for menor que o centro menos a deadzone
        return BAIXO;                                // Retorna BAIXO
    } else {                                          // Se estiver dentro da deadzone
        return CENTRO;                               // Retorna CENTRO
    }
}

// Lê o estado do botão da cor 1
bool lerBotaoCor1() {
    return !gpio_get(BUTTON_COR_1); // Retorna TRUE se o botão ESTÁ pressionado (nível baixo)
}

// Lê o estado do botão da cor 2
bool lerBotaoCor2() {
    return !gpio_get(BUTTON_COR_2); // Retorna TRUE se o botão ESTÁ pressionado (nível baixo)
}

// Lê o estado do botão do joystick
bool lerBotaoJoystick() {
    return !gpio_get(JOYSTICK_SW); // Retorna TRUE se o botão ESTÁ pressionado (nível baixo)
}

// Funções de feedback de som
void tocarBuzzerEco(int buzzer_a_pin, int buzzer_b_pin) {
    if (buzzer_a_pin != -1 && buzzer_b_pin != -1) { // Se os pinos do buzzer forem válidos
        gpio_put(buzzer_a_pin, 1);                     // Liga o buzzer A
        sleep_ms(100);                                 // Aguarda 100ms
        gpio_put(buzzer_a_pin, 0);                     // Desliga o buzzer A
        sleep_ms(50);                                  // Aguarda 50ms
        gpio_put(buzzer_b_pin, 1);                     // Liga o buzzer B
        sleep_ms(150);                                 // Aguarda 150ms
        gpio_put(buzzer_b_pin, 0);                     // Desliga o buzzer B
    }
}

// Toca o buzzer em coro
void tocarBuzzerCoro(int buzzer_a_pin, int buzzer_b_pin) {
    if (buzzer_a_pin != -1 && buzzer_b_pin != -1) { // Se os pinos do buzzer forem válidos
        gpio_put(buzzer_a_pin, 1);                     // Liga o buzzer A
        gpio_put(buzzer_b_pin, 1);                     // Liga o buzzer B
        sleep_ms(200);                                 // Aguarda 200ms
        gpio_put(buzzer_a_pin, 0);                     // Desliga o buzzer A
        gpio_put(buzzer_b_pin, 0);                     // Desliga o buzzer B
    }
}

// Função de feedback unificada
void feedback(int buzzer_a_pin, int buzzer_b_pin, int led_pin, int duracao_ms, bool is_acerto) {
    if (is_acerto) {                                    // Se acertou
        tocarBuzzerEco(buzzer_a_pin, buzzer_b_pin);   // Toca o buzzer com efeito de eco
    } else {                                             // Se errou
        tocarBuzzerCoro(buzzer_a_pin, buzzer_b_pin);   // Toca o buzzer em coro
    }

    gpio_put(led_pin, 1);    // Liga o LED
    sleep_ms(duracao_ms); // Aguarda o tempo especificado
    gpio_put(led_pin, 0);    // Desliga o LED
}

void desenharCheckmark(int r, int g, int b) {
    npClear();
    npSetLED(getIndex(0, 2), r, g, b);
    npSetLED(getIndex(1, 3), r, g, b);
    npSetLED(getIndex(2, 4), r, g, b);
    npSetLED(getIndex(3, 3), r, g, b);
    npSetLED(getIndex(4, 1), r, g, b);

    npWrite();
}

// Funções de manipulação da matriz
void mapearDirecaoNaMatriz(Direcao direcao, int r, int g, int b) {
    npClear(); // Apaga todos os LEDs
    switch (direcao) {
        case CIMA: // Se a direção for CIMA
            npSetLED(getIndex(2, 0), r, g, b);
            npSetLED(getIndex(1, 1), r, g, b);
            npSetLED(getIndex(3, 1), r, g, b);
            npSetLED(getIndex(2, 1), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(2, 3), r, g, b);
            npSetLED(getIndex(2, 4), r, g, b);
            break;
        case BAIXO: // Se a direção for BAIXO
            npSetLED(getIndex(2, 4), r, g, b);
            npSetLED(getIndex(1, 3), r, g, b);
            npSetLED(getIndex(3, 3), r, g, b);
            npSetLED(getIndex(2, 3), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(2, 1), r, g, b);
            npSetLED(getIndex(2, 0), r, g, b);
            break;
        case ESQUERDA: // Se a direção for ESQUERDA
            npSetLED(getIndex(0, 2), r, g, b);
            npSetLED(getIndex(1, 1), r, g, b);
            npSetLED(getIndex(1, 3), r, g, b);
            npSetLED(getIndex(1, 2), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(3, 2), r, g, b);
            npSetLED(getIndex(4, 2), r, g, b);
            break;
        case DIREITA: // Se a direção for DIREITA
            npSetLED(getIndex(4, 2), r, g, b);
            npSetLED(getIndex(3, 1), r, g, b);
            npSetLED(getIndex(3, 3), r, g, b);
            npSetLED(getIndex(3, 2), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(1, 2), r, g, b);
            npSetLED(getIndex(0, 2), r, g, b);
            break;
        default: // Se a direção for inválida
            npClear(); // Apaga todos os LEDs
            break;
    }
    npWrite(); // Envia os dados para os LEDs
}

// Exibe a sequência de direções e cores
void mostrarSequencia(const Direcao *sequencia, const bool *cores, int tamanho) {
    for (int i = 0; i < tamanho; i++) { // Itera sobre a sequência
        if (cores[i]) {                   // Se a cor for a cor 1
            mapearDirecaoNaMatriz(sequencia[i], COR_1_R, COR_1_G, COR_1_B); // Mapeia a direção com a cor 1
        } else {                                                            // Se a cor for a cor 2
            mapearDirecaoNaMatriz(sequencia[i], COR_2_R, COR_2_G, COR_2_B); // Mapeia a direção com a cor 2
        }
        sleep_ms(500); // Aguarda 500ms
        npClear();     // Apaga todos os LEDs
        npWrite();     // Envia os dados para os LEDs
        sleep_ms(250); // Aguarda 250ms
    }
}

// Exibe uma barra de progresso na matriz de LEDs
void mostrarBarraProgresso(int progresso) {
    npClear();                                                // Apaga todos os LEDs
    int num_leds_acesos = (int)((float)progresso / 100.0 * LED_COUNT); // Calcula o número de LEDs a serem acesos
    for (int i = 0; i < num_leds_acesos; i++) {                          // Itera sobre os LEDs a serem acesos
        npSetLED(i, COR_PROGRESSO_R, COR_PROGRESSO_G, COR_PROGRESSO_B);   // Define a cor do LED
    }
    npWrite(); // Envia os dados para os LEDs
}

// Verifica se a sequência inserida está correta
bool verificarSequencia(const Direcao *sequencia_correta, const bool *cores_corretas, int tamanho) {
    printf("Prepare-se! \n"); // Imprime mensagem de preparação
    sleep_ms(500);              // Aguarda 500ms

    for (int i = 0; i < tamanho; i++) { // Itera sobre a sequência
        Direcao input_direcao = CENTRO;    // Inicializa a direção inserida
        bool input_cor = false;           // Inicializa a cor inserida
        bool botao_pressionado = false;  // Inicializa o estado do botão
        int tempo_limite = 5000;          // Define o tempo limite para a entrada
        int tempo_inicio = to_ms_since_boot(get_absolute_time()); // Marca o tempo de início
        int tempo_decorrido;          // Variável para armazenar o tempo decorrido
        int progresso;                // Variável para armazenar o progresso

        // 1. Ler Direção do Joystick
        printf("Aguardando entrada do joystick para a direção %d...\n", i + 1); // Imprime mensagem
        while (input_direcao == CENTRO && (tempo_decorrido = to_ms_since_boot(get_absolute_time()) - tempo_inicio) < tempo_limite) {
            Direcao direcao_lida = lerJoystick(); // Lê a direção do joystick
            if (direcao_lida != CENTRO) {         // Se a direção for diferente de CENTRO
                input_direcao = direcao_lida;      // Define a direção inserida
            }
            sleep_ms(50); // Aguarda 50ms
            progresso = (int)((float)tempo_decorrido / tempo_limite * 100); // Calcula o progresso
            mostrarBarraProgresso(progresso); // Exibe a barra de progresso
        }

        if (input_direcao == CENTRO) { // Se o tempo limite for atingido sem nenhuma entrada
            printf("Tempo esgotado para a direção!\n"); // Imprime mensagem de erro
            mostrarBarraProgresso(0);                  // Apaga a barra de progresso
            return false;                              // Retorna falso (errou)
        }
        printf("Direção inserida: %d, Direção correta: %d\n", input_direcao, sequencia_correta[i]); // Imprime a direção inserida e a correta

        // 2. Ler Cor (Botão)
        printf("Aguardando entrada do botão para a cor %d...\n", i + 1); // Imprime mensagem
        tempo_inicio = to_ms_since_boot(get_absolute_time());            // Marca o tempo de início
        while (!botao_pressionado && (tempo_decorrido = to_ms_since_boot(get_absolute_time()) - tempo_inicio) < tempo_limite) {
            if (lerBotaoCor1()) {           // Se o botão da cor 1 for pressionado
                input_cor = true;            // Define a cor inserida como TRUE
                botao_pressionado = true;  // Define o estado do botão como TRUE
            } else if (lerBotaoCor2()) {    // Se o botão da cor 2 for pressionado
                input_cor = false;           // Define a cor inserida como FALSE
                botao_pressionado = true;  // Define o estado do botão como TRUE
            }
            sleep_ms(50); // Aguarda 50ms
            progresso = (int)((float)tempo_decorrido / tempo_limite * 100); // Calcula o progresso
            mostrarBarraProgresso(progresso); // Exibe a barra de progresso
        }

        if (!botao_pressionado) {           // Se o tempo limite for atingido sem nenhuma entrada
            printf("Tempo esgotado para a cor!\n"); // Imprime mensagem de erro
            mostrarBarraProgresso(0);                  // Apaga a barra de progresso
            return false;                              // Retorna falso (errou)
        }

        printf("Cor inserida: %s, Cor correta: %s\n", input_cor ? "Vermelho" : "Verde", cores_corretas[i] ? "Vermelho" : "Verde"); // Imprime a cor inserida e a correta

        if (input_direcao != sequencia_correta[i]) { // Se a direção inserida for diferente da correta
            printf("Direcao incorreta! Esperado: %d, Recebido: %d\n", sequencia_correta[i], input_direcao); // Imprime mensagem de erro
            mostrarBarraProgresso(0);                  // Apaga a barra de progresso
            return false;                              // Retorna falso (errou)
        }

        if (input_cor != cores_corretas[i]) { // Se a cor inserida for diferente da correta
            printf("Cor incorreta! Esperado: %s, Recebido: %s\n", cores_corretas[i] ? "Vermelho" : "Verde", input_cor ? "Vermelho" : "Verde"); // Imprime mensagem de erro
            mostrarBarraProgresso(0);                  // Apaga a barra de progresso
            return false;                              // Retorna falso (errou)
        }
        mostrarBarraProgresso(0); // Apaga a barra de progresso
    }

    return true; // Retorna verdadeiro (acertou)
}

// Desenha um número na matriz de LEDs
void desenharNumero(int numero, int r, int g, int b) {
    npClear(); // Apaga todos os LEDs
    switch (numero) {
        case 0: // Se o número for 0
            npSetLED(getIndex(1, 0), r, g, b);
            npSetLED(getIndex(2, 0), r, g, b);
            npSetLED(getIndex(3, 0), r, g, b);
            npSetLED(getIndex(0, 1), r, g, b);
            npSetLED(getIndex(4, 1), r, g, b);
            npSetLED(getIndex(0, 2), r, g, b);
            npSetLED(getIndex(4, 2), r, g, b);
            npSetLED(getIndex(0, 3), r, g, b);
            npSetLED(getIndex(4, 3), r, g, b);
            npSetLED(getIndex(1, 4), r, g, b);
            npSetLED(getIndex(2, 4), r, g, b);
            npSetLED(getIndex(3, 4), r, g, b);
            break;
        case 1: // Se o número for 1
            npSetLED(getIndex(2, 0), r, g, b);
            npSetLED(getIndex(1, 1), r, g, b);
            npSetLED(getIndex(2, 1), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(2, 3), r, g, b);
            npSetLED(getIndex(2, 4), r, g, b);
            break;
        case 2: // Se o número for 2
            npSetLED(getIndex(1, 0), r, g, b);
            npSetLED(getIndex(2, 0), r, g, b);
            npSetLED(getIndex(3, 0), r, g, b);
            npSetLED(getIndex(4, 1), r, g, b);
            npSetLED(getIndex(3, 2), r, g, b);
            npSetLED(getIndex(2, 3), r, g, b);
            npSetLED(getIndex(1, 4), r, g, b);
            npSetLED(getIndex(0, 4), r, g, b);
            npSetLED(getIndex(1, 4), r, g, b);
            npSetLED(getIndex(2, 4), r, g, b);
            npSetLED(getIndex(3, 4), r, g, b);
            break;
        case 3: // Se o número for 3
            npSetLED(getIndex(1, 0), r, g, b);
            npSetLED(getIndex(2, 0), r, g, b);
            npSetLED(getIndex(3, 0), r, g, b);
            npSetLED(getIndex(4, 1), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(4, 3), r, g, b);
            npSetLED(getIndex(1, 4), r, g, b);
            npSetLED(getIndex(2, 4), r, g, b);
            npSetLED(getIndex(3, 4), r, g, b);
            break;
        case 4: // Se o número for 4
            npSetLED(getIndex(0, 0), r, g, b);
            npSetLED(getIndex(4, 0), r, g, b);
            npSetLED(getIndex(0, 1), r, g, b);
            npSetLED(getIndex(4, 1), r, g, b);
            npSetLED(getIndex(0, 2), r, g, b);
            npSetLED(getIndex(1, 2), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(3, 2), r, g, b);
            npSetLED(getIndex(4, 2), r, g, b);
            npSetLED(getIndex(4, 3), r, g, b);
            npSetLED(getIndex(4, 4), r, g, b);
            break;
        case 5: // Se o número for 5
            npSetLED(getIndex(0, 0), r, g, b);
            npSetLED(getIndex(1, 0), r, g, b);
            npSetLED(getIndex(2, 0), r, g, b);
            npSetLED(getIndex(3, 0), r, g, b);
            npSetLED(getIndex(0, 1), r, g, b);
            npSetLED(getIndex(0, 2), r, g, b);
            npSetLED(getIndex(1, 2), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(3, 3), r, g, b);
            npSetLED(getIndex(4, 4), r, g, b);
            npSetLED(getIndex(3, 4), r, g, b);
            npSetLED(getIndex(2, 4), r, g, b);
            break;
        case 6: // Se o número for 6
            npSetLED(getIndex(1, 0), r, g, b);
            npSetLED(getIndex(2, 0), r, g, b);
            npSetLED(getIndex(3, 0), r, g, b);
            npSetLED(getIndex(0, 1), r, g, b);
            npSetLED(getIndex(0, 2), r, g, b);
            npSetLED(getIndex(1, 2), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
             npSetLED(getIndex(0, 3), r, g, b);
            npSetLED(getIndex(0, 4), r, g, b);
            npSetLED(getIndex(1, 4), r, g, b);
            npSetLED(getIndex(2, 4), r, g, b);
            npSetLED(getIndex(3, 4), r, g, b);
            break;
        case 7: // Se o número for 7
            npSetLED(getIndex(0, 0), r, g, b);
            npSetLED(getIndex(1, 0), r, g, b);
            npSetLED(getIndex(2, 0), r, g, b);
            npSetLED(getIndex(3, 0), r, g, b);
            npSetLED(getIndex(4, 0), r, g, b);
            npSetLED(getIndex(4, 1), r, g, b);
            npSetLED(getIndex(3, 2), r, g, b);
            npSetLED(getIndex(2, 3), r, g, b);
            npSetLED(getIndex(1, 4), r, g, b);
            break;
        case 8: // Se o número for 8
            npSetLED(getIndex(1, 0), r, g, b);
            npSetLED(getIndex(2, 0), r, g, b);
            npSetLED(getIndex(3, 0), r, g, b);
            npSetLED(getIndex(0, 1), r, g, b);
             npSetLED(getIndex(4, 1), r, g, b);
            npSetLED(getIndex(0, 2), r, g, b);
            npSetLED(getIndex(1, 2), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(3, 2), r, g, b);
            npSetLED(getIndex(4, 2), r, g, b);
            npSetLED(getIndex(0, 3), r, g, b);
            npSetLED(getIndex(4, 3), r, g, b);
            npSetLED(getIndex(1, 4), r, g, b);
            npSetLED(getIndex(2, 4), r, g, b);
            npSetLED(getIndex(3, 4), r, g, b);
            break;
        case 9: // Se o número for 9
             npSetLED(getIndex(1, 0), r, g, b);
            npSetLED(getIndex(2, 0), r, g, b);
            npSetLED(getIndex(3, 0), r, g, b);
            npSetLED(getIndex(0, 1), r, g, b);
             npSetLED(getIndex(4, 1), r, g, b);
            npSetLED(getIndex(0, 2), r, g, b);
            npSetLED(getIndex(1, 2), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(3, 2), r, g, b);
            npSetLED(getIndex(4, 2), r, g, b);
            npSetLED(getIndex(4, 3), r, g, b);
            npSetLED(getIndex(1, 4), r, g, b);
            npSetLED(getIndex(2, 4), r, g, b);
            npSetLED(getIndex(3, 4), r, g, b);
            break;
        default: // Se o número for inválido
            npClear(); // Apaga todos os LEDs
            break;
    }
    npWrite(); // Envia os dados para os LEDs
}

int main() {
    stdio_init_all(); // Inicializa a E/S padrão (stdio)

    // Inicializa os pinos do joystick
    gpio_init(JOYSTICK_VRX);
    gpio_init(JOYSTICK_VRY);
    gpio_init(JOYSTICK_SW);

    // Inicializa os pinos dos botões de cor
    gpio_init(BUTTON_COR_1);
    gpio_init(BUTTON_COR_2);

    // Inicializa os pinos dos buzzers e LEDs de feedback

    gpio_init(BUZZER_ACERTO);
    gpio_init(BUZZER_ERRO);
    gpio_init(LED_ERRO);
    gpio_init(LED_ACERTO);

    // Define a direção dos pinos (ENTRADA ou SAÍDA)
    gpio_set_dir(JOYSTICK_VRX, GPIO_IN);
    gpio_set_dir(JOYSTICK_VRY, GPIO_IN);
    gpio_set_dir(JOYSTICK_SW, GPIO_IN);
    gpio_set_dir(BUTTON_COR_1, GPIO_IN);
    gpio_set_dir(BUTTON_COR_2, GPIO_IN);
    gpio_set_dir(BUZZER_ACERTO, GPIO_OUT);
    gpio_set_dir(BUZZER_ERRO, GPIO_OUT);
    gpio_set_dir(LED_ERRO, GPIO_OUT);
    gpio_set_dir(LED_ACERTO, GPIO_OUT);

    // Ativa o resistor pull-up interno para o botão do joystick
    gpio_pull_up(JOYSTICK_SW);

    // Ativa o resistor pull-up interno para os botões de cor
    gpio_pull_up(BUTTON_COR_1);
    gpio_pull_up(BUTTON_COR_2);

    // Inicializa o ADC (Analog-to-Digital Converter)
    adc_init();
    adc_gpio_init(JOYSTICK_VRY); // Inicializa o pino do joystick Y para leitura analógica
    adc_gpio_init(JOYSTICK_VRX); // Inicializa o pino do joystick X para leitura analógica
    adc_select_input(1);         // Seleciona o pino do joystick Y como entrada do ADC

    // Inicializa os LEDs NeoPixel
    npInit(LED_PIN);
    npClear(); // Apaga todos os LEDs
    npWrite(); // Envia os dados (apagados) para os LEDs

    // Inicializa o gerador de números aleatórios com o tempo atual
    srand(time(NULL));

    // Variáveis do jogo
    int nivel = 1;                 // Nível inicial do jogo
    const int MAX_SEQUENCIA = 9;   // Nível máximo do jogo
    bool paused = false;             // Indica se o jogo está pausado

    // Loop principal do jogo
    while (true) {
        // Verifica se o botão do joystick foi pressionado
        if (lerBotaoJoystick()) {
            paused = !paused; // Inverte o estado da pausa
            if (paused) {      // Se o jogo foi pausado
                printf("Jogo pausado!\n"); // Imprime mensagem
            } else {                      // Se o jogo foi retomado
                printf("Jogo retomado!\n"); // Imprime mensagem
                sleep_ms(1000);            // Aguarda 1 segundo
            }
            sleep_ms(200); // Pequena pausa para evitar leituras múltiplas do botão
        }

        // Se o jogo não está pausado
        if (!paused) {
            printf("Nível: %d\n", nivel);                                          // Imprime o nível atual
            desenharNumero(nivel, COR_NUMERO_R, COR_NUMERO_G, COR_NUMERO_B); // Exibe o número do nível na matriz de LEDs
            sleep_ms(2000);                                                       // Aguarda 2 segundos

            // Cria a sequência de direções e cores
            Direcao sequencia[MAX_SEQUENCIA];
            bool cores[MAX_SEQUENCIA];
            for (int i = 0; i < nivel; i++) {
                sequencia[i] = (Direcao)(rand() % 4); // Gera uma direção aleatória
                cores[i] = rand() % 2;                  // Gera uma cor aleatória (0 ou 1)
            }

            mostrarSequencia(sequencia, cores, nivel); // Exibe a sequência

            printf("Sua vez!\n"); // Imprime mensagem
            bool acertou = verificarSequencia(sequencia, cores, nivel); // Verifica se a sequência inserida está correta

            // Se o jogador acertou
            if (acertou) {
                printf("Parabéns! Próximo nível.\n");        // Imprime mensagem
                feedback(BUZZER_ACERTO, BUZZER_ERRO, LED_ACERTO, 200, true); // Fornece feedback de acerto
                nivel++;                                         // Aumenta o nível
                printf("Prepare-se\n");                            // Imprime mensagem
                sleep_ms(2000);                                      // Aguarda 2 segundos

                // Se o jogador venceu o jogo (atingiu o nível máximo)
                if (nivel > MAX_SEQUENCIA) {
                    printf("Você venceu o jogo!\n"); // Imprime mensagem de vitória
                    desenharCheckmark(0, 32 , 0); // Desenha o sinal de verificado verde
                    sleep_ms(5000); // Mostra por 5 segundos
                    npClear(); // Apaga a matriz
                    npWrite();
                    nivel = 1;
                }
            } else { // Se o jogador errou
                printf("Errou! Game Over.\n");                                 // Imprime mensagem de Game Over
                feedback(BUZZER_ACERTO, BUZZER_ERRO, LED_ERRO, 500, false); // Fornece feedback de erro
                sleep_ms(2000);                                               // Aguarda 2 segundos
                nivel = 1;                                                    // Reinicia o nível
            }
        } else {          // Se o jogo está pausado
            sleep_ms(100); // Aguarda 100ms
        }
    }

    return 0; // Retorna 0 (fim do programa)
}